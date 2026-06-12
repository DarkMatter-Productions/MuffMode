using System.Diagnostics;
using System.IO.Compression;
using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace MuffMode.Updater;

internal static partial class InstallationManager
{
    private const string SettingsFileName = "updater-settings.json";
    private const string MarkerJsonFileName = "muffmode-version.json";
    private const string MarkerTextFileName = "muffmode.version";
    private const int MaxPackageFileCount = 20_000;
    private const long MaxPackageUncompressedBytes = 1024L * 1024L * 1024L;

    private static readonly HashSet<string> AllowedRootPackageFiles = new(StringComparer.OrdinalIgnoreCase)
    {
        "CHANGELOG.md",
        "MuffMode.version",
        "MuffModeUpdater.exe",
        "README.html",
        "README.md",
        "VERSION"
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
            if (File.Exists(SettingsPath))
            {
                var json = File.ReadAllText(SettingsPath);
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
        Directory.CreateDirectory(SettingsDirectory);
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

        var normalized = NormalizePath(installPath);
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

    public static bool IsValidInstallPath(string? installPath) => ResolveInstallRoot(installPath) is not null;

    public static LocalInstallVersion ReadLocalVersion(string? installPath)
    {
        if (!IsValidInstallPath(installPath))
        {
            return new LocalInstallVersion(null, "Choose a valid Quake 2 install folder", "No valid install path");
        }

        var normalized = ResolveInstallRoot(installPath)!;
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
        if (!IsValidInstallPath(installPath))
        {
            throw new InvalidOperationException("Select the Quake 2 installation folder, its rerelease folder, or its baseq2 folder.");
        }

        var normalizedInstallPath = ResolveInstallRoot(installPath)!;
        var extractRoot = Path.Combine(Path.GetTempPath(), "MuffModeUpdater", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(extractRoot);

        try
        {
            progress?.Report(new UpdaterProgress("Extracting release package...", 46));
            await ExtractReleasePackageAsync(zipPath, extractRoot, cancellationToken);

            var packageRoot = ResolvePackageRoot(extractRoot);
            ValidateReleasePackage(packageRoot, release);

            var releaseFiles = Directory.EnumerateFiles(packageRoot, "*", SearchOption.AllDirectories)
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
        if (!IsValidInstallPath(installPath))
        {
            throw new InvalidOperationException("Select a valid Quake 2 installation folder before launching.");
        }

        var normalized = ResolveInstallRoot(installPath)!;
        foreach (var candidate in EnumerateLaunchCandidates(normalized))
        {
            if (File.Exists(candidate))
            {
                Process.Start(new ProcessStartInfo(candidate)
                {
                    WorkingDirectory = Path.GetDirectoryName(candidate) ?? normalized,
                    UseShellExecute = true
                });
                return;
            }
        }

        Process.Start(new ProcessStartInfo("steam://rungameid/2320")
        {
            UseShellExecute = true
        });
    }

    private static string NormalizePath(string? path) => (path ?? "").Trim().Trim('"');

    private static string GetBaseq2Path(string installPath) => Path.Combine(installPath, "rerelease", "baseq2");

    private static bool TryReadVersionMarker(string path, out SemanticVersion version)
    {
        version = default;
        if (!File.Exists(path))
        {
            return false;
        }

        try
        {
            var marker = JsonSerializer.Deserialize<InstalledVersionMarker>(File.ReadAllText(path), JsonOptions);
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
            var text = File.ReadAllText(path);
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

        var key = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        if (seenPaths.Add(key))
        {
            candidates.Add(new InstallCandidate(source, key));
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

            string libraryText;
            try
            {
                libraryText = File.ReadAllText(libraryFile);
            }
            catch
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
                using var key = hive.OpenSubKey(keyPath);
                var installPath = key?.GetValue("InstallPath") as string
                    ?? key?.GetValue("SteamPath") as string;

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
            using var document = JsonDocument.Parse(File.ReadAllText(manifestPath));
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
                using var gamesKey = hive.OpenSubKey(keyPath);
                if (gamesKey is null)
                {
                    continue;
                }

                foreach (var subKeyName in gamesKey.GetSubKeyNames())
                {
                    using var gameKey = gamesKey.OpenSubKey(subKeyName);
                    var gameName = gameKey?.GetValue("gameName") as string
                        ?? gameKey?.GetValue("title") as string
                        ?? gameKey?.GetValue("name") as string;
                    var path = gameKey?.GetValue("path") as string;

                    if (!string.IsNullOrWhiteSpace(path)
                        && (LooksLikeQuake2Name(gameName) || LooksLikeQuake2Name(path)))
                    {
                        yield return NormalizePath(path);
                    }
                }
            }
        }
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

    private static async Task ExtractReleasePackageAsync(string zipPath, string extractRoot, CancellationToken cancellationToken)
    {
        using var archive = ZipFile.OpenRead(zipPath);
        var extractedPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        long totalUncompressedBytes = 0;
        var fileCount = 0;

        foreach (var entry in archive.Entries)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var relativePath = NormalizeArchiveEntryPath(entry.FullName);
            var destinationPath = ResolvePathUnderRoot(
                extractRoot,
                relativePath,
                $"Archive entry would extract outside the temporary folder: {entry.FullName}");

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

            totalUncompressedBytes += entry.Length;
            if (totalUncompressedBytes > MaxPackageUncompressedBytes)
            {
                throw new InvalidOperationException($"The release package expands beyond the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
            }

            if (!extractedPaths.Add(destinationPath))
            {
                throw new InvalidOperationException($"The release package contains duplicate entries for: {relativePath}");
            }

            var destinationDirectory = Path.GetDirectoryName(destinationPath);
            if (!string.IsNullOrWhiteSpace(destinationDirectory))
            {
                Directory.CreateDirectory(destinationDirectory);
            }

            await using (var source = entry.Open())
            await using (var destination = new FileStream(
                destinationPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                128 * 1024,
                FileOptions.Asynchronous | FileOptions.SequentialScan))
            {
                await source.CopyToAsync(destination, cancellationToken);
            }

            File.SetLastWriteTimeUtc(destinationPath, entry.LastWriteTime.UtcDateTime);
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
            if (segment is "." or ".." || segment.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            {
                throw new InvalidOperationException($"The release package contains an unsafe archive entry: {entryPath}");
            }
        }

        return Path.Combine(segments);
    }

    private static void ValidateReleasePackage(string packageRoot, ReleaseInfo release)
    {
        var packageBaseq2 = Path.Combine(packageRoot, "rerelease", "baseq2");
        var packageDll = Path.Combine(packageBaseq2, "game_x64.dll");
        if (!File.Exists(packageDll))
        {
            throw new InvalidOperationException("The release package is missing rerelease\\baseq2\\game_x64.dll.");
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

        foreach (var filePath in Directory.EnumerateFiles(packageRoot, "*", SearchOption.AllDirectories))
        {
            var relativePath = Path.GetRelativePath(packageRoot, filePath);
            if (!IsAllowedPackagePath(relativePath))
            {
                throw new InvalidOperationException($"The release package contains an unexpected file path: {relativePath}");
            }
        }
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
        var root = Path.GetFullPath(rootPath).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        var destinationPath = Path.GetFullPath(Path.Combine(root, relativePath));
        var rootPrefix = root + Path.DirectorySeparatorChar;

        if (!destinationPath.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase)
            && !string.Equals(destinationPath, root, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(failureMessage);
        }

        return destinationPath;
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

        try
        {
            if (File.Exists(destinationPath))
            {
                var attributes = File.GetAttributes(destinationPath);
                if ((attributes & FileAttributes.ReadOnly) != 0)
                {
                    File.SetAttributes(destinationPath, attributes & ~FileAttributes.ReadOnly);
                }
            }

            File.Copy(sourcePath, temporaryPath, overwrite: false);
            File.SetLastWriteTimeUtc(temporaryPath, File.GetLastWriteTimeUtc(sourcePath));

            if (File.Exists(destinationPath))
            {
                File.Replace(temporaryPath, destinationPath, destinationBackupFileName: null, ignoreMetadataErrors: true);
            }
            else
            {
                File.Move(temporaryPath, destinationPath);
            }
        }
        catch
        {
            TryDeleteFile(temporaryPath);
            throw;
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

        try
        {
            File.WriteAllText(temporaryPath, contents);
            File.Move(temporaryPath, path, overwrite: true);
        }
        catch
        {
            TryDeleteFile(temporaryPath);
            throw;
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

        var backupDirectory = Path.Combine(baseq2, "MuffModeBackups");
        Directory.CreateDirectory(backupDirectory);

        var timestamp = DateTimeOffset.Now.ToString("yyyyMMdd-HHmmss");
        var backupPath = GetUniqueBackupPath(backupDirectory, timestamp);
        File.Copy(dllPath, backupPath, overwrite: false);
        progress?.Report(new UpdaterProgress($"Backed up existing game_x64.dll to {Path.GetFileName(backupPath)}.", 49, CanCancel: false));
    }

    private static string GetUniqueBackupPath(string backupDirectory, string timestamp)
    {
        var backupPath = Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}.dll");
        if (!File.Exists(backupPath))
        {
            return backupPath;
        }

        for (var index = 2; index < 100; index++)
        {
            var indexedBackupPath = Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}-{index}.dll");
            if (!File.Exists(indexedBackupPath))
            {
                return indexedBackupPath;
            }
        }

        return Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}-{Guid.NewGuid():N}.dll");
    }

    private static void WriteInstalledMarker(string installPath, ReleaseInfo release)
    {
        var baseq2 = GetBaseq2Path(installPath);
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
            @"rerelease\quake2ex.exe",
            @"rerelease\quake2ex_steam.exe",
            @"rerelease\Quake II.exe"
        })
        {
            yield return Path.Combine(installPath, relativePath);
        }
    }

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path))
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
            if (File.Exists(path))
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
