using System.Diagnostics;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
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
    private const int MaxPackageDirectoryCount = 20_000;
    private const int MaxPackageFileCount = 20_000;
    private const int MaxArchiveEntryPathCharacters = 512;
    private const int MaxArchivePathSegmentCharacters = 128;
    private const long MinPackageZipBytes = 1024L * 1024L;
    private const long MaxPackageCompressionRatio = 1000;
    private const long MaxPackageUncompressedBytes = 1024L * 1024L * 1024L;
    private const int CopyBufferSize = 128 * 1024;
    private const long MaxSettingsBytes = 64L * 1024L;
    private const long MaxVersionMarkerBytes = 64L * 1024L;
    private const long MaxDiscoveryFileBytes = 1024L * 1024L;
    private const long MinimumInstallFreeSpaceHeadroomBytes = 128L * 1024L * 1024L;
    private const int MaxSavedInstallPathCharacters = 4096;
    private const int MaxGameDllBackups = 20;
    private const ushort ImageFileMachineAmd64 = 0x8664;
    private static readonly TimeSpan TemporaryExtractionMaxAge = TimeSpan.FromDays(2);
    private static readonly TimeSpan MetadataTimestampFutureTolerance = TimeSpan.FromMinutes(10);
    private static readonly Encoding StrictUtf8Encoding = new UTF8Encoding(
        encoderShouldEmitUTF8Identifier: false,
        throwOnInvalidBytes: true);

    private sealed record PackageInstallFile(string SourcePath, string RelativePath, string DestinationPath);

    private static readonly HashSet<string> AllowedRootPackageFiles = new(StringComparer.OrdinalIgnoreCase)
    {
        "CHANGELOG.md",
        "LICENSE",
        "MuffMode.version",
        "MuffModeUpdater.exe",
        "README.html",
        "README.md",
        "THIRD_PARTY_NOTICES.md",
        "VERSION"
    };

    private static readonly HashSet<string> RequiredRootPackageFiles = new(StringComparer.OrdinalIgnoreCase)
    {
        "CHANGELOG.md",
        "LICENSE",
        "MuffMode.version",
        "MuffModeUpdater.exe",
        "README.html",
        "README.md",
        "THIRD_PARTY_NOTICES.md",
        "VERSION"
    };

    private static readonly HashSet<string> RequiredPackageFiles = new(StringComparer.OrdinalIgnoreCase)
    {
        Path.Combine("rerelease", "baseq2", "CONFIGS_README.md"),
        Path.Combine("rerelease", "baseq2", "gt-CA.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-CTF.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-DUEL.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-FFA.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-HORDE.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-INSTAGIB.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-NADEFEST.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-REDROVER.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-STRIKE.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-TDM.cfg"),
        Path.Combine("rerelease", "baseq2", "docs", "muffmode", "maps", "original-readmes", "README.md"),
        Path.Combine("rerelease", "baseq2", MarkerJsonFileName),
        Path.Combine("rerelease", "baseq2", MarkerTextFileName),
        Path.Combine("rerelease", "baseq2", "server-base.cfg")
    };

    private static readonly HashSet<string> RequiredInstallPlanFiles = new(StringComparer.OrdinalIgnoreCase)
    {
        "CHANGELOG.md",
        "LICENSE",
        "MuffMode.version",
        "README.html",
        "README.md",
        "THIRD_PARTY_NOTICES.md",
        "VERSION",
        Path.Combine("rerelease", "baseq2", "CONFIGS_README.md"),
        Path.Combine("rerelease", "baseq2", "game_x64.dll"),
        Path.Combine("rerelease", "baseq2", "gt-CA.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-CTF.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-DUEL.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-FFA.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-HORDE.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-INSTAGIB.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-NADEFEST.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-REDROVER.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-STRIKE.cfg"),
        Path.Combine("rerelease", "baseq2", "gt-TDM.cfg"),
        Path.Combine("rerelease", "baseq2", "docs", "muffmode", "maps", "original-readmes", "README.md"),
        Path.Combine("rerelease", "baseq2", MarkerJsonFileName),
        Path.Combine("rerelease", "baseq2", MarkerTextFileName),
        Path.Combine("rerelease", "baseq2", "server-base.cfg")
    };

    private static readonly HashSet<string> AllowedRereleaseTopLevelDirectories = new(StringComparer.OrdinalIgnoreCase)
    {
        "baseq2",
        "bots",
        "maps"
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
            ValidateSettingsPath();
            if (TryReadSmallTextFile(SettingsPath, MaxSettingsBytes, out var json))
            {
                return SanitizeSettings(JsonSerializer.Deserialize<AppSettings>(json, JsonOptions));
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
        var sanitizedSettings = SanitizeSettings(settings);
        ValidateSettingsPath();
        Directory.CreateDirectory(SettingsDirectory);
        ValidateSettingsPath();
        WriteAllTextAtomic(SettingsPath, JsonSerializer.Serialize(sanitizedSettings, JsonOptions));
    }

    private static AppSettings SanitizeSettings(AppSettings? settings)
    {
        if (settings is null)
        {
            return new AppSettings();
        }

        settings.InstallPath = SanitizeSavedInstallPath(settings.InstallPath);
        return settings;
    }

    private static string? SanitizeSavedInstallPath(string? installPath)
    {
        var normalized = NormalizePath(installPath);
        if (string.IsNullOrWhiteSpace(normalized)
            || normalized.Length > MaxSavedInstallPathCharacters
            || normalized.IndexOfAny(Path.GetInvalidPathChars()) >= 0)
        {
            return null;
        }

        try
        {
            if (!Path.IsPathFullyQualified(normalized))
            {
                return null;
            }

            return Path.GetFullPath(normalized);
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
        {
            return null;
        }
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
        return normalized is not null && FindUnsafeInstallDirectory(normalized) is null;
    }

    public static LocalInstallVersion ReadLocalVersion(string? installPath)
    {
        var normalized = ResolveInstallRoot(installPath);
        if (normalized is null)
        {
            return new LocalInstallVersion(null, "Choose a valid Quake 2 install folder", "No valid install path");
        }

        if (FindUnsafeInstallDirectory(normalized) is not null)
        {
            return new LocalInstallVersion(null, "Choose a valid Quake 2 install folder", "Install folder contains a reparse point");
        }

        var baseq2 = GetBaseq2Path(normalized);

        var jsonMarker = Path.Combine(baseq2, MarkerJsonFileName);
        if (TryReadVersionMarker(jsonMarker, out var markerVersion))
        {
            return new LocalInstallVersion(markerVersion, markerVersion.ToString(), MarkerJsonFileName);
        }

        if (File.Exists(jsonMarker))
        {
            return new LocalInstallVersion(null, "Unknown / marker unreadable", $"{MarkerJsonFileName} unreadable");
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
        ValidateDownloadedArchiveMatchesRelease(zipPath, release);
        var packageSha256 = ComputeFileSha256(zipPath);
        ValidateGitHubAssetDigest(release.AssetDigest, packageSha256);
        var updaterTempRoot = Path.Combine(Path.GetTempPath(), "MuffModeUpdater");
        PrepareUpdaterTempRoot(updaterTempRoot);
        CleanupOldExtractionDirectories(updaterTempRoot);
        var extractRoot = Path.Combine(updaterTempRoot, Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(extractRoot);
        EnsureDirectoryPathHasNoReparsePoints(extractRoot, "updater temporary extraction folder");

        try
        {
            progress?.Report(new UpdaterProgress("Extracting release package...", 46));
            await ExtractReleasePackageAsync(
                zipPath,
                extractRoot,
                Path.GetFileNameWithoutExtension(release.AssetName),
                cancellationToken);

            var packageRoot = ResolvePackageRoot(extractRoot);
            EnsureSafePackageRoot(extractRoot, packageRoot);
            ValidateExtractionRootContents(extractRoot, packageRoot);
            var packageMarker = ValidateReleasePackage(packageRoot, release, packageSha256);

            var releaseFiles = EnumeratePackageFiles(packageRoot)
                .Order(StringComparer.OrdinalIgnoreCase)
                .ToList();
            if (releaseFiles.Count == 0)
            {
                throw new InvalidOperationException("The downloaded release package did not contain any files to install.");
            }

            var runningUpdaterPath = GetRunningUpdaterPath();
            var installPlan = BuildInstallPlan(normalizedInstallPath, packageRoot, releaseFiles, runningUpdaterPath, progress);
            if (installPlan.Count == 0)
            {
                throw new InvalidOperationException("The downloaded release package did not contain any installable files.");
            }

            VerifyRequiredInstallPlanFiles(installPlan);
            EnsureGameProcessIsNotRunning(normalizedInstallPath);
            VerifyInstallPlanTargets(normalizedInstallPath, installPlan);
            EnsureSufficientInstallDiskSpace(normalizedInstallPath, installPlan);

            cancellationToken.ThrowIfCancellationRequested();
            progress?.Report(new UpdaterProgress("Applying release package...", 49, CanCancel: false));
            var backupFileName = BackupCurrentGameDll(normalizedInstallPath, progress);

            for (var index = 0; index < installPlan.Count; index++)
            {
                var installFile = installPlan[index];
                CopyFileAtomically(normalizedInstallPath, installFile.SourcePath, installFile.DestinationPath, installFile.RelativePath);

                var percentage = 50 + (int)Math.Round((double)(index + 1) / installPlan.Count * 45);
                progress?.Report(new UpdaterProgress($"Syncing {installFile.RelativePath}...", Math.Clamp(percentage, 50, 95), CanCancel: false));
            }

            VerifyInstallPlanApplied(installPlan);
            WriteInstalledMarker(normalizedInstallPath, release, packageMarker, packageSha256, installPlan, backupFileName);
            VerifyInstalledMarker(normalizedInstallPath, release, packageMarker, packageSha256, installPlan);
            progress?.Report(new UpdaterProgress($"MuffMode {release.Version} installed.", 100, CanCancel: false));
        }
        finally
        {
            TryDeleteDirectory(extractRoot);
        }
    }

    public static string LaunchGame(string installPath)
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
                return candidate;
            }
        }

        if (IsLikelySteamInstall(normalized))
        {
            StartProcessOrThrow(
                new ProcessStartInfo("steam://rungameid/2320")
                {
                    UseShellExecute = true
                },
                "Steam Quake II launch URL",
                requireProcessHandle: false);
            return "Steam Quake II launch URL";
        }

        throw new FileNotFoundException("Could not find a launchable Quake II executable under the selected install folder.");
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

    private static bool IsLikelySteamInstall(string installPath)
    {
        try
        {
            var normalized = NormalizeFullPathForComparison(installPath);
            var segments = normalized.Split(
                new[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar },
                StringSplitOptions.RemoveEmptyEntries);
            return segments.Any(segment => segment.Equals("steamapps", StringComparison.OrdinalIgnoreCase));
        }
        catch
        {
            return false;
        }
    }

    private static string NormalizePath(string? path) => (path ?? "").Trim().Trim('"');

    private static string GetBaseq2Path(string installPath) => Path.Combine(installPath, "rerelease", "baseq2");

    private static void ValidateSettingsPath()
    {
        EnsureDirectoryPathHasNoReparsePoints(SettingsDirectory, "MuffMode settings folder");

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

        if (length < MinPackageZipBytes)
        {
            throw new InvalidOperationException($"The downloaded release package is smaller than the supported minimum of {MinPackageZipBytes / 1024 / 1024} MB.");
        }

        if (length > MaxPackageUncompressedBytes)
        {
            throw new InvalidOperationException($"The downloaded release package is larger than the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
        }
    }

    private static void ValidateDownloadedArchiveMatchesRelease(string zipPath, ReleaseInfo release)
    {
        var fileName = Path.GetFileName(zipPath);
        if (!string.Equals(fileName, release.AssetName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The downloaded release package file name ({fileName}) did not match the selected GitHub asset ({release.AssetName}).");
        }

        if (release.AssetSize is not > 0)
        {
            throw new InvalidOperationException("GitHub release metadata did not include a usable package size.");
        }

        var actualLength = new FileInfo(zipPath).Length;
        if (actualLength != release.AssetSize.Value)
        {
            throw new InvalidOperationException($"The downloaded release package size ({actualLength:N0} bytes) did not match the selected GitHub asset size ({release.AssetSize.Value:N0} bytes).");
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

            text = StrictUtf8Encoding.GetString(buffer.ToArray());
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
        return TryReadInstalledVersionMarker(path, out var marker)
            && IsTrustedMarkerRepository(marker.Repository)
            && SemanticVersion.TryParse(marker.Version, out version);
    }

    private static bool TryReadInstalledVersionMarker(string path, out InstalledVersionMarker marker)
    {
        marker = new InstalledVersionMarker();
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

            var parsedMarker = JsonSerializer.Deserialize<InstalledVersionMarker>(json, JsonOptions);
            if (parsedMarker is null)
            {
                return false;
            }

            marker = parsedMarker;
            return true;
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

        if (FindUnsafeInstallDirectory(key) is not null)
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

        if (packageDirectories.Count > 1)
        {
            var rootNames = string.Join(", ", packageDirectories.Select(Path.GetFileName));
            throw new InvalidOperationException($"The release package contains multiple possible package roots: {rootNames}");
        }

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

    private static void ValidateExtractionRootContents(string extractRoot, string packageRoot)
    {
        var expectedRoot = NormalizeFullPathForComparison(packageRoot);
        foreach (var entry in Directory.EnumerateFileSystemEntries(extractRoot))
        {
            var actualEntry = NormalizeFullPathForComparison(entry);
            if (!string.Equals(actualEntry, expectedRoot, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException($"The release package contains unexpected top-level content: {Path.GetFileName(entry)}");
            }
        }
    }

    private static IReadOnlyList<PackageInstallFile> BuildInstallPlan(
        string installPath,
        string packageRoot,
        IReadOnlyList<string> releaseFiles,
        string? runningUpdaterPath,
        IProgress<UpdaterProgress>? progress)
    {
        var installPlan = new List<PackageInstallFile>(releaseFiles.Count);
        var destinationPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var sourcePath in releaseFiles)
        {
            var relativePath = Path.GetRelativePath(packageRoot, sourcePath);
            ValidatePackageSourceFile(sourcePath, relativePath);
            var destinationPath = ResolveDestinationPath(installPath, relativePath);
            if (IsRunningUpdaterDestination(destinationPath, runningUpdaterPath))
            {
                progress?.Report(new UpdaterProgress($"Skipping running updater executable: {relativePath}.", null, CanCancel: false));
                continue;
            }

            EnsureSafeInstallWritePath(
                installPath,
                destinationPath,
                $"Package entry would write through an unsafe install path: {relativePath}");
            var destinationKey = NormalizeFullPathForComparison(destinationPath);
            if (!destinationPaths.Add(destinationKey))
            {
                throw new InvalidOperationException($"The release package contains multiple files that resolve to the same install path: {relativePath}");
            }

            installPlan.Add(new PackageInstallFile(sourcePath, relativePath, destinationPath));
        }

        return installPlan;
    }

    private static void VerifyRequiredInstallPlanFiles(IReadOnlyList<PackageInstallFile> installPlan)
    {
        var plannedFiles = installPlan
            .Select(installFile => installFile.RelativePath)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        foreach (var requiredPath in RequiredInstallPlanFiles)
        {
            if (!plannedFiles.Contains(requiredPath))
            {
                throw new InvalidOperationException($"The release package install plan is missing required file: {requiredPath}");
            }
        }
    }

    private static void ValidatePackageSourceFile(string sourcePath, string relativePath)
    {
        if (!File.Exists(sourcePath))
        {
            throw new IOException($"The release package source file is missing: {relativePath}");
        }

        var attributes = File.GetAttributes(sourcePath);
        if ((attributes & (FileAttributes.Directory | FileAttributes.ReparsePoint)) != 0)
        {
            throw new IOException($"The release package source file is not a regular file: {relativePath}");
        }

        if (new FileInfo(sourcePath).Length == 0)
        {
            throw new IOException($"The release package source file became empty: {relativePath}");
        }
    }

    private static void EnsureGameProcessIsNotRunning(string installPath)
    {
        foreach (var processName in new[]
        {
            "quake2",
            "q2",
            "quake2rerelease",
            "quake2ex",
            "quake2ex_steam"
        }.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            foreach (var process in TryGetProcessesByName(processName))
            {
                using (process)
                {
                    var executablePath = TryGetProcessExecutablePath(process);
                    if (executablePath is not null && IsPathUnderRoot(installPath, executablePath))
                    {
                        throw new InvalidOperationException(
                            $"Close {process.ProcessName} (PID {process.Id}) before updating Muff Mode.");
                    }
                }
            }
        }
    }

    private static IReadOnlyList<Process> TryGetProcessesByName(string processName)
    {
        try
        {
            return Process.GetProcessesByName(processName);
        }
        catch
        {
            return [];
        }
    }

    private static string? TryGetProcessExecutablePath(Process process)
    {
        try
        {
            return process.MainModule?.FileName;
        }
        catch
        {
            return null;
        }
    }

    private static void VerifyInstallPlanTargets(string installPath, IReadOnlyList<PackageInstallFile> installPlan)
    {
        foreach (var installFile in installPlan)
        {
            EnsureSafeInstallWritePath(
                installPath,
                installFile.DestinationPath,
                $"Package entry would write through an unsafe install path: {installFile.RelativePath}");

            var destinationDirectory = Path.GetDirectoryName(installFile.DestinationPath);
            if (!string.IsNullOrWhiteSpace(destinationDirectory))
            {
                EnsureSafeInstallWritePath(
                    installPath,
                    destinationDirectory,
                    $"Package entry would write through an unsafe install folder: {installFile.RelativePath}");
            }

            if (Directory.Exists(installFile.DestinationPath))
            {
                throw new IOException($"Cannot install {installFile.RelativePath} because the destination path is a directory.");
            }

            if (File.Exists(installFile.DestinationPath))
            {
                EnsureTargetFileCanBeReplaced(installFile.DestinationPath, installFile.RelativePath);
            }
        }
    }

    private static void EnsureTargetFileCanBeReplaced(string destinationPath, string relativePath)
    {
        var originalAttributes = File.GetAttributes(destinationPath);
        if ((originalAttributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new IOException($"Cannot replace {relativePath} because the destination file is a reparse point.");
        }

        var changedAttributes = false;
        try
        {
            if ((originalAttributes & FileAttributes.ReadOnly) != 0)
            {
                File.SetAttributes(destinationPath, originalAttributes & ~FileAttributes.ReadOnly);
                changedAttributes = true;
            }

            using var stream = new FileStream(destinationPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            throw new IOException($"Cannot replace {relativePath}. Close Quake II or any tools using this file, then try again.", ex);
        }
        finally
        {
            if (changedAttributes)
            {
                RestoreDestinationAttributes(destinationPath, originalAttributes);
            }
        }
    }

    private static void EnsureSufficientInstallDiskSpace(string installPath, IReadOnlyList<PackageInstallFile> installPlan)
    {
        try
        {
            var root = Path.GetPathRoot(NormalizeFullPathForComparison(installPath));
            if (string.IsNullOrWhiteSpace(root))
            {
                return;
            }

            var drive = new DriveInfo(root);
            if (!drive.IsReady)
            {
                return;
            }

            var requiredBytes = MinimumInstallFreeSpaceHeadroomBytes;
            foreach (var installFile in installPlan)
            {
                requiredBytes = AddSaturating(requiredBytes, new FileInfo(installFile.SourcePath).Length);
            }

            requiredBytes = AddSaturating(requiredBytes, GetExistingGameDllLength(installPath));
            if (drive.AvailableFreeSpace < requiredBytes)
            {
                throw new IOException(
                    $"The install drive has {FormatByteCount(drive.AvailableFreeSpace)} free, but the updater needs about {FormatByteCount(requiredBytes)} for the package, backup, and working space.");
            }
        }
        catch (IOException)
        {
            throw;
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or UnauthorizedAccessException)
        {
            // Some virtual or removable locations do not expose drive statistics reliably.
        }
    }

    private static long GetExistingGameDllLength(string installPath)
    {
        var dllPath = Path.Combine(GetBaseq2Path(installPath), "game_x64.dll");
        try
        {
            return File.Exists(dllPath) && !IsReparsePoint(dllPath)
                ? new FileInfo(dllPath).Length
                : 0;
        }
        catch
        {
            return 0;
        }
    }

    private static string ComputeFileSha256(string path)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        var hash = SHA256.HashData(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static void ValidateGitHubAssetDigest(string? assetDigest, string packageSha256)
    {
        if (string.IsNullOrWhiteSpace(assetDigest))
        {
            throw new InvalidOperationException("GitHub release metadata did not include a package SHA-256 digest.");
        }

        const string sha256Prefix = "sha256:";
        if (!assetDigest.StartsWith(sha256Prefix, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("GitHub release metadata contained an unsupported asset digest format.");
        }

        var expected = assetDigest[sha256Prefix.Length..].Trim();
        if (expected.Length != 64 || expected.Any(value => !Uri.IsHexDigit(value)))
        {
            throw new InvalidOperationException("GitHub release metadata contained an invalid SHA-256 digest.");
        }

        if (!expected.Equals(packageSha256, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The downloaded release package did not match the SHA-256 digest published by GitHub.");
        }
    }

    private static long AddSaturating(long left, long right)
    {
        return long.MaxValue - left < right ? long.MaxValue : left + right;
    }

    private static string FormatByteCount(long bytes)
    {
        var units = new[] { "bytes", "KB", "MB", "GB" };
        var value = (double)bytes;
        var unitIndex = 0;
        while (value >= 1024 && unitIndex < units.Length - 1)
        {
            value /= 1024;
            unitIndex++;
        }

        return unitIndex == 0
            ? $"{bytes:N0} {units[unitIndex]}"
            : $"{value:N1} {units[unitIndex]}";
    }

    private static async Task ExtractReleasePackageAsync(
        string zipPath,
        string extractRoot,
        string expectedRootName,
        CancellationToken cancellationToken)
    {
        using var archive = ZipFile.OpenRead(zipPath);
        var extractedPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        long totalUncompressedBytes = 0;
        var entryCount = 0;
        var directoryCount = 0;
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
            ValidateArchiveEntryRootName(relativePath, expectedRootName, entry.FullName);
            var destinationPath = ResolvePathUnderRoot(
                extractRoot,
                relativePath,
                $"Archive entry would extract outside the temporary folder: {entry.FullName}");
            var destinationKey = NormalizeFullPathForComparison(destinationPath);
            if (!extractedPaths.Add(destinationKey))
            {
                throw new InvalidOperationException($"The release package contains duplicate or conflicting entries for: {relativePath}");
            }

            ValidateArchiveEntryKind(entry, relativePath);
            if (string.IsNullOrEmpty(entry.Name))
            {
                directoryCount++;
                if (directoryCount > MaxPackageDirectoryCount)
                {
                    throw new InvalidOperationException($"The release package contains more than the supported limit of {MaxPackageDirectoryCount:N0} directories.");
                }

                Directory.CreateDirectory(destinationPath);
                EnsureDirectoryPathHasNoReparsePoints(destinationPath, "temporary extraction folder");
                continue;
            }

            fileCount++;
            if (fileCount > MaxPackageFileCount)
            {
                throw new InvalidOperationException($"The release package contains more than the supported limit of {MaxPackageFileCount:N0} files.");
            }

            if (entry.Length == 0)
            {
                throw new InvalidOperationException($"The release package contains an empty file: {relativePath}");
            }

            if (entry.Length < 0 || entry.Length > MaxPackageUncompressedBytes - totalUncompressedBytes)
            {
                throw new InvalidOperationException($"The release package expands beyond the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
            }

            ValidateArchiveEntryCompression(entry, relativePath);
            ValidateArchiveEntryTimestamp(entry, relativePath);

            var destinationDirectory = Path.GetDirectoryName(destinationPath);
            if (!string.IsNullOrWhiteSpace(destinationDirectory))
            {
                Directory.CreateDirectory(destinationDirectory);
                EnsureDirectoryPathHasNoReparsePoints(destinationDirectory, "temporary extraction folder");
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
                break;
            }

            totalBytes += bytesRead;
            if (totalBytes > maxBytes)
            {
                throw new InvalidOperationException($"The release package expands beyond the supported limit of {MaxPackageUncompressedBytes / 1024 / 1024} MB.");
            }

            await destination.WriteAsync(buffer.AsMemory(0, bytesRead), cancellationToken);
        }

        destination.Flush(flushToDisk: true);
        if (IsReparsePoint(destinationPath))
        {
            throw new IOException($"The extracted release package file became a reparse point: {destinationPath}");
        }

        return totalBytes;
    }

    private static string NormalizeArchiveEntryPath(string entryPath)
    {
        if (string.IsNullOrWhiteSpace(entryPath))
        {
            throw new InvalidOperationException("The release package contains an empty archive entry.");
        }

        var normalized = entryPath.Replace('\\', '/');
        if (normalized.Length > MaxArchiveEntryPathCharacters)
        {
            throw new InvalidOperationException($"The release package contains an archive entry path longer than {MaxArchiveEntryPathCharacters:N0} characters.");
        }

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
            if (segment.Length > MaxArchivePathSegmentCharacters)
            {
                throw new InvalidOperationException($"The release package contains an archive entry segment longer than {MaxArchivePathSegmentCharacters:N0} characters: {entryPath}");
            }

            if (IsUnsafeArchivePathSegment(segment))
            {
                throw new InvalidOperationException($"The release package contains an unsafe archive entry: {entryPath}");
            }
        }

        return Path.Combine(segments);
    }

    private static void ValidateArchiveEntryRootName(string relativePath, string expectedRootName, string entryName)
    {
        var segments = relativePath.Split(
            new[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar },
            StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length == 0
            || string.IsNullOrWhiteSpace(expectedRootName)
            || !segments[0].Equals(expectedRootName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The release package entry is outside the expected package root {expectedRootName}: {entryName}");
        }
    }

    private static void ValidateArchiveEntryCompression(ZipArchiveEntry entry, string relativePath)
    {
        if (entry.Length <= 0)
        {
            return;
        }

        if (entry.CompressedLength <= 0)
        {
            throw new InvalidOperationException($"The release package entry has invalid compressed metadata: {relativePath}");
        }

        var maximumAllowedLength = entry.CompressedLength > long.MaxValue / MaxPackageCompressionRatio
            ? long.MaxValue
            : entry.CompressedLength * MaxPackageCompressionRatio;
        if (entry.Length > maximumAllowedLength)
        {
            throw new InvalidOperationException($"The release package entry has an unusually high compression ratio: {relativePath}");
        }
    }

    private static void ValidateArchiveEntryTimestamp(ZipArchiveEntry entry, string relativePath)
    {
        if (entry.LastWriteTime > DateTimeOffset.UtcNow + MetadataTimestampFutureTolerance)
        {
            throw new InvalidOperationException($"The release package entry has a future timestamp: {relativePath}");
        }
    }

    private static void ValidateArchiveEntryKind(ZipArchiveEntry entry, string relativePath)
    {
        var unixFileType = (entry.ExternalAttributes >> 16) & 0xF000;
        if (unixFileType is 0xA000 or 0x6000 or 0x2000)
        {
            throw new InvalidOperationException($"The release package contains an unsupported special file entry: {relativePath}");
        }

        var windowsAttributes = (FileAttributes)(entry.ExternalAttributes & 0xFFFF);
        if ((windowsAttributes & (FileAttributes.ReparsePoint | FileAttributes.Device)) != 0)
        {
            throw new InvalidOperationException($"The release package contains an unsupported Windows special file entry: {relativePath}");
        }
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

    private static void CreateDirectoryUnderInstallRoot(string installPath, string directoryPath, string failureMessage)
    {
        EnsureSafeInstallWritePath(installPath, directoryPath, failureMessage);
        Directory.CreateDirectory(directoryPath);
        EnsureSafeInstallWritePath(installPath, directoryPath, failureMessage);
    }

    private static void EnsureSafeInstallRoot(string installPath)
    {
        if (!Directory.Exists(installPath))
        {
            throw new InvalidOperationException("The selected Quake 2 installation folder could not be found.");
        }

        var unsafeDirectory = FindUnsafeInstallDirectory(installPath);
        if (unsafeDirectory is not null)
        {
            throw new InvalidOperationException($"The selected Quake 2 installation folder contains a reparse point and will not be modified automatically: {unsafeDirectory}");
        }
    }

    private static void EnsureSafeLaunchRoot(string installPath)
    {
        if (!Directory.Exists(installPath))
        {
            throw new InvalidOperationException("The selected Quake 2 installation folder could not be found.");
        }

        var unsafeDirectory = FindUnsafeInstallDirectory(installPath);
        if (unsafeDirectory is not null)
        {
            throw new InvalidOperationException($"The selected Quake 2 installation folder contains a reparse point and will not be launched automatically: {unsafeDirectory}");
        }
    }

    private static string? FindUnsafeInstallDirectory(string installPath)
    {
        foreach (var directory in new[]
        {
            installPath,
            Path.Combine(installPath, "rerelease"),
            GetBaseq2Path(installPath)
        })
        {
            if (Directory.Exists(directory) && IsReparsePoint(directory))
            {
                return directory;
            }
        }

        return null;
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

    private static void ValidatePeImage(string path, string description, ushort? expectedMachine)
    {
        try
        {
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, CopyBufferSize, FileOptions.SequentialScan);
            if (stream.Length < 64)
            {
                throw new InvalidOperationException($"The {description} is too small to be a Windows executable image.");
            }

            Span<byte> dosHeader = stackalloc byte[64];
            stream.ReadExactly(dosHeader);
            if (dosHeader[0] != (byte)'M' || dosHeader[1] != (byte)'Z')
            {
                throw new InvalidOperationException($"The {description} does not have a valid MZ header.");
            }

            var peHeaderOffset = BitConverter.ToInt32(dosHeader[0x3C..0x40]);
            if (peHeaderOffset < 64 || peHeaderOffset > stream.Length - 6)
            {
                throw new InvalidOperationException($"The {description} has an invalid PE header offset.");
            }

            stream.Position = peHeaderOffset;
            Span<byte> peHeader = stackalloc byte[6];
            stream.ReadExactly(peHeader);
            if (peHeader[0] != (byte)'P'
                || peHeader[1] != (byte)'E'
                || peHeader[2] != 0
                || peHeader[3] != 0)
            {
                throw new InvalidOperationException($"The {description} does not have a valid PE signature.");
            }

            var machine = BitConverter.ToUInt16(peHeader[4..6]);
            if (expectedMachine is { } requiredMachine && machine != requiredMachine)
            {
                throw new InvalidOperationException($"The {description} has machine type 0x{machine:X4}; expected 0x{requiredMachine:X4}.");
            }
        }
        catch (IOException)
        {
            throw;
        }
        catch (UnauthorizedAccessException)
        {
            throw;
        }
    }

    private static void ValidatePackageRootName(string packageRoot, ReleaseInfo release)
    {
        var actualRootName = Path.GetFileName(packageRoot.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        var expectedRootName = Path.GetFileNameWithoutExtension(release.AssetName);
        if (string.IsNullOrWhiteSpace(actualRootName)
            || string.IsNullOrWhiteSpace(expectedRootName)
            || !string.Equals(actualRootName, expectedRootName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The release package root folder must be named {expectedRootName}.");
        }
    }

    private static void ValidatePackageDirectories(string packageRoot)
    {
        var directoryCount = 0;
        foreach (var directoryPath in Directory.EnumerateDirectories(
            packageRoot,
            "*",
            new EnumerationOptions
            {
                RecurseSubdirectories = true,
                AttributesToSkip = 0,
                IgnoreInaccessible = false
            }))
        {
            directoryCount++;
            if (directoryCount > MaxPackageDirectoryCount)
            {
                throw new InvalidOperationException($"The release package contains more than the supported limit of {MaxPackageDirectoryCount:N0} directories.");
            }

            if (IsReparsePoint(directoryPath))
            {
                var relativePath = Path.GetRelativePath(packageRoot, directoryPath);
                throw new InvalidOperationException($"The release package contains a reparse point directory: {relativePath}");
            }

            var directoryRelativePath = Path.GetRelativePath(packageRoot, directoryPath);
            if (!IsAllowedPackageDirectoryPath(directoryRelativePath))
            {
                throw new InvalidOperationException($"The release package contains an unexpected directory path: {directoryRelativePath}");
            }
        }
    }

    private static InstalledVersionMarker ValidateReleasePackage(string packageRoot, ReleaseInfo release, string packageSha256)
    {
        ValidatePackageRootName(packageRoot, release);
        ValidatePackageDirectories(packageRoot);
        ValidateRequiredRootPackageFiles(packageRoot);
        ValidateRequiredPackageFiles(packageRoot);
        var packageBaseq2 = Path.Combine(packageRoot, "rerelease", "baseq2");
        var packageDll = Path.Combine(packageBaseq2, "game_x64.dll");
        var packageUpdater = Path.Combine(packageRoot, "MuffModeUpdater.exe");
        if (!File.Exists(packageDll))
        {
            throw new InvalidOperationException("The release package is missing rerelease\\baseq2\\game_x64.dll.");
        }

        if (IsReparsePoint(packageDll))
        {
            throw new InvalidOperationException("The release package game_x64.dll is a reparse point.");
        }

        if (new FileInfo(packageDll).Length == 0)
        {
            throw new InvalidOperationException("The release package game_x64.dll is empty.");
        }

        ValidatePeImage(packageDll, "release package game_x64.dll", ImageFileMachineAmd64);
        ValidatePeImage(packageUpdater, "release package MuffModeUpdater.exe", expectedMachine: null);

        if (TryReadDllFileVersion(packageDll, out var packageDllVersion)
            && packageDllVersion.CompareTo(release.Version) != 0)
        {
            throw new InvalidOperationException($"The release package game_x64.dll file version ({packageDllVersion}) does not match the GitHub release version ({release.Version}).");
        }

        var packageMarkerPath = Path.Combine(packageBaseq2, MarkerJsonFileName);
        if (!TryReadInstalledVersionMarker(packageMarkerPath, out var packageMarker)
            || !SemanticVersion.TryParse(packageMarker.Version, out var packageVersion))
        {
            throw new InvalidOperationException("The release package is missing a readable MuffMode JSON version marker.");
        }

        if (packageVersion.CompareTo(release.Version) != 0)
        {
            throw new InvalidOperationException($"The release package version ({packageVersion}) does not match the GitHub release version ({release.Version}).");
        }

        if (!string.Equals(packageMarker.Version, release.Version.ToString(), StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"The release package version marker must use the exact version text {release.Version}.");
        }

        ValidatePackageMarkerIsNotInstalledReceipt(packageMarker);
        ValidatePackageVersionMarkerMetadata(packageMarker, release, packageSha256);
        ValidatePackageVersionMarker(packageRoot, Path.Combine(packageBaseq2, MarkerTextFileName), release.Version);
        ValidatePackageVersionMarker(packageRoot, Path.Combine(packageRoot, "MuffMode.version"), release.Version);
        ValidatePackageVersionMarker(packageRoot, Path.Combine(packageRoot, "VERSION"), release.Version);

        var packageRelativePaths = EnumeratePackageFiles(packageRoot)
            .Select(filePath => Path.GetRelativePath(packageRoot, filePath))
            .ToList();
        foreach (var relativePath in packageRelativePaths)
        {
            if (!IsAllowedPackagePath(relativePath))
            {
                throw new InvalidOperationException($"The release package contains an unexpected file path: {relativePath}");
            }
        }

        ValidatePackageContentInventory(packageRelativePaths);
        return packageMarker;
    }

    private static void ValidatePackageVersionMarkerMetadata(InstalledVersionMarker packageMarker, ReleaseInfo release, string packageSha256)
    {
        ValidateRequiredPackageMarkerText(packageMarker.Version, "Version");
        ValidateRequiredPackageMarkerText(packageMarker.Repository, "Repository");
        ValidateRequiredPackageMarkerText(packageMarker.TagName, "TagName");
        ValidateRequiredPackageMarkerText(packageMarker.Channel, "Channel");
        ValidateRequiredPackageMarkerText(packageMarker.ReleaseUrl, "ReleaseUrl");
        ValidateRequiredPackageMarkerText(packageMarker.AssetName, "AssetName");
        ValidatePackageMarkerAssetName(packageMarker.AssetName);

        if (!string.Equals(packageMarker.Repository, GitHubReleaseClient.Repository, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker points at an unexpected repository.");
        }

        if (!string.Equals(packageMarker.TagName, release.TagName, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"The release package version marker tag ({packageMarker.TagName}) does not match the GitHub release tag ({release.TagName}).");
        }

        if (!string.Equals(packageMarker.Channel, release.Channel, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The release package version marker channel ({packageMarker.Channel}) does not match the GitHub asset channel ({release.Channel}).");
        }

        if (!string.Equals(packageMarker.ReleaseUrl, release.HtmlUrl, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker URL does not match the GitHub release URL.");
        }

        if (!string.Equals(packageMarker.AssetName, release.AssetName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The release package version marker asset ({packageMarker.AssetName}) does not match the GitHub asset ({release.AssetName}).");
        }

        ValidateOptionalPackageMarkerMetadata(packageMarker, release, packageSha256);

        if (packageMarker.PackagedAtUtc is not { } packagedAtUtc)
        {
            throw new InvalidOperationException("The release package version marker did not include a packaged timestamp.");
        }

        if (packagedAtUtc > DateTimeOffset.UtcNow + MetadataTimestampFutureTolerance)
        {
            throw new InvalidOperationException("The release package version marker packaged timestamp is in the future.");
        }

        if (release.AssetUpdatedAt is { } assetUpdatedAt
            && packagedAtUtc > assetUpdatedAt + MetadataTimestampFutureTolerance)
        {
            throw new InvalidOperationException("The release package version marker packaged timestamp is newer than the GitHub asset timestamp.");
        }
    }

    private static void ValidateOptionalPackageMarkerMetadata(InstalledVersionMarker packageMarker, ReleaseInfo release, string packageSha256)
    {
        if (packageMarker.ReleaseId > 0 && packageMarker.ReleaseId != release.ReleaseId)
        {
            throw new InvalidOperationException("The release package version marker release ID does not match GitHub metadata.");
        }

        if (packageMarker.AssetId > 0 && packageMarker.AssetId != release.AssetId)
        {
            throw new InvalidOperationException("The release package version marker asset ID does not match GitHub metadata.");
        }

        if (packageMarker.ReleasePublishedAt is { } releasePublishedAt
            && releasePublishedAt != release.PublishedAt)
        {
            throw new InvalidOperationException("The release package version marker published timestamp does not match GitHub metadata.");
        }

        if (packageMarker.AssetUpdatedAt is { } assetUpdatedAt
            && assetUpdatedAt != release.AssetUpdatedAt)
        {
            throw new InvalidOperationException("The release package version marker asset timestamp does not match GitHub metadata.");
        }

        if (packageMarker.AssetSize is > 0
            && packageMarker.AssetSize != release.AssetSize)
        {
            throw new InvalidOperationException("The release package version marker asset size does not match GitHub metadata.");
        }

        if (!string.IsNullOrWhiteSpace(packageMarker.AssetContentType)
            && !string.Equals(packageMarker.AssetContentType, release.AssetContentType, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker asset content type does not match GitHub metadata.");
        }

        if (!string.IsNullOrWhiteSpace(packageMarker.AssetDigest)
            && !string.Equals(packageMarker.AssetDigest, release.AssetDigest, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker asset digest does not match GitHub metadata.");
        }

        if (!string.IsNullOrWhiteSpace(packageMarker.PackageSha256)
            && !string.Equals(packageMarker.PackageSha256, packageSha256, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker package hash does not match the downloaded zip.");
        }

        if (!string.IsNullOrWhiteSpace(packageMarker.PackageRootName)
            && !string.Equals(packageMarker.PackageRootName, Path.GetFileNameWithoutExtension(release.AssetName), StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker root name does not match the GitHub asset.");
        }
    }

    private static void ValidatePackageMarkerIsNotInstalledReceipt(InstalledVersionMarker packageMarker)
    {
        if (packageMarker.InstalledAtUtc != default
            || packageMarker.InstalledFileCount != 0
            || packageMarker.InstalledBytes != 0
            || !string.IsNullOrWhiteSpace(packageMarker.BackupFileName)
            || !string.IsNullOrWhiteSpace(packageMarker.BackupGameDllSha256)
            || !string.IsNullOrWhiteSpace(packageMarker.InstalledGameDllSha256)
            || !string.IsNullOrWhiteSpace(packageMarker.InstalledManifestSha256)
            || !string.IsNullOrWhiteSpace(packageMarker.InstalledDestinationManifestSha256)
            || !string.IsNullOrWhiteSpace(packageMarker.InstalledByUpdaterVersion))
        {
            throw new InvalidOperationException("The release package version marker looked like an installed receipt instead of package metadata.");
        }
    }

    private static void ValidateRequiredPackageMarkerText(string? value, string propertyName)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            throw new InvalidOperationException($"The release package version marker is missing {propertyName}.");
        }
    }

    private static void ValidatePackageMarkerAssetName(string? assetName)
    {
        if (string.IsNullOrWhiteSpace(assetName)
            || assetName.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || !string.Equals(Path.GetFileName(assetName), assetName, StringComparison.Ordinal)
            || !assetName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package version marker contains an unsafe asset name.");
        }
    }

    private static void ValidateRequiredRootPackageFiles(string packageRoot)
    {
        foreach (var fileName in RequiredRootPackageFiles)
        {
            ValidateRequiredPackageFile(packageRoot, fileName, $"required root file {fileName}");
        }
    }

    private static void ValidateRequiredPackageFiles(string packageRoot)
    {
        foreach (var relativePath in RequiredPackageFiles)
        {
            ValidateRequiredPackageFile(packageRoot, relativePath, $"required file {relativePath}");
        }
    }

    private static void ValidateRequiredPackageFile(string packageRoot, string relativePath, string description)
    {
        var path = Path.Combine(packageRoot, relativePath);
        if (!File.Exists(path))
        {
            throw new InvalidOperationException($"The release package is missing {description}.");
        }

        if (IsReparsePoint(path) || new FileInfo(path).Length == 0)
        {
            throw new InvalidOperationException($"The release package file is not usable: {relativePath}.");
        }
    }

    private static void ValidatePackageContentInventory(IEnumerable<string> relativePaths)
    {
        var hasCompiledMuffModeMap = false;
        var hasMapEntityOverrides = false;
        var hasBotNavigation = false;
        var hasBaseq2Config = false;

        foreach (var relativePath in relativePaths)
        {
            var segments = relativePath.Split(
                new[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar },
                StringSplitOptions.RemoveEmptyEntries);
            if (segments.Length < 3 || !string.Equals(segments[0], "rerelease", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var extension = Path.GetExtension(relativePath);
            if (string.Equals(segments[1], "maps", StringComparison.OrdinalIgnoreCase))
            {
                hasCompiledMuffModeMap |= extension.Equals(".bsp", StringComparison.OrdinalIgnoreCase)
                    && segments[^1].StartsWith("mm-", StringComparison.OrdinalIgnoreCase);
                hasMapEntityOverrides |= extension.Equals(".ent", StringComparison.OrdinalIgnoreCase);
            }
            else if (string.Equals(segments[1], "bots", StringComparison.OrdinalIgnoreCase))
            {
                hasBotNavigation |= extension.Equals(".nav", StringComparison.OrdinalIgnoreCase);
            }
            else if (string.Equals(segments[1], "baseq2", StringComparison.OrdinalIgnoreCase))
            {
                hasBaseq2Config |= segments.Length == 3 && extension.Equals(".cfg", StringComparison.OrdinalIgnoreCase);
            }
        }

        if (!hasCompiledMuffModeMap)
        {
            throw new InvalidOperationException("The release package does not contain any compiled MuffMode maps under rerelease\\maps.");
        }

        if (!hasMapEntityOverrides)
        {
            throw new InvalidOperationException("The release package does not contain any map entity overrides under rerelease\\maps.");
        }

        if (!hasBotNavigation)
        {
            throw new InvalidOperationException("The release package does not contain any bot navigation files under rerelease\\bots.");
        }

        if (!hasBaseq2Config)
        {
            throw new InvalidOperationException("The release package does not contain any baseq2 configuration files.");
        }
    }

    private static void ValidatePackageVersionMarker(string packageRoot, string markerPath, SemanticVersion expectedVersion)
    {
        if (!File.Exists(markerPath))
        {
            return;
        }

        if (!TryReadSmallTextFile(markerPath, MaxDiscoveryFileBytes, out var text))
        {
            var relativePath = Path.GetRelativePath(packageRoot, markerPath);
            throw new InvalidOperationException($"The release package version marker is not readable: {relativePath}");
        }

        var markerText = text.Trim();
        if (!SemanticVersion.TryParse(markerText, out _)
            || !string.Equals(markerText, expectedVersion.ToString(), StringComparison.Ordinal))
        {
            var relativePath = Path.GetRelativePath(packageRoot, markerPath);
            throw new InvalidOperationException($"The release package version marker {relativePath} must contain the exact version text {expectedVersion}.");
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

        if (segments.Length < 3 || !AllowedRereleaseTopLevelDirectories.Contains(segments[1]))
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

        return IsAllowedRereleaseExtension(segments, extension);
    }

    private static bool IsAllowedPackageDirectoryPath(string relativePath)
    {
        var segments = relativePath.Split(
            new[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar },
            StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length == 0)
        {
            return true;
        }

        if (!string.Equals(segments[0], "rerelease", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return segments.Length == 1
            || AllowedRereleaseTopLevelDirectories.Contains(segments[1])
            || (segments.Length >= 3
                && string.Equals(segments[1], "baseq2", StringComparison.OrdinalIgnoreCase)
                && string.Equals(segments[2], "docs", StringComparison.OrdinalIgnoreCase));
    }

    private static bool IsAllowedRereleaseExtension(string[] segments, string extension)
    {
        if (string.Equals(segments[1], "maps", StringComparison.OrdinalIgnoreCase))
        {
            return extension.Equals(".bsp", StringComparison.OrdinalIgnoreCase)
                || extension.Equals(".ent", StringComparison.OrdinalIgnoreCase);
        }

        if (string.Equals(segments[1], "bots", StringComparison.OrdinalIgnoreCase))
        {
            return extension.Equals(".nav", StringComparison.OrdinalIgnoreCase);
        }

        if (string.Equals(segments[1], "baseq2", StringComparison.OrdinalIgnoreCase))
        {
            return extension.Equals(".cfg", StringComparison.OrdinalIgnoreCase)
                || extension.Equals(".json", StringComparison.OrdinalIgnoreCase)
                || extension.Equals(".md", StringComparison.OrdinalIgnoreCase)
                || extension.Equals(".txt", StringComparison.OrdinalIgnoreCase)
                || extension.Equals(".version", StringComparison.OrdinalIgnoreCase);
        }

        return false;
    }

    private static bool IsBlockedPackageExtension(string extension)
    {
        return extension.Equals(".appref-ms", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".bat", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".cmd", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".com", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".cpl", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".hta", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".jar", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".dll", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".exe", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".js", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".lnk", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".msc", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".msi", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".pif", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".ps1", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".psd1", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".psm1", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".reg", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".scr", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".sh", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".url", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".vb", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".vbs", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".ws", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".wsh", StringComparison.OrdinalIgnoreCase)
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

    private static bool IsPathUnderRoot(string rootPath, string candidatePath)
    {
        try
        {
            var root = NormalizeFullPathForComparison(rootPath);
            var candidate = NormalizeFullPathForComparison(candidatePath);
            var rootPrefix = EnsureTrailingDirectorySeparator(root);
            return string.Equals(candidate, root, StringComparison.OrdinalIgnoreCase)
                || candidate.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
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

    private static void CopyFileAtomically(string installPath, string sourcePath, string destinationPath, string relativePath)
    {
        ValidatePackageSourceFile(sourcePath, relativePath);
        var destinationDirectory = Path.GetDirectoryName(destinationPath);
        if (!string.IsNullOrWhiteSpace(destinationDirectory))
        {
            CreateDirectoryUnderInstallRoot(
                installPath,
                destinationDirectory,
                $"Package entry would create an unsafe install folder: {relativePath}");
        }

        var temporaryPath = Path.Combine(
            destinationDirectory ?? Path.GetTempPath(),
            $".{Path.GetFileName(destinationPath)}.{Guid.NewGuid():N}.tmp");
        EnsureSafeInstallWritePath(
            installPath,
            temporaryPath,
            $"Package entry would create an unsafe temporary install file: {relativePath}");
        var originalAttributes = PrepareDestinationForReplace(destinationPath);

        try
        {
            CopyFileContentsWithFlush(sourcePath, temporaryPath);
            VerifyCopiedFileLength(sourcePath, temporaryPath, "Copied package file length did not match the source file.");
            SetLastWriteTimeUtcBestEffort(temporaryPath, File.GetLastWriteTimeUtc(sourcePath));
            ReplaceTemporaryFile(temporaryPath, destinationPath);
            if (IsReparsePoint(destinationPath))
            {
                throw new IOException($"Installed package file became a reparse point: {relativePath}");
            }

            VerifyCopiedFileLength(sourcePath, destinationPath, "Installed package file length did not match the source file.");
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
        CopyFileContentsWithFlush(sourcePath, destinationPath);
        try
        {
            if (IsReparsePoint(destinationPath))
            {
                throw new IOException("Backup file became a reparse point.");
            }

            VerifyCopiedFileLength(sourcePath, destinationPath, "Backup file length did not match the source game DLL.");
            VerifyCopiedFileHash(sourcePath, destinationPath, Path.GetFileName(destinationPath));
        }
        catch
        {
            TryDeleteFile(destinationPath);
            throw;
        }
    }

    private static void CopyFileContentsWithFlush(string sourcePath, string destinationPath)
    {
        if (!File.Exists(sourcePath) || IsReparsePoint(sourcePath))
        {
            throw new IOException("The source file is not safe to copy.");
        }

        using var source = new FileStream(
            sourcePath,
            FileMode.Open,
            FileAccess.Read,
            FileShare.Read,
            CopyBufferSize,
            FileOptions.SequentialScan);
        using var destination = new FileStream(
            destinationPath,
            FileMode.CreateNew,
            FileAccess.Write,
            FileShare.None,
            CopyBufferSize,
            FileOptions.SequentialScan | FileOptions.WriteThrough);

        source.CopyTo(destination, CopyBufferSize);
        destination.Flush(flushToDisk: true);
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

    private static void VerifyInstallPlanApplied(IReadOnlyList<PackageInstallFile> installPlan)
    {
        foreach (var installFile in installPlan)
        {
            if (!File.Exists(installFile.DestinationPath))
            {
                throw new IOException($"Installed file is missing after update: {installFile.RelativePath}");
            }

            if (IsReparsePoint(installFile.DestinationPath))
            {
                throw new IOException($"Installed file became a reparse point: {installFile.RelativePath}");
            }

            VerifyCopiedFileLength(
                installFile.SourcePath,
                installFile.DestinationPath,
                $"Installed file verification failed for {installFile.RelativePath}.");
            VerifyCopiedFileHash(installFile.SourcePath, installFile.DestinationPath, installFile.RelativePath);
        }
    }

    private static void VerifyCopiedFileHash(string sourcePath, string destinationPath, string relativePath)
    {
        var sourceHash = ComputeFileSha256(sourcePath);
        var destinationHash = ComputeFileSha256(destinationPath);
        if (!sourceHash.Equals(destinationHash, StringComparison.OrdinalIgnoreCase))
        {
            throw new IOException($"Installed file hash verification failed for {relativePath}.");
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
            EnsureDirectoryPathHasNoReparsePoints(directory, "write target folder");
            Directory.CreateDirectory(directory);
            EnsureDirectoryPathHasNoReparsePoints(directory, "write target folder");
        }

        var temporaryPath = Path.Combine(
            directory ?? Path.GetTempPath(),
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        var originalAttributes = PrepareDestinationForReplace(path);

        try
        {
            WriteAllTextWithFlush(temporaryPath, contents);
            ReplaceTemporaryFile(temporaryPath, path);
            VerifyWrittenTextFile(path, contents);
        }
        catch
        {
            TryDeleteFile(temporaryPath);
            RestoreDestinationAttributes(path, originalAttributes);
            throw;
        }
    }

    private static void VerifyWrittenTextFile(string path, string expectedContents)
    {
        if (IsReparsePoint(path))
        {
            throw new IOException($"The written file became a reparse point: {path}");
        }

        var maxBytes = Math.Max(MaxVersionMarkerBytes, (long)StrictUtf8Encoding.GetByteCount(expectedContents) + 1);
        if (!TryReadSmallTextFile(path, maxBytes, out var actualContents)
            || !string.Equals(actualContents, expectedContents, StringComparison.Ordinal))
        {
            throw new IOException($"The written file could not be verified: {path}");
        }
    }

    private static void WriteAllTextWithFlush(string path, string contents)
    {
        using var destination = new FileStream(
            path,
            FileMode.CreateNew,
            FileAccess.Write,
            FileShare.None,
            CopyBufferSize,
            FileOptions.SequentialScan | FileOptions.WriteThrough);
        var bytes = StrictUtf8Encoding.GetBytes(contents);
        destination.Write(bytes);
        destination.Flush(flushToDisk: true);
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

    private static string? BackupCurrentGameDll(string installPath, IProgress<UpdaterProgress>? progress)
    {
        var baseq2 = GetBaseq2Path(installPath);
        var dllPath = Path.Combine(baseq2, "game_x64.dll");
        if (!File.Exists(dllPath))
        {
            return null;
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
        CreateDirectoryUnderInstallRoot(
            installPath,
            backupDirectory,
            "The MuffMode backup folder would write through an unsafe install path.");

        var timestamp = DateTimeOffset.Now.ToString("yyyyMMdd-HHmmss");
        var backupPath = GetUniqueBackupPath(backupDirectory, timestamp);
        CopyFileWithVerification(dllPath, backupPath);
        PruneOldGameDllBackups(backupDirectory);
        var backupFileName = Path.GetFileName(backupPath);
        progress?.Report(new UpdaterProgress($"Backed up existing game_x64.dll to {backupFileName}.", 49, CanCancel: false));
        return backupFileName;
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

    private static void PruneOldGameDllBackups(string backupDirectory)
    {
        List<FileInfo> backups;
        try
        {
            backups = Directory.EnumerateFiles(backupDirectory, "game_x64.before-muffmode-*.dll")
                .Where(path => !IsReparsePoint(path))
                .Select(path => new FileInfo(path))
                .OrderByDescending(file => file.LastWriteTimeUtc)
                .ToList();
        }
        catch
        {
            return;
        }

        foreach (var backup in backups.Skip(MaxGameDllBackups))
        {
            try
            {
                backup.Delete();
            }
            catch
            {
                // Backup retention cleanup failure is non-fatal.
            }
        }
    }

    private static void WriteInstalledMarker(
        string installPath,
        ReleaseInfo release,
        InstalledVersionMarker packageMarker,
        string packageSha256,
        IReadOnlyList<PackageInstallFile> installPlan,
        string? backupFileName)
    {
        var baseq2 = GetBaseq2Path(installPath);
        EnsureSafeInstallWritePath(
            installPath,
            Path.Combine(baseq2, MarkerJsonFileName),
            "The MuffMode version marker would write through an unsafe install path.");
        CreateDirectoryUnderInstallRoot(
            installPath,
            baseq2,
            "The MuffMode version marker would write through an unsafe install path.");

        var marker = new InstalledVersionMarker
        {
            Version = release.Version.ToString(),
            ReleaseId = release.ReleaseId,
            TagName = release.TagName,
            Channel = release.Channel,
            ReleaseUrl = release.HtmlUrl,
            ReleaseIsPrerelease = release.IsPrerelease,
            ReleasePublishedAt = release.PublishedAt,
            AssetId = release.AssetId,
            AssetName = release.AssetName,
            AssetSize = release.AssetSize,
            AssetContentType = release.AssetContentType,
            AssetUpdatedAt = release.AssetUpdatedAt,
            AssetDigest = release.AssetDigest,
            PackageRootName = Path.GetFileNameWithoutExtension(release.AssetName),
            BackupFileName = backupFileName,
            BackupGameDllSha256 = ComputeBackupGameDllSha256(installPath, backupFileName),
            PackageSha256 = packageSha256,
            PackagedAtUtc = packageMarker.PackagedAtUtc,
            InstalledGameDllSha256 = ComputeInstalledGameDllSha256(installPath),
            InstalledManifestSha256 = ComputeInstallPlanManifestSha256(installPlan),
            InstalledDestinationManifestSha256 = ComputeInstalledDestinationManifestSha256(installPlan),
            InstalledFileCount = installPlan.Count,
            InstalledBytes = GetInstallPlanTotalBytes(installPlan),
            Repository = GitHubReleaseClient.Repository,
            InstalledByUpdaterVersion = GetUpdaterVersion(),
            InstalledAtUtc = DateTimeOffset.UtcNow
        };

        WriteAllTextAtomic(Path.Combine(baseq2, MarkerJsonFileName), JsonSerializer.Serialize(marker, JsonOptions));
        WriteAllTextAtomic(Path.Combine(baseq2, MarkerTextFileName), release.Version.ToString());
    }

    private static void VerifyInstalledMarker(
        string installPath,
        ReleaseInfo release,
        InstalledVersionMarker packageMarker,
        string packageSha256,
        IReadOnlyList<PackageInstallFile> installPlan)
    {
        var baseq2 = GetBaseq2Path(installPath);
        var markerPath = Path.Combine(baseq2, MarkerJsonFileName);
        if (!TryReadInstalledVersionMarker(markerPath, out var marker)
            || !string.Equals(marker.Repository, GitHubReleaseClient.Repository, StringComparison.OrdinalIgnoreCase)
            || !SemanticVersion.TryParse(marker.Version, out var markerVersion)
            || markerVersion.CompareTo(release.Version) != 0)
        {
            throw new IOException($"The updater installed MuffMode {release.Version}, but the installed version marker could not be verified.");
        }

        ValidateInstalledMarkerSanity(marker, packageMarker);

        if (!string.Equals(marker.TagName, release.TagName, StringComparison.Ordinal)
            || marker.ReleaseId != release.ReleaseId
            || !string.Equals(marker.Channel, release.Channel, StringComparison.OrdinalIgnoreCase)
            || !string.Equals(marker.ReleaseUrl, release.HtmlUrl, StringComparison.OrdinalIgnoreCase)
            || marker.ReleaseIsPrerelease != release.IsPrerelease
            || marker.ReleasePublishedAt != release.PublishedAt
            || marker.AssetId != release.AssetId
            || !string.Equals(marker.AssetName, release.AssetName, StringComparison.OrdinalIgnoreCase)
            || marker.AssetSize != release.AssetSize
            || !string.Equals(marker.AssetContentType, release.AssetContentType, StringComparison.OrdinalIgnoreCase)
            || marker.AssetUpdatedAt != release.AssetUpdatedAt
            || !string.Equals(marker.AssetDigest, release.AssetDigest, StringComparison.OrdinalIgnoreCase)
            || !string.Equals(marker.PackageRootName, Path.GetFileNameWithoutExtension(release.AssetName), StringComparison.OrdinalIgnoreCase)
            || !string.Equals(marker.BackupGameDllSha256, ComputeBackupGameDllSha256(installPath, marker.BackupFileName), StringComparison.OrdinalIgnoreCase)
            || !string.Equals(marker.PackageSha256, packageSha256, StringComparison.OrdinalIgnoreCase)
            || marker.PackagedAtUtc != packageMarker.PackagedAtUtc
            || !string.Equals(marker.InstalledGameDllSha256, ComputeInstalledGameDllSha256(installPath), StringComparison.OrdinalIgnoreCase)
            || !string.Equals(marker.InstalledManifestSha256, ComputeInstallPlanManifestSha256(installPlan), StringComparison.OrdinalIgnoreCase)
            || !string.Equals(marker.InstalledDestinationManifestSha256, ComputeInstalledDestinationManifestSha256(installPlan), StringComparison.OrdinalIgnoreCase)
            || marker.InstalledFileCount != installPlan.Count
            || marker.InstalledBytes != GetInstallPlanTotalBytes(installPlan))
        {
            throw new IOException("The installed MuffMode version marker metadata did not match the completed install.");
        }

        if (!TryReadVersionTextFile(Path.Combine(baseq2, MarkerTextFileName), out var textMarkerVersion)
            || textMarkerVersion.CompareTo(release.Version) != 0
            || !TryReadSmallTextFile(Path.Combine(baseq2, MarkerTextFileName), MaxDiscoveryFileBytes, out var textMarker)
            || !string.Equals(textMarker.Trim(), release.Version.ToString(), StringComparison.Ordinal))
        {
            throw new IOException("The installed MuffMode text version marker could not be verified.");
        }
    }

    private static void ValidateInstalledMarkerSanity(InstalledVersionMarker marker, InstalledVersionMarker packageMarker)
    {
        if (marker.InstalledFileCount <= 0 || marker.InstalledBytes <= 0)
        {
            throw new IOException("The installed MuffMode version marker contained invalid install totals.");
        }

        if (string.IsNullOrWhiteSpace(marker.InstalledByUpdaterVersion))
        {
            throw new IOException("The installed MuffMode version marker did not include the updater version.");
        }

        if (!SemanticVersion.TryParse(marker.InstalledByUpdaterVersion, out _))
        {
            throw new IOException("The installed MuffMode version marker contained an invalid updater version.");
        }

        if (!string.Equals(marker.InstalledByUpdaterVersion, GetUpdaterVersion(), StringComparison.OrdinalIgnoreCase))
        {
            throw new IOException("The installed MuffMode version marker was not written by this updater build.");
        }

        if (marker.InstalledAtUtc == default)
        {
            throw new IOException("The installed MuffMode version marker did not include an install timestamp.");
        }

        if (marker.InstalledAtUtc > DateTimeOffset.UtcNow + MetadataTimestampFutureTolerance)
        {
            throw new IOException("The installed MuffMode version marker install timestamp is in the future.");
        }

        if (packageMarker.PackagedAtUtc is { } packagedAtUtc
            && marker.InstalledAtUtc < packagedAtUtc - MetadataTimestampFutureTolerance)
        {
            throw new IOException("The installed MuffMode version marker install timestamp predates the package timestamp.");
        }
    }

    private static string? ComputeBackupGameDllSha256(string installPath, string? backupFileName)
    {
        if (string.IsNullOrWhiteSpace(backupFileName))
        {
            return null;
        }

        if (!string.Equals(Path.GetFileName(backupFileName), backupFileName, StringComparison.Ordinal)
            || !backupFileName.StartsWith("game_x64.before-muffmode-", StringComparison.OrdinalIgnoreCase)
            || !backupFileName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
        {
            throw new IOException("The MuffMode backup marker contained an unsafe backup file name.");
        }

        var backupPath = Path.Combine(GetBaseq2Path(installPath), "MuffModeBackups", backupFileName);
        if (!File.Exists(backupPath) || IsReparsePoint(backupPath))
        {
            throw new IOException("The MuffMode backup file could not be hashed.");
        }

        return ComputeFileSha256(backupPath);
    }

    private static string ComputeInstalledGameDllSha256(string installPath)
    {
        var dllPath = Path.Combine(GetBaseq2Path(installPath), "game_x64.dll");
        if (!File.Exists(dllPath) || IsReparsePoint(dllPath))
        {
            throw new IOException("The installed game_x64.dll could not be hashed.");
        }

        return ComputeFileSha256(dllPath);
    }

    private static long GetInstallPlanTotalBytes(IReadOnlyList<PackageInstallFile> installPlan)
    {
        long totalBytes = 0;
        foreach (var installFile in installPlan)
        {
            totalBytes = AddSaturating(totalBytes, new FileInfo(installFile.SourcePath).Length);
        }

        return totalBytes;
    }

    private static string ComputeInstallPlanManifestSha256(IReadOnlyList<PackageInstallFile> installPlan)
    {
        var manifest = new StringBuilder();
        foreach (var installFile in installPlan.OrderBy(file => file.RelativePath, StringComparer.OrdinalIgnoreCase))
        {
            if (!File.Exists(installFile.SourcePath) || IsReparsePoint(installFile.SourcePath))
            {
                throw new IOException($"The install manifest could not hash package file: {installFile.RelativePath}");
            }

            var normalizedRelativePath = installFile.RelativePath
                .Replace(Path.DirectorySeparatorChar, '/')
                .Replace(Path.AltDirectorySeparatorChar, '/');
            manifest.Append(normalizedRelativePath);
            manifest.Append('\0');
            manifest.Append(new FileInfo(installFile.SourcePath).Length);
            manifest.Append('\0');
            manifest.Append(ComputeFileSha256(installFile.SourcePath));
            manifest.Append('\n');
        }

        return Convert.ToHexString(SHA256.HashData(StrictUtf8Encoding.GetBytes(manifest.ToString()))).ToLowerInvariant();
    }

    private static string ComputeInstalledDestinationManifestSha256(IReadOnlyList<PackageInstallFile> installPlan)
    {
        var manifest = new StringBuilder();
        foreach (var installFile in installPlan.OrderBy(file => file.RelativePath, StringComparer.OrdinalIgnoreCase))
        {
            if (!File.Exists(installFile.DestinationPath) || IsReparsePoint(installFile.DestinationPath))
            {
                throw new IOException($"The installed manifest could not hash installed file: {installFile.RelativePath}");
            }

            var normalizedRelativePath = installFile.RelativePath
                .Replace(Path.DirectorySeparatorChar, '/')
                .Replace(Path.AltDirectorySeparatorChar, '/');
            manifest.Append(normalizedRelativePath);
            manifest.Append('\0');
            manifest.Append(new FileInfo(installFile.DestinationPath).Length);
            manifest.Append('\0');
            manifest.Append(ComputeFileSha256(installFile.DestinationPath));
            manifest.Append('\n');
        }

        return Convert.ToHexString(SHA256.HashData(StrictUtf8Encoding.GetBytes(manifest.ToString()))).ToLowerInvariant();
    }

    private static string GetUpdaterVersion()
    {
        var version = Assembly.GetExecutingAssembly().GetName().Version;
        return version is null ? "unknown" : version.ToString(3);
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

    private static void PrepareUpdaterTempRoot(string updaterTempRoot)
    {
        if (File.Exists(updaterTempRoot))
        {
            throw new IOException("The updater temporary folder path points at a file.");
        }

        EnsureDirectoryPathHasNoReparsePoints(updaterTempRoot, "updater temporary folder");
        Directory.CreateDirectory(updaterTempRoot);
        EnsureDirectoryPathHasNoReparsePoints(updaterTempRoot, "updater temporary folder");
    }

    private static void EnsureDirectoryPathHasNoReparsePoints(string directoryPath, string description)
    {
        try
        {
            var current = Path.GetFullPath(directoryPath);
            while (!string.IsNullOrWhiteSpace(current))
            {
                if (Directory.Exists(current) && IsReparsePoint(current))
                {
                    throw new IOException($"The {description} contains a reparse point: {current}");
                }

                var parent = Directory.GetParent(current);
                if (parent is null)
                {
                    break;
                }

                current = parent.FullName;
            }
        }
        catch (IOException)
        {
            throw;
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
        {
            throw new IOException($"The {description} path was invalid.", ex);
        }
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
                    || DirectoryTreeContainsReparsePoint(extractionDirectory)
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
            if (Directory.Exists(path)
                && !IsReparsePoint(path)
                && !DirectoryTreeContainsReparsePoint(path))
            {
                Directory.Delete(path, recursive: true);
            }
        }
        catch
        {
            // Temp cleanup failure is non-fatal.
        }
    }

    private static bool DirectoryTreeContainsReparsePoint(string path)
    {
        try
        {
            foreach (var entry in Directory.EnumerateFileSystemEntries(
                path,
                "*",
                new EnumerationOptions
                {
                    RecurseSubdirectories = true,
                    AttributesToSkip = 0,
                    IgnoreInaccessible = false
                }))
            {
                if (IsReparsePoint(entry))
                {
                    return true;
                }
            }

            return false;
        }
        catch
        {
            return true;
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
