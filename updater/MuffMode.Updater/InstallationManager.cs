using System.Diagnostics;
using System.IO.Compression;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace MuffMode.Updater;

internal static partial class InstallationManager
{
    private const string SettingsFileName = "updater-settings.json";
    private const string MarkerJsonFileName = "muffmode-version.json";
    private const string MarkerTextFileName = "muffmode.version";
    private const int MaxPackageEntryCount = 40_000;
    private const int MaxPackageFileCount = 20_000;
    private const long MaxPackageUncompressedBytes = 1024L * 1024L * 1024L;
    private const int CopyBufferSize = 128 * 1024;
    private const long MaxSettingsBytes = 64L * 1024L;
    private const long MaxVersionMarkerBytes = 64L * 1024L;
    private const long MaxDiscoveryFileBytes = 1024L * 1024L;
    private static readonly TimeSpan TemporaryExtractionMaxAge = TimeSpan.FromDays(2);

    private static readonly HashSet<string> AllowedRootPackageFiles = new(StringComparer.OrdinalIgnoreCase)
    {
        "CHANGELOG.md",
        "MuffMode.version",
        "MuffModeUpdater.exe",
        "README.html",
        "README.md",
        "VERSION"
    };

    private static readonly HashSet<string> ReservedWindowsFileNames = new(StringComparer.OrdinalIgnoreCase)
    {
        "CON",
        "PRN",
        "AUX",
        "NUL",
        "COM1",
        "COM2",
        "COM3",
        "COM4",
        "COM5",
        "COM6",
        "COM7",
        "COM8",
        "COM9",
        "LPT1",
        "LPT2",
        "LPT3",
        "LPT4",
        "LPT5",
        "LPT6",
        "LPT7",
        "LPT8",
        "LPT9"
    };

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true
    };

    public static string SettingsDirectory =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "MuffMode");

    public static string SettingsPath => Path.Combine(SettingsDirectory, SettingsFileName);

    public static AppSettings LoadSettings()
    {
        try
        {
            if (TryReadSmallTextFile(SettingsPath, MaxSettingsBytes, out var json))
            {
                return JsonSerializer.Deserialize<AppSettings>(json, JsonOptions) ?? new AppSettings();
            }
        }
        catch
        {
            // A damaged settings file should not keep the updater from opening.
        }

        return new AppSettings();
    }

    public static void SaveSettings(AppSettings settings)
    {
        ValidateSettingsPath();
        Directory.CreateDirectory(SettingsDirectory);
        ValidateSettingsPath();
        WriteAllTextAtomic(SettingsPath, JsonSerializer.Serialize(settings, JsonOptions));
    }

    public static string? ResolveInitialInstallPath(string? savedPath)
    {
        return GetInstallCandidates(savedPath).FirstOrDefault()?.Path;
    }

    public static IReadOnlyList<InstallCandidate> GetInstallCandidates(string? savedPath)
    {
        var candidates = new List<InstallCandidate>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        AddInstallCandidate(candidates, seen, "Saved location", savedPath);
        foreach (var steamPath in EnumerateSteamInstallCandidates())
        {
            AddInstallCandidate(candidates, seen, "Steam", steamPath);
        }

        foreach (var epicPath in EnumerateEpicInstallCandidates())
        {
            AddInstallCandidate(candidates, seen, "Epic Online Store", epicPath);
        }

        foreach (var gogPath in EnumerateGogInstallCandidates())
        {
            AddInstallCandidate(candidates, seen, "GOG", gogPath);
        }

        return candidates;
    }

    public static string? ResolveInstallRoot(string? installPath)
    {
        if (string.IsNullOrWhiteSpace(installPath))
        {
            return null;
        }

        if (!TryGetNormalizedFullPath(NormalizePath(installPath), out var normalized))
        {
            return null;
        }

        if (Directory.Exists(normalized) && Directory.Exists(Path.Combine(normalized, "rerelease", "baseq2")))
        {
            return normalized;
        }

        if (Directory.Exists(normalized) && Directory.Exists(Path.Combine(normalized, "baseq2")))
        {
            var folderName = Path.GetFileName(normalized.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            if (string.Equals(folderName, "rerelease", StringComparison.OrdinalIgnoreCase))
            {
                return Directory.GetParent(normalized)?.FullName ?? normalized;
            }
        }

        if (Directory.Exists(normalized))
        {
            var folderName = Path.GetFileName(normalized.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            var parent = Directory.GetParent(normalized);
            if (string.Equals(folderName, "baseq2", StringComparison.OrdinalIgnoreCase)
                && parent is not null
                && string.Equals(parent.Name, "rerelease", StringComparison.OrdinalIgnoreCase))
            {
                return parent.Parent?.FullName;
            }
        }

        return null;
    }

    public static bool IsValidInstallPath(string? installPath)
    {
        var normalized = ResolveInstallRoot(installPath);
        return normalized is not null && !IsReparsePoint(normalized);
    }

    public static LocalInstallVersion ReadLocalVersion(string? installPath)
    {
        var normalized = ResolveInstallRoot(installPath);
        if (normalized is null)
        {
            return new LocalInstallVersion(null, "Choose a valid Quake 2 install folder", "No valid install path");
        }

        if (IsReparsePoint(normalized))
        {
            return new LocalInstallVersion(null, "Choose a valid Quake 2 install folder", "Install folder is a reparse point");
        }

        var baseq2 = GetBaseq2Path(normalized);

        var jsonMarker = Path.Combine(baseq2, MarkerJsonFileName);
        if (TryReadVersionMarker(jsonMarker, out var markerVersion))
        {
            return new LocalInstallVersion(markerVersion, markerVersion.ToString(), MarkerJsonFileName);
        }

        foreach (var textMarker in new[]
        {
            Path.Combine(baseq2, MarkerTextFileName),
            Path.Combine(normalized, "MuffMode.version"),
            Path.Combine(normalized, "VERSION")
        })
        {
            if (TryReadVersionTextFile(textMarker, out var textVersion))
            {
                return new LocalInstallVersion(textVersion, textVersion.ToString(), Path.GetFileName(textMarker));
            }
        }

        var dllPath = Path.Combine(baseq2, "game_x64.dll");
        if (TryReadDllFileVersion(dllPath, out var dllVersion))
        {
            return new LocalInstallVersion(dllVersion, dllVersion.ToString(), "game_x64.dll file version");
        }

        var changelog = Path.Combine(normalized, "CHANGELOG.md");
        if (TryReadVersionTextFile(changelog, out var changelogVersion))
        {
            return new LocalInstallVersion(changelogVersion, changelogVersion.ToString(), "CHANGELOG.md");
        }

        return new LocalInstallVersion(null, "Unknown / not installed by updater", "No MuffMode version marker found");
    }

    public static async Task SyncReleaseToInstallAsync(
        ReleaseInfo release,
        string zipPath,
        string installPath,
        IProgress<UpdaterProgress>? progress,
        CancellationToken cancellationToken)
    {
        var normalizedInstallPath = ResolveInstallRoot(installPath);
        if (normalizedInstallPath is null)
        {
            throw new InvalidOperationException("Select the Quake 2 installation folder, its rerelease folder, or its baseq2 folder.");
        }

        EnsureSafeInstallRoot(normalizedInstallPath);
        ValidateDownloadedArchive(zipPath);
        var updaterTempRoot = Path.Combine(Path.GetTempPath(), "MuffModeUpdater");
        CleanupOldExtractionDirectories(updaterTempRoot);
        var extractRoot = Path.Combine(updaterTempRoot, Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(extractRoot);

        try
        {
            progress?.Report(new UpdaterProgress("Extracting release package...", 46));
            await ExtractReleasePackageAsync(zipPath, extractRoot, cancellationToken);

            var packageRoot = ResolvePackageRoot(extractRoot);
            EnsureSafePackageRoot(extractRoot, packageRoot);
            ValidateReleasePackage(packageRoot, release);

            var releaseFiles = EnumeratePackageFiles(packageRoot)
                .Order(StringComparer.OrdinalIgnoreCase)
                .ToList();
            if (releaseFiles.Count == 0)
            {
                throw new InvalidOperationException("The downloaded release package did not contain any files to install.");
            }

            var runningUpdaterPath = GetRunningUpdaterPath();
            cancellationToken.ThrowIfCancellationRequested();
            progress?.Report(new UpdaterProgress("Applying release package...", 49, CanCancel: false));
            BackupCurrentGameDll(normalizedInstallPath, progress);

            for (var index = 0; index < releaseFiles.Count; index++)
            {
                var sourcePath = releaseFiles[index];
                var relativePath = Path.GetRelativePath(packageRoot, sourcePath);
                var destinationPath = ResolveDestinationPath(normalizedInstallPath, relativePath);
                if (IsRunningUpdaterDestination(destinationPath, runningUpdaterPath))
                {
                    progress?.Report(new UpdaterProgress($"Skipping running updater executable: {relativePath}.", null, CanCancel: false));
                    continue;
                }

                EnsureSafeInstallWritePath(
                    normalizedInstallPath,
                    destinationPath,
                    $"Package entry would write through an unsafe install path: {relativePath}");
                CopyFileAtomically(sourcePath, destinationPath);

                var percentage = 50 + (int)Math.Round((double)(index + 1) / releaseFiles.Count * 45);
                progress?.Report(new UpdaterProgress($"Syncing {relativePath}...", Math.Clamp(percentage, 50, 95), CanCancel: false));
            }

            WriteInstalledMarker(normalizedInstallPath, release);
            progress?.Report(new UpdaterProgress($"MuffMode {release.Version} installed.", 100, CanCancel: false));
        }
        finally
        {
            TryDeleteDirectory(extractRoot);
        }
    }

    public static void LaunchGame(string installPath)
    {
        var normalized = ResolveInstallRoot(installPath);
        if (normalized is null)
        {
            throw new InvalidOperationException("Select a valid Quake 2 installation folder before launching.");
        }

        EnsureSafeLaunchRoot(normalized);
        foreach (var candidate in EnumerateLaunchCandidates(normalized))
        {
            if (IsSafeLaunchCandidate(normalized, candidate) && IsLaunchableFile(candidate))
            {
                StartProcessOrThrow(
                    new ProcessStartInfo(candidate)
                    {
                        WorkingDirectory = Path.GetDirectoryName(candidate) ?? normalized,
                        UseShellExecute = true
                    },
                    Path.GetFileName(candidate),
                    requireProcessHandle: true);
                return;
            }
        }

        StartProcessOrThrow(
            new ProcessStartInfo("steam://rungameid/2320")
            {
                UseShellExecute = true
            },
            "Steam Quake II launch URL",
            requireProcessHandle: false);
    }

    private static void StartProcessOrThrow(ProcessStartInfo startInfo, string displayName, bool requireProcessHandle)
    {
        try
        {
            var process = Process.Start(startInfo);
            if (requireProcessHandle && process is null)
            {
                throw new InvalidOperationException($"Could not launch {displayName}.");
            }
        }
        catch (Exception ex) when (ex is not InvalidOperationException)
        {
            throw new InvalidOperationException($"Could not launch {displayName}.", ex);
        }
    }

    private static string NormalizePath(string? path) => (path ?? "").Trim().Trim('"');

    private static string GetBaseq2Path(string installPath) => Path.Combine(installPath, "rerelease", "baseq2");

    private static void ValidateSettingsPath()
    {
        if (Directory.Exists(SettingsDirectory) && IsReparsePoint(SettingsDirectory))
        {
            throw new IOException("The MuffMode settings folder is a reparse point.");
        }

        if (Directory.Exists(SettingsPath))
        {
            throw new IOException("The MuffMode settings path points at a directory.");
        }

        if (File.Exists(SettingsPath) && IsReparsePoint(SettingsPath))
        {
            throw new IOException("The MuffMode settings file is a reparse point.");
        }
    }

    private static void ValidateDownloadedArchive(string zipPath)
    {
        if (string.IsNullOrWhiteSpace(zipPath))
        {
            throw new InvalidOperationException("The downloaded release package path was empty.");
        }

        string fullPath;
        try
        {
            fullPath = Path.GetFullPath(zipPath);
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
        {
            throw new InvalidOperationException("The downloaded release package path was invalid.", ex);
        }

        if (!File.Exists(fullPath))
        {
            throw new FileNotFoundException("The downloaded release package could not be found.", fullPath);
        }

        if (!string.Equals(Path.GetExtension(fullPath), ".zip", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The downloaded release package must be a .zip file.");
        }

        var attributes = File.GetAttributes(fullPath);
        if ((attributes & FileAttributes.Directory) != 0)
        {
            throw new InvalidOperationException("The downloaded release package path points at a directory.");
        }

        if ((attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new InvalidOperationException("The downloaded release package path points at a reparse point.");
        }

        var length = new FileInfo(fullPath).Length;
        if (length == 0)
        {
            throw new InvalidOperationException("The downloaded release package was empty.");
        }

        if (length > MaxPackageUncompressedBytes)
        {
            throw new InvalidOperationException($"The downloaded release package is larger than the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
        }
    }

    private static bool TryReadSmallTextFile(string path, long maxBytes, out string text)
    {
        text = "";
        try
        {
            if (!File.Exists(path))
            {
                return false;
            }

            var attributes = File.GetAttributes(path);
            if ((attributes & (FileAttributes.Directory | FileAttributes.ReparsePoint)) != 0)
            {
                return false;
            }

            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using var buffer = new MemoryStream();
            var chunk = new byte[CopyBufferSize];
            long totalRead = 0;

            while (true)
            {
                var bytesRead = stream.Read(chunk, 0, chunk.Length);
                if (bytesRead == 0)
                {
                    break;
                }

                totalRead += bytesRead;
                if (totalRead > maxBytes)
                {
                    return false;
                }

                buffer.Write(chunk, 0, bytesRead);
            }

            text = Encoding.UTF8.GetString(buffer.ToArray());
            return true;
        }
        catch
        {
            return false;
        }
    }

    private static bool TryReadVersionMarker(string path, out SemanticVersion version)
    {
        version = default;
        if (!File.Exists(path))
        {
            return false;
        }

        try
        {
            if (!TryReadSmallTextFile(path, MaxVersionMarkerBytes, out var json))
            {
                return false;
            }

            var marker = JsonSerializer.Deserialize<InstalledVersionMarker>(json, JsonOptions);
            if (marker is null || !IsTrustedMarkerRepository(marker.Repository))
            {
                return false;
            }

            return SemanticVersion.TryParse(marker?.Version, out version);
        }
        catch
        {
            return false;
        }
    }

    private static bool TryReadVersionTextFile(string path, out SemanticVersion version)
    {
        version = default;
        if (!File.Exists(path))
        {
            return false;
        }

        try
        {
            if (!TryReadSmallTextFile(path, MaxDiscoveryFileBytes, out var text))
            {
                return false;
            }

            return SemanticVersion.TryParse(text, out version);
        }
        catch
        {
            return false;
        }
    }

    private static bool TryReadDllFileVersion(string path, out SemanticVersion version)
    {
        version = default;
        if (!File.Exists(path))
        {
            return false;
        }

        try
        {
            var attributes = File.GetAttributes(path);
            if ((attributes & (FileAttributes.Directory | FileAttributes.ReparsePoint)) != 0)
            {
                return false;
            }

            var info = FileVersionInfo.GetVersionInfo(path);
            return SemanticVersion.TryParse(info.ProductVersion, out version)
                || SemanticVersion.TryParse(info.FileVersion, out version);
        }
        catch
        {
            return false;
        }
    }

    private static void AddInstallCandidate(
        ICollection<InstallCandidate> candidates,
        ISet<string> seenPaths,
        string source,
        string? path)
    {
        var root = ResolveInstallRoot(path);
        if (root is null)
        {
            return;
        }

        if (!TryGetNormalizedFullPath(root, out var key))
        {
            return;
        }

        if (IsReparsePoint(key))
        {
            return;
        }

        if (seenPaths.Add(key))
        {
            candidates.Add(new InstallCandidate(source, key));
        }
    }

    private static bool IsTrustedMarkerRepository(string? repository)
    {
        return string.IsNullOrWhiteSpace(repository)
            || string.Equals(repository, GitHubReleaseClient.Repository, StringComparison.OrdinalIgnoreCase);
    }

    private static bool TryGetNormalizedFullPath(string path, out string normalized)
    {
        normalized = "";
        try
        {
            normalized = NormalizeFullPathForComparison(path);
            return true;
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException or IOException)
        {
            return false;
        }
    }

    private static IEnumerable<string> EnumerateSteamInstallCandidates()
    {
        foreach (var libraryPath in EnumerateSteamLibraryRoots().Distinct(StringComparer.OrdinalIgnoreCase))
        {
            yield return Path.Combine(libraryPath, "steamapps", "common", "Quake 2");
        }

        yield return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam", "steamapps", "common", "Quake 2");
        yield return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Steam", "steamapps", "common", "Quake 2");
    }

    private static IEnumerable<string> EnumerateSteamLibraryRoots()
    {
        foreach (var steamPath in EnumerateSteamRoots())
        {
            if (Directory.Exists(steamPath))
            {
                yield return steamPath;
            }

            var libraryFile = Path.Combine(steamPath, "steamapps", "libraryfolders.vdf");
            if (!File.Exists(libraryFile))
            {
                continue;
            }

            if (!TryReadSmallTextFile(libraryFile, MaxDiscoveryFileBytes, out var libraryText))
            {
                continue;
            }

            foreach (Match match in SteamLibraryPathRegex().Matches(libraryText))
            {
                var path = match.Groups["path"].Value.Replace(@"\\", @"\");
                if (Directory.Exists(path))
                {
                    yield return path;
                }
            }
        }
    }

    private static IEnumerable<string> EnumerateSteamRoots()
    {
        foreach (var keyPath in new[]
        {
            @"SOFTWARE\Valve\Steam",
            @"SOFTWARE\WOW6432Node\Valve\Steam"
        })
        {
            foreach (var hive in new[] { Registry.CurrentUser, Registry.LocalMachine })
            {
                using var key = TryOpenRegistrySubKey(hive, keyPath);
                var installPath = TryGetRegistryString(key, "InstallPath", "SteamPath");

                if (!string.IsNullOrWhiteSpace(installPath))
                {
                    yield return NormalizePath(installPath);
                }
            }
        }

        yield return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam");
        yield return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Steam");
    }

    private static IEnumerable<string> EnumerateEpicInstallCandidates()
    {
        foreach (var candidate in new[]
        {
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Epic Games", "Quake 2"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Epic Games", "QuakeII"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Epic Games", "Quake2"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Epic Games", "Quake 2"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Epic Games", "QuakeII"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Epic Games", "Quake2")
        })
        {
            yield return candidate;
        }

        var manifestsRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            "Epic",
            "EpicGamesLauncher",
            "Data",
            "Manifests");

        if (!Directory.Exists(manifestsRoot))
        {
            yield break;
        }

        List<string> manifestPaths;
        try
        {
            manifestPaths = Directory.EnumerateFiles(manifestsRoot, "*.item").ToList();
        }
        catch
        {
            yield break;
        }

        foreach (var manifestPath in manifestPaths)
        {
            var installLocation = TryReadEpicManifestInstallLocation(manifestPath);
            if (!string.IsNullOrWhiteSpace(installLocation))
            {
                yield return installLocation;
            }
        }
    }

    private static string? TryReadEpicManifestInstallLocation(string manifestPath)
    {
        try
        {
            if (!TryReadSmallTextFile(manifestPath, MaxDiscoveryFileBytes, out var manifestJson))
            {
                return null;
            }

            using var document = JsonDocument.Parse(manifestJson);
            var root = document.RootElement;
            var displayName = TryGetJsonString(root, "DisplayName");
            var appName = TryGetJsonString(root, "AppName");
            var installLocation = TryGetJsonString(root, "InstallLocation");

            if (!string.IsNullOrWhiteSpace(installLocation)
                && (LooksLikeQuake2Name(displayName) || LooksLikeQuake2Name(appName) || LooksLikeQuake2Name(installLocation)))
            {
                return NormalizePath(installLocation);
            }
        }
        catch
        {
            // Broken launcher manifests should not keep the updater from opening.
        }

        return null;
    }

    private static IEnumerable<string> EnumerateGogInstallCandidates()
    {
        var systemDriveRoot = Path.GetPathRoot(Environment.GetFolderPath(Environment.SpecialFolder.System)) ?? @"C:\";
        foreach (var candidate in new[]
        {
            Path.Combine(systemDriveRoot, "GOG Games", "Quake II"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "GOG Galaxy", "Games", "Quake II"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "GOG Galaxy", "Games", "Quake II")
        })
        {
            yield return candidate;
        }

        foreach (var keyPath in new[]
        {
            @"SOFTWARE\GOG.com\Games",
            @"SOFTWARE\WOW6432Node\GOG.com\Games"
        })
        {
            foreach (var hive in new[] { Registry.CurrentUser, Registry.LocalMachine })
            {
                using var gamesKey = TryOpenRegistrySubKey(hive, keyPath);
                if (gamesKey is null)
                {
                    continue;
                }

                foreach (var subKeyName in TryGetRegistrySubKeyNames(gamesKey))
                {
                    using var gameKey = TryOpenRegistrySubKey(gamesKey, subKeyName);
                    var gameName = TryGetRegistryString(gameKey, "gameName", "title", "name");
                    var path = TryGetRegistryString(gameKey, "path");

                    if (!string.IsNullOrWhiteSpace(path)
                        && (LooksLikeQuake2Name(gameName) || LooksLikeQuake2Name(path)))
                    {
                        yield return NormalizePath(path);
                    }
                }
            }
        }
    }

    private static RegistryKey? TryOpenRegistrySubKey(RegistryKey? key, string subKeyName)
    {
        if (key is null)
        {
            return null;
        }

        try
        {
            return key.OpenSubKey(subKeyName);
        }
        catch
        {
            return null;
        }
    }

    private static IReadOnlyList<string> TryGetRegistrySubKeyNames(RegistryKey key)
    {
        try
        {
            return key.GetSubKeyNames();
        }
        catch
        {
            return [];
        }
    }

    private static string? TryGetRegistryString(RegistryKey? key, params string[] valueNames)
    {
        if (key is null)
        {
            return null;
        }

        foreach (var valueName in valueNames)
        {
            try
            {
                if (key.GetValue(valueName) is string value && !string.IsNullOrWhiteSpace(value))
                {
                    return value;
                }
            }
            catch
            {
                // Try the next value name or key source.
            }
        }

        return null;
    }

    private static string? TryGetJsonString(JsonElement element, string propertyName)
    {
        return element.TryGetProperty(propertyName, out var property) && property.ValueKind == JsonValueKind.String
            ? property.GetString()
            : null;
    }

    private static bool LooksLikeQuake2Name(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var normalized = value.ToLowerInvariant()
            .Replace('_', ' ')
            .Replace('-', ' ');

        return normalized.Contains("quake 2")
            || normalized.Contains("quake2")
            || normalized.Contains("quake ii");
    }

    private static string ResolvePackageRoot(string extractRoot)
    {
        if (Directory.Exists(Path.Combine(extractRoot, "rerelease")))
        {
            return extractRoot;
        }

        var packageDirectories = Directory.GetDirectories(extractRoot)
            .Where(directory =>
                Directory.Exists(Path.Combine(directory, "rerelease"))
                || File.Exists(Path.Combine(directory, "README.html"))
                || File.Exists(Path.Combine(directory, "CHANGELOG.md")))
            .ToList();

        return packageDirectories.Count == 1 ? packageDirectories[0] : extractRoot;
    }

    private static void EnsureSafePackageRoot(string extractRoot, string packageRoot)
    {
        var root = NormalizeFullPathForComparison(extractRoot);
        var package = NormalizeFullPathForComparison(packageRoot);
        var rootPrefix = EnsureTrailingDirectorySeparator(root);
        if (!package.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase)
            && !string.Equals(package, root, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package resolved outside the temporary extraction folder.");
        }

        if (!Directory.Exists(packageRoot))
        {
            throw new InvalidOperationException("The release package root could not be found after extraction.");
        }

        if (IsReparsePoint(packageRoot))
        {
            throw new InvalidOperationException("The release package root is a reparse point.");
        }
    }

    private static async Task ExtractReleasePackageAsync(string zipPath, string extractRoot, CancellationToken cancellationToken)
    {
        using var archive = ZipFile.OpenRead(zipPath);
        var extractedPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        long totalUncompressedBytes = 0;
        var entryCount = 0;
        var fileCount = 0;

        foreach (var entry in archive.Entries)
        {
            cancellationToken.ThrowIfCancellationRequested();
            entryCount++;
            if (entryCount > MaxPackageEntryCount)
            {
                throw new InvalidOperationException($"The release package contains more than the supported limit of {MaxPackageEntryCount:N0} entries.");
            }

            var relativePath = NormalizeArchiveEntryPath(entry.FullName);
            var destinationPath = ResolvePathUnderRoot(
                extractRoot,
                relativePath,
                $"Archive entry would extract outside the temporary folder: {entry.FullName}");
            if (!extractedPaths.Add(destinationPath))
            {
                throw new InvalidOperationException($"The release package contains duplicate or conflicting entries for: {relativePath}");
            }

            if (string.IsNullOrEmpty(entry.Name))
            {
                Directory.CreateDirectory(destinationPath);
                continue;
            }

            fileCount++;
            if (fileCount > MaxPackageFileCount)
            {
                throw new InvalidOperationException($"The release package contains more than the supported limit of {MaxPackageFileCount:N0} files.");
            }

            if (entry.Length < 0 || entry.Length > MaxPackageUncompressedBytes - totalUncompressedBytes)
            {
                throw new InvalidOperationException($"The release package expands beyond the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
            }

            var destinationDirectory = Path.GetDirectoryName(destinationPath);
            if (!string.IsNullOrWhiteSpace(destinationDirectory))
            {
                Directory.CreateDirectory(destinationDirectory);
            }

            try
            {
                await using var source = entry.Open();
                var bytesWritten = await CopyArchiveEntryToFileAsync(
                    source,
                    destinationPath,
                    MaxPackageUncompressedBytes - totalUncompressedBytes,
                    cancellationToken);
                if (bytesWritten != entry.Length)
                {
                    throw new InvalidOperationException($"The release package entry {relativePath} extracted to {bytesWritten:N0} bytes, but the archive declared {entry.Length:N0} bytes.");
                }

                totalUncompressedBytes += bytesWritten;
            }
            catch
            {
                TryDeleteFile(destinationPath);
                throw;
            }

            SetLastWriteTimeUtcBestEffort(destinationPath, entry.LastWriteTime.UtcDateTime);
        }
    }

    private static async Task<long> CopyArchiveEntryToFileAsync(
        Stream source,
        string destinationPath,
        long maxBytes,
        CancellationToken cancellationToken)
    {
        await using var destination = new FileStream(
            destinationPath,
            FileMode.CreateNew,
            FileAccess.Write,
            FileShare.None,
            CopyBufferSize,
            FileOptions.Asynchronous | FileOptions.SequentialScan);

        var buffer = new byte[CopyBufferSize];
        long totalBytes = 0;
        while (true)
        {
            var bytesRead = await source.ReadAsync(buffer.AsMemory(0, buffer.Length), cancellationToken);
            if (bytesRead == 0)
            {
                return totalBytes;
            }

            totalBytes += bytesRead;
            if (totalBytes > maxBytes)
            {
                throw new InvalidOperationException($"The release package expands beyond the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
            }

            await destination.WriteAsync(buffer.AsMemory(0, bytesRead), cancellationToken);
        }
    }

    private static string NormalizeArchiveEntryPath(string entryPath)
    {
        if (string.IsNullOrWhiteSpace(entryPath))
        {
            throw new InvalidOperationException("The release package contains an empty archive entry.");
        }

        var normalized = entryPath.Replace('\\', '/');
        if (Path.IsPathRooted(normalized))
        {
            throw new InvalidOperationException($"The release package contains a rooted archive entry: {entryPath}");
        }

        var segments = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length == 0)
        {
            throw new InvalidOperationException($"The release package contains an invalid archive entry: {entryPath}");
        }

        foreach (var segment in segments)
        {
            if (IsUnsafeArchivePathSegment(segment))
            {
                throw new InvalidOperationException($"The release package contains an unsafe archive entry: {entryPath}");
            }
        }

        return Path.Combine(segments);
    }

    private static bool IsUnsafeArchivePathSegment(string segment)
    {
        if (segment is "." or ".."
            || segment.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || segment.EndsWith(' ')
            || segment.EndsWith('.'))
        {
            return true;
        }

        var nameWithoutExtension = segment.Split('.', 2)[0];
        return ReservedWindowsFileNames.Contains(nameWithoutExtension);
    }

    private static void EnsureSafeInstallWritePath(string installPath, string destinationPath, string failureMessage)
    {
        EnsureSafeInstallRoot(installPath);

        var root = NormalizeFullPathForComparison(installPath);
        var destination = NormalizeFullPathForComparison(destinationPath);
        var rootPrefix = EnsureTrailingDirectorySeparator(root);
        if (!destination.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase)
            && !string.Equals(destination, root, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(failureMessage);
        }

        if ((File.Exists(destinationPath) || Directory.Exists(destinationPath))
            && IsReparsePoint(destinationPath))
        {
            throw new InvalidOperationException(failureMessage);
        }

        var directory = Path.GetDirectoryName(destinationPath);
        while (!string.IsNullOrWhiteSpace(directory))
        {
            var current = NormalizeFullPathForComparison(directory);
            if (string.Equals(current, root, StringComparison.OrdinalIgnoreCase))
            {
                break;
            }

            if (!current.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException(failureMessage);
            }

            if (Directory.Exists(current) && IsReparsePoint(current))
            {
                throw new InvalidOperationException(failureMessage);
            }

            directory = Directory.GetParent(current)?.FullName;
        }
    }

    private static void EnsureSafeInstallRoot(string installPath)
    {
        if (!Directory.Exists(installPath))
        {
            throw new InvalidOperationException("The selected Quake 2 installation folder could not be found.");
        }

        if (IsReparsePoint(installPath))
        {
            throw new InvalidOperationException("The selected Quake 2 installation folder is a reparse point and will not be modified automatically.");
        }
    }

    private static void EnsureSafeLaunchRoot(string installPath)
    {
        if (!Directory.Exists(installPath))
        {
            throw new InvalidOperationException("The selected Quake 2 installation folder could not be found.");
        }

        if (IsReparsePoint(installPath))
        {
            throw new InvalidOperationException("The selected Quake 2 installation folder is a reparse point and will not be launched automatically.");
        }
    }

    private static bool IsReparsePoint(string path)
    {
        try
        {
            return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0;
        }
        catch
        {
            return false;
        }
    }

    private static void ValidateReleasePackage(string packageRoot, ReleaseInfo release)
    {
        var packageBaseq2 = Path.Combine(packageRoot, "rerelease", "baseq2");
        var packageDll = Path.Combine(packageBaseq2, "game_x64.dll");
        if (!File.Exists(packageDll))
        {
            throw new InvalidOperationException("The release package is missing rerelease\\baseq2\\game_x64.dll.");
        }

        if (IsReparsePoint(packageDll))
        {
            throw new InvalidOperationException("The release package game_x64.dll is a reparse point.");
        }

        if (!TryReadVersionMarker(Path.Combine(packageBaseq2, MarkerJsonFileName), out var packageVersion)
            && !TryReadVersionTextFile(Path.Combine(packageBaseq2, MarkerTextFileName), out packageVersion))
        {
            throw new InvalidOperationException("The release package is missing a readable MuffMode version marker.");
        }

        if (packageVersion.CompareTo(release.Version) != 0)
        {
            throw new InvalidOperationException($"The release package version ({packageVersion}) does not match the GitHub release version ({release.Version}).");
        }

        foreach (var filePath in EnumeratePackageFiles(packageRoot))
        {
            var relativePath = Path.GetRelativePath(packageRoot, filePath);
            if (!IsAllowedPackagePath(relativePath))
            {
                throw new InvalidOperationException($"The release package contains an unexpected file path: {relativePath}");
            }
        }
    }

    private static IEnumerable<string> EnumeratePackageFiles(string packageRoot)
    {
        return Directory.EnumerateFiles(
            packageRoot,
            "*",
            new EnumerationOptions
            {
                RecurseSubdirectories = true,
                AttributesToSkip = FileAttributes.ReparsePoint,
                IgnoreInaccessible = false
            });
    }

    private static bool IsAllowedPackagePath(string relativePath)
    {
        var segments = relativePath.Split(
            new[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar },
            StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length == 0)
        {
            return false;
        }

        if (segments.Length == 1)
        {
            return AllowedRootPackageFiles.Contains(segments[0]);
        }

        if (!string.Equals(segments[0], "rerelease", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var normalized = string.Join(Path.DirectorySeparatorChar, segments);
        var extension = Path.GetExtension(normalized);
        if (IsBlockedPackageExtension(extension))
        {
            return string.Equals(
                normalized,
                Path.Combine("rerelease", "baseq2", "game_x64.dll"),
                StringComparison.OrdinalIgnoreCase);
        }

        return true;
    }

    private static bool IsBlockedPackageExtension(string extension)
    {
        return extension.Equals(".bat", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".cmd", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".com", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".hta", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".jar", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".dll", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".exe", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".js", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".lnk", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".msi", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".pif", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".ps1", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".scr", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".vbs", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".wsf", StringComparison.OrdinalIgnoreCase);
    }

    private static string ResolveDestinationPath(string installPath, string relativePath)
    {
        return ResolvePathUnderRoot(
            installPath,
            relativePath,
            $"Package entry would write outside the selected Quake 2 folder: {relativePath}");
    }

    private static string ResolvePathUnderRoot(string rootPath, string relativePath, string failureMessage)
    {
        var root = NormalizeFullPathForComparison(rootPath);
        var destinationPath = Path.GetFullPath(Path.Combine(root, relativePath));
        var destinationForComparison = NormalizeFullPathForComparison(destinationPath);
        var rootPrefix = EnsureTrailingDirectorySeparator(root);

        if (!destinationForComparison.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase)
            && !string.Equals(destinationForComparison, root, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(failureMessage);
        }

        return destinationPath;
    }

    private static string NormalizeFullPathForComparison(string path)
    {
        var fullPath = Path.GetFullPath(path);
        var root = Path.GetPathRoot(fullPath);
        if (!string.IsNullOrEmpty(root) && string.Equals(fullPath, root, StringComparison.OrdinalIgnoreCase))
        {
            return fullPath;
        }

        return fullPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
    }

    private static string EnsureTrailingDirectorySeparator(string path)
    {
        return Path.EndsInDirectorySeparator(path)
            ? path
            : path + Path.DirectorySeparatorChar;
    }

    private static string? GetRunningUpdaterPath()
    {
        try
        {
            return Environment.ProcessPath ?? Process.GetCurrentProcess().MainModule?.FileName;
        }
        catch
        {
            return null;
        }
    }

    private static bool IsRunningUpdaterDestination(string destinationPath, string? runningUpdaterPath)
    {
        if (string.IsNullOrWhiteSpace(runningUpdaterPath))
        {
            return false;
        }

        var destinationFullPath = Path.GetFullPath(destinationPath);
        var runningFullPath = Path.GetFullPath(runningUpdaterPath);
        return string.Equals(destinationFullPath, runningFullPath, StringComparison.OrdinalIgnoreCase);
    }

    private static void CopyFileAtomically(string sourcePath, string destinationPath)
    {
        var destinationDirectory = Path.GetDirectoryName(destinationPath);
        if (!string.IsNullOrWhiteSpace(destinationDirectory))
        {
            Directory.CreateDirectory(destinationDirectory);
        }

        var temporaryPath = Path.Combine(
            destinationDirectory ?? Path.GetTempPath(),
            $".{Path.GetFileName(destinationPath)}.{Guid.NewGuid():N}.tmp");
        var originalAttributes = PrepareDestinationForReplace(destinationPath);

        try
        {
            File.Copy(sourcePath, temporaryPath, overwrite: false);
            VerifyCopiedFileLength(sourcePath, temporaryPath, "Copied package file length did not match the source file.");
            SetLastWriteTimeUtcBestEffort(temporaryPath, File.GetLastWriteTimeUtc(sourcePath));
            ReplaceTemporaryFile(temporaryPath, destinationPath);
        }
        catch
        {
            TryDeleteFile(temporaryPath);
            RestoreDestinationAttributes(destinationPath, originalAttributes);
            throw;
        }
    }

    private static void CopyFileWithVerification(string sourcePath, string destinationPath)
    {
        File.Copy(sourcePath, destinationPath, overwrite: false);
        try
        {
            VerifyCopiedFileLength(sourcePath, destinationPath, "Backup file length did not match the source game DLL.");
        }
        catch
        {
            TryDeleteFile(destinationPath);
            throw;
        }
    }

    private static void VerifyCopiedFileLength(string sourcePath, string destinationPath, string failureMessage)
    {
        var sourceLength = new FileInfo(sourcePath).Length;
        var destinationLength = new FileInfo(destinationPath).Length;
        if (sourceLength != destinationLength)
        {
            throw new IOException($"{failureMessage} Expected {sourceLength:N0} bytes, wrote {destinationLength:N0} bytes.");
        }
    }

    private static void SetLastWriteTimeUtcBestEffort(string path, DateTime lastWriteTimeUtc)
    {
        try
        {
            File.SetLastWriteTimeUtc(path, lastWriteTimeUtc);
        }
        catch
        {
            // File timestamp metadata should not make an otherwise valid install fail.
        }
    }

    private static void WriteAllTextAtomic(string path, string contents)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        var temporaryPath = Path.Combine(
            directory ?? Path.GetTempPath(),
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        var originalAttributes = PrepareDestinationForReplace(path);

        try
        {
            File.WriteAllText(temporaryPath, contents, Encoding.UTF8);
            ReplaceTemporaryFile(temporaryPath, path);
        }
        catch
        {
            TryDeleteFile(temporaryPath);
            RestoreDestinationAttributes(path, originalAttributes);
            throw;
        }
    }

    private static FileAttributes? PrepareDestinationForReplace(string destinationPath)
    {
        if (!File.Exists(destinationPath) && !Directory.Exists(destinationPath))
        {
            return null;
        }

        var attributes = File.GetAttributes(destinationPath);
        if ((attributes & FileAttributes.Directory) != 0)
        {
            throw new IOException($"The destination path is a directory: {destinationPath}");
        }

        if ((attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new IOException($"The destination path is a reparse point: {destinationPath}");
        }

        if ((attributes & FileAttributes.ReadOnly) != 0)
        {
            File.SetAttributes(destinationPath, attributes & ~FileAttributes.ReadOnly);
        }

        return attributes;
    }

    private static void ReplaceTemporaryFile(string temporaryPath, string destinationPath)
    {
        if (File.Exists(destinationPath))
        {
            File.Replace(temporaryPath, destinationPath, destinationBackupFileName: null, ignoreMetadataErrors: true);
            return;
        }

        File.Move(temporaryPath, destinationPath);
    }

    private static void RestoreDestinationAttributes(string destinationPath, FileAttributes? attributes)
    {
        if (attributes is null || !File.Exists(destinationPath))
        {
            return;
        }

        try
        {
            File.SetAttributes(destinationPath, attributes.Value);
        }
        catch
        {
            // Attribute restoration is best-effort after a failed replace.
        }
    }

    private static void BackupCurrentGameDll(string installPath, IProgress<UpdaterProgress>? progress)
    {
        var baseq2 = GetBaseq2Path(installPath);
        var dllPath = Path.Combine(baseq2, "game_x64.dll");
        if (!File.Exists(dllPath))
        {
            return;
        }

        if (IsReparsePoint(dllPath))
        {
            throw new InvalidOperationException("The existing game_x64.dll is a reparse point and will not be backed up or replaced automatically.");
        }

        var backupDirectory = Path.Combine(baseq2, "MuffModeBackups");
        EnsureSafeInstallWritePath(
            installPath,
            Path.Combine(backupDirectory, "game_x64.backup-check"),
            "The MuffMode backup folder would write through an unsafe install path.");
        Directory.CreateDirectory(backupDirectory);

        var timestamp = DateTimeOffset.Now.ToString("yyyyMMdd-HHmmss");
        var backupPath = GetUniqueBackupPath(backupDirectory, timestamp);
        CopyFileWithVerification(dllPath, backupPath);
        progress?.Report(new UpdaterProgress($"Backed up existing game_x64.dll to {Path.GetFileName(backupPath)}.", 49, CanCancel: false));
    }

    private static string GetUniqueBackupPath(string backupDirectory, string timestamp)
    {
        var backupPath = Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}.dll");
        if (!PathExists(backupPath))
        {
            return backupPath;
        }

        for (var index = 2; index < 100; index++)
        {
            var indexedBackupPath = Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}-{index}.dll");
            if (!PathExists(indexedBackupPath))
            {
                return indexedBackupPath;
            }
        }

        for (var attempt = 0; attempt < 10; attempt++)
        {
            var randomBackupPath = Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}-{Guid.NewGuid():N}.dll");
            if (!PathExists(randomBackupPath))
            {
                return randomBackupPath;
            }
        }

        throw new IOException("Could not allocate a unique MuffMode backup file path.");
    }

    private static bool PathExists(string path)
    {
        try
        {
            return File.Exists(path) || Directory.Exists(path);
        }
        catch
        {
            return true;
        }
    }

    private static void WriteInstalledMarker(string installPath, ReleaseInfo release)
    {
        var baseq2 = GetBaseq2Path(installPath);
        EnsureSafeInstallWritePath(
            installPath,
            Path.Combine(baseq2, MarkerJsonFileName),
            "The MuffMode version marker would write through an unsafe install path.");
        Directory.CreateDirectory(baseq2);

        var marker = new InstalledVersionMarker
        {
            Version = release.Version.ToString(),
            TagName = release.TagName,
            ReleaseUrl = release.HtmlUrl,
            AssetName = release.AssetName,
            InstalledAtUtc = DateTimeOffset.UtcNow
        };

        WriteAllTextAtomic(Path.Combine(baseq2, MarkerJsonFileName), JsonSerializer.Serialize(marker, JsonOptions));
        WriteAllTextAtomic(Path.Combine(baseq2, MarkerTextFileName), release.Version.ToString());
    }

    private static IEnumerable<string> EnumerateLaunchCandidates(string installPath)
    {
        foreach (var relativePath in new[]
        {
            "quake2.exe",
            "q2.exe",
            @"rerelease\quake2.exe",
            @"rerelease\Quake2.exe",
            @"rerelease\quake2rerelease.exe",
            @"rerelease\Quake2Rerelease.exe",
            @"rerelease\quake2ex.exe",
            @"rerelease\quake2ex_steam.exe",
            @"rerelease\Quake II.exe",
            @"rerelease\Quake II Rerelease.exe"
        })
        {
            yield return Path.Combine(installPath, relativePath);
        }
    }

    private static bool IsLaunchableFile(string path)
    {
        try
        {
            if (!File.Exists(path))
            {
                return false;
            }

            var attributes = File.GetAttributes(path);
            return (attributes & (FileAttributes.Directory | FileAttributes.ReparsePoint)) == 0
                && string.Equals(Path.GetExtension(path), ".exe", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    private static bool IsSafeLaunchCandidate(string installPath, string candidatePath)
    {
        try
        {
            var root = NormalizeFullPathForComparison(installPath);
            var candidate = NormalizeFullPathForComparison(candidatePath);
            var rootPrefix = EnsureTrailingDirectorySeparator(root);
            if (!candidate.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase))
            {
                return false;
            }

            var directory = Path.GetDirectoryName(candidatePath);
            while (!string.IsNullOrWhiteSpace(directory))
            {
                var current = NormalizeFullPathForComparison(directory);
                if (string.Equals(current, root, StringComparison.OrdinalIgnoreCase))
                {
                    return true;
                }

                if (!current.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase)
                    || (Directory.Exists(current) && IsReparsePoint(current)))
                {
                    return false;
                }

                directory = Directory.GetParent(current)?.FullName;
            }
        }
        catch
        {
            return false;
        }

        return false;
    }

    private static void CleanupOldExtractionDirectories(string updaterTempRoot)
    {
        if (!Directory.Exists(updaterTempRoot))
        {
            return;
        }

        List<string> extractionDirectories;
        try
        {
            extractionDirectories = Directory.EnumerateDirectories(updaterTempRoot)
                .Where(directory => Guid.TryParse(Path.GetFileName(directory), out _))
                .ToList();
        }
        catch
        {
            return;
        }

        var cutoffUtc = DateTime.UtcNow - TemporaryExtractionMaxAge;
        foreach (var extractionDirectory in extractionDirectories)
        {
            try
            {
                var attributes = File.GetAttributes(extractionDirectory);
                if ((attributes & FileAttributes.ReparsePoint) != 0
                    || Directory.GetLastWriteTimeUtc(extractionDirectory) >= cutoffUtc)
                {
                    continue;
                }

                Directory.Delete(extractionDirectory, recursive: true);
            }
            catch
            {
                // Stale temp cleanup failure is non-fatal.
            }
        }
    }

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path) && !IsReparsePoint(path))
            {
                Directory.Delete(path, recursive: true);
            }
        }
        catch
        {
            // Temp cleanup failure is non-fatal.
        }
    }

    private static void TryDeleteFile(string path)
    {
        try
        {
            if (File.Exists(path) && !IsReparsePoint(path))
            {
                File.Delete(path);
            }
        }
        catch
        {
            // Temp cleanup failure is non-fatal.
        }
    }

    [GeneratedRegex(@"""path""\s+""(?<path>[^""]+)""", RegexOptions.IgnoreCase)]
    private static partial Regex SteamLibraryPathRegex();
}
