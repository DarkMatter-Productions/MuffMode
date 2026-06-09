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
        File.WriteAllText(SettingsPath, JsonSerializer.Serialize(settings, JsonOptions));
    }

    public static string? ResolveInitialInstallPath(string? savedPath)
    {
        var savedRoot = ResolveInstallRoot(savedPath);
        if (savedRoot is not null)
        {
            return savedRoot;
        }

        return EnumerateInstallCandidates().FirstOrDefault(IsValidInstallPath);
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
            throw new InvalidOperationException("Select the Quake 2 installation folder, or its rerelease folder. It must contain baseq2.");
        }

        var normalizedInstallPath = ResolveInstallRoot(installPath)!;
        var extractRoot = Path.Combine(Path.GetTempPath(), "MuffModeUpdater", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(extractRoot);

        try
        {
            progress?.Report(new UpdaterProgress("Extracting release package...", 46));
            await Task.Run(() => ZipFile.ExtractToDirectory(zipPath, extractRoot, overwriteFiles: true), cancellationToken);

            var packageRoot = ResolvePackageRoot(extractRoot);
            var releaseFiles = Directory.EnumerateFiles(packageRoot, "*", SearchOption.AllDirectories).ToList();
            if (releaseFiles.Count == 0)
            {
                throw new InvalidOperationException("The downloaded release package did not contain any files to install.");
            }

            var runningUpdaterPath = GetRunningUpdaterPath();
            BackupCurrentGameDll(normalizedInstallPath, progress);

            for (var index = 0; index < releaseFiles.Count; index++)
            {
                cancellationToken.ThrowIfCancellationRequested();

                var sourcePath = releaseFiles[index];
                var relativePath = Path.GetRelativePath(packageRoot, sourcePath);
                var destinationPath = Path.Combine(normalizedInstallPath, relativePath);
                if (IsRunningUpdaterDestination(destinationPath, runningUpdaterPath))
                {
                    progress?.Report(new UpdaterProgress($"Skipping running updater executable: {relativePath}.", null));
                    continue;
                }

                var destinationDirectory = Path.GetDirectoryName(destinationPath);
                if (!string.IsNullOrWhiteSpace(destinationDirectory))
                {
                    Directory.CreateDirectory(destinationDirectory);
                }

                if (File.Exists(destinationPath))
                {
                    var attributes = File.GetAttributes(destinationPath);
                    if ((attributes & FileAttributes.ReadOnly) != 0)
                    {
                        File.SetAttributes(destinationPath, attributes & ~FileAttributes.ReadOnly);
                    }
                }

                File.Copy(sourcePath, destinationPath, overwrite: true);
                File.SetLastWriteTimeUtc(destinationPath, File.GetLastWriteTimeUtc(sourcePath));

                var percentage = 50 + (int)Math.Round((double)(index + 1) / releaseFiles.Count * 45);
                progress?.Report(new UpdaterProgress($"Syncing {relativePath}...", Math.Clamp(percentage, 50, 95)));
            }

            WriteInstalledMarker(normalizedInstallPath, release);
            progress?.Report(new UpdaterProgress($"MuffMode {release.Version} installed.", 100));
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

    private static IEnumerable<string> EnumerateInstallCandidates()
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
        var backupPath = Path.Combine(backupDirectory, $"game_x64.before-muffmode-{timestamp}.dll");
        File.Copy(dllPath, backupPath, overwrite: false);
        progress?.Report(new UpdaterProgress($"Backed up existing game_x64.dll to {Path.GetFileName(backupPath)}.", 49));
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

        File.WriteAllText(Path.Combine(baseq2, MarkerJsonFileName), JsonSerializer.Serialize(marker, JsonOptions));
        File.WriteAllText(Path.Combine(baseq2, MarkerTextFileName), release.Version.ToString());
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

    [GeneratedRegex(@"""path""\s+""(?<path>[^""]+)""", RegexOptions.IgnoreCase)]
    private static partial Regex SteamLibraryPathRegex();
}
