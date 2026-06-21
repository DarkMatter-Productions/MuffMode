using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;

namespace MuffMode.Updater;

internal sealed class GitHubReleaseClient : IDisposable
{
    public const string Repository = "DarkMatter-Productions/MuffMode";

    private const string ReleasesEndpoint = $"https://api.github.com/repos/{Repository}/releases?per_page=30";
    private const long MaxDownloadBytes = 1024L * 1024L * 1024L;
    private const long MaxReleaseMetadataBytes = 4L * 1024L * 1024L;
    private const int HttpErrorBodyLimit = 4096;
    private const int TransferBufferSize = 128 * 1024;
    private static readonly TimeSpan TemporaryDownloadMaxAge = TimeSpan.FromDays(2);
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
    private readonly HttpClient _httpClient;

    public GitHubReleaseClient()
    {
        _httpClient = new HttpClient
        {
            Timeout = TimeSpan.FromSeconds(60)
        };
        _httpClient.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("MuffModeUpdater", "0.1.0"));
        _httpClient.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
    }

    public async Task<ReleaseInfo> GetLatestReleaseAsync(CancellationToken cancellationToken)
    {
        using var response = await _httpClient.GetAsync(ReleasesEndpoint, cancellationToken);
        await EnsureSuccessStatusCodeAsync(response, "GitHub release lookup", cancellationToken);

        var releaseJson = await ReadBoundedContentStringAsync(
            response.Content,
            MaxReleaseMetadataBytes,
            "GitHub release metadata",
            cancellationToken);
        List<GitHubReleaseDto> releases;
        try
        {
            releases = JsonSerializer.Deserialize<List<GitHubReleaseDto>>(releaseJson) ?? [];
        }
        catch (JsonException ex)
        {
            throw new InvalidOperationException("GitHub release metadata was not valid JSON.", ex);
        }

        var release = releases
            .Where(candidate => !candidate.Draft)
            .Select(TryCreateReleaseInfo)
            .OfType<ReleaseInfo>()
            .OrderByDescending(candidate => candidate.PublishedAt ?? DateTimeOffset.MinValue)
            .ThenByDescending(candidate => candidate.Version)
            .FirstOrDefault()
            ?? throw new InvalidOperationException("No published MuffMode releases with a downloadable muffmode-<version>[-channel].zip package were found on GitHub.");

        return release;
    }

    public async Task<string> DownloadReleaseAssetAsync(
        ReleaseInfo release,
        string destinationDirectory,
        IProgress<UpdaterProgress>? progress,
        CancellationToken cancellationToken)
    {
        PrepareDownloadDirectory(destinationDirectory);
        CleanupOldDownloadFiles(destinationDirectory);
        var destinationFileName = GetSafeAssetFileName(release.AssetName);
        var destinationPath = Path.Combine(destinationDirectory, destinationFileName);
        var temporaryPath = Path.Combine(destinationDirectory, $".{destinationFileName}.{Guid.NewGuid():N}.download");
        var downloadUri = CreateDownloadUri(release.AssetDownloadUrl);

        progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", 0));

        try
        {
            using var response = await _httpClient.GetAsync(downloadUri, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
            await EnsureSuccessStatusCodeAsync(response, "Release package download", cancellationToken);

            var totalBytes = response.Content.Headers.ContentLength;
            if (totalBytes is > MaxDownloadBytes)
            {
                throw new InvalidOperationException($"The release package is larger than the supported limit of {MaxDownloadBytes / 1024 / 1024} MB.");
            }

            if (totalBytes is > 0
                && release.AssetSize is > 0
                && totalBytes.Value != release.AssetSize.Value)
            {
                throw new IOException($"The release package download size changed after release lookup: GitHub listed {release.AssetSize.Value:N0} bytes, but the download response listed {totalBytes.Value:N0} bytes.");
            }

            long totalRead = 0;
            await using (var source = await response.Content.ReadAsStreamAsync(cancellationToken))
            await using (var destination = new FileStream(
                temporaryPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                TransferBufferSize,
                FileOptions.Asynchronous | FileOptions.SequentialScan))
            {
                var buffer = new byte[TransferBufferSize];
                while (true)
                {
                    var bytesRead = await source.ReadAsync(buffer, cancellationToken);
                    if (bytesRead == 0)
                    {
                        break;
                    }

                    await destination.WriteAsync(buffer.AsMemory(0, bytesRead), cancellationToken);
                    totalRead += bytesRead;
                    if (totalRead > MaxDownloadBytes)
                    {
                        throw new InvalidOperationException($"The release package is larger than the supported limit of {MaxDownloadBytes / 1024 / 1024} MB.");
                    }

                    if (totalBytes is > 0)
                    {
                        var percentage = Math.Clamp((int)((double)totalRead / totalBytes.Value * 45), 0, 45);
                        progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", percentage));
                    }
                }
            }

            if (totalBytes is > 0 && totalRead != totalBytes.Value)
            {
                throw new IOException($"The release package download was incomplete: expected {totalBytes.Value:N0} bytes, received {totalRead:N0} bytes.");
            }

            if (totalRead == 0)
            {
                throw new InvalidOperationException("The release package download was empty.");
            }

            if (release.AssetSize is > 0 && totalRead != release.AssetSize.Value)
            {
                throw new IOException($"The release package download did not match the GitHub asset size: expected {release.AssetSize.Value:N0} bytes, received {totalRead:N0} bytes.");
            }

            VerifyFileLength(temporaryPath, totalRead, "downloaded release package");
            MoveFileIntoPlace(temporaryPath, destinationPath);
            progress?.Report(new UpdaterProgress("Download complete.", 45));
            return destinationPath;
        }
        catch
        {
            TryDeleteFile(temporaryPath);
            throw;
        }
    }

    public void Dispose() => _httpClient.Dispose();

    private static void PrepareDownloadDirectory(string destinationDirectory)
    {
        if (string.IsNullOrWhiteSpace(destinationDirectory))
        {
            throw new InvalidOperationException("The download folder path was empty.");
        }

        if (File.Exists(destinationDirectory))
        {
            throw new IOException("The download folder path points at a file.");
        }

        if (Directory.Exists(destinationDirectory) && IsReparsePoint(destinationDirectory))
        {
            throw new IOException("The download folder is a reparse point.");
        }

        Directory.CreateDirectory(destinationDirectory);
        if (IsReparsePoint(destinationDirectory))
        {
            throw new IOException("The download folder is a reparse point.");
        }
    }

    private static void CleanupOldDownloadFiles(string destinationDirectory)
    {
        IEnumerable<string> temporaryFiles;
        try
        {
            temporaryFiles = Directory.EnumerateFiles(destinationDirectory, ".muffmode-*.download").ToList();
        }
        catch
        {
            return;
        }

        var cutoffUtc = DateTime.UtcNow - TemporaryDownloadMaxAge;
        foreach (var temporaryFile in temporaryFiles)
        {
            try
            {
                var attributes = File.GetAttributes(temporaryFile);
                if ((attributes & FileAttributes.ReparsePoint) != 0
                    || File.GetLastWriteTimeUtc(temporaryFile) >= cutoffUtc)
                {
                    continue;
                }

                File.Delete(temporaryFile);
            }
            catch
            {
                // Stale temp cleanup failure is non-fatal.
            }
        }
    }

    private static async Task<string> ReadBoundedContentStringAsync(
        HttpContent content,
        long maxBytes,
        string description,
        CancellationToken cancellationToken)
    {
        var contentLength = content.Headers.ContentLength;
        if (contentLength is > 0 && contentLength.Value > maxBytes)
        {
            throw new InvalidOperationException($"{description} is larger than the supported limit of {maxBytes / 1024 / 1024} MB.");
        }

        await using var source = await content.ReadAsStreamAsync(cancellationToken);
        using var destination = new MemoryStream();
        var buffer = new byte[TransferBufferSize];
        long totalRead = 0;

        while (true)
        {
            var bytesRead = await source.ReadAsync(buffer.AsMemory(0, buffer.Length), cancellationToken);
            if (bytesRead == 0)
            {
                break;
            }

            totalRead += bytesRead;
            if (totalRead > maxBytes)
            {
                throw new InvalidOperationException($"{description} is larger than the supported limit of {maxBytes / 1024 / 1024} MB.");
            }

            destination.Write(buffer, 0, bytesRead);
        }

        if (totalRead == 0)
        {
            throw new InvalidOperationException($"{description} was empty.");
        }

        return Encoding.UTF8.GetString(destination.ToArray());
    }

    private static async Task EnsureSuccessStatusCodeAsync(
        HttpResponseMessage response,
        string operation,
        CancellationToken cancellationToken)
    {
        if (response.IsSuccessStatusCode)
        {
            return;
        }

        var detail = await ReadErrorBodySnippetAsync(response, cancellationToken);
        var message = $"{operation} failed with HTTP {(int)response.StatusCode} {response.ReasonPhrase}.";
        if (!string.IsNullOrWhiteSpace(detail))
        {
            message += $" Response: {detail}";
        }

        throw new HttpRequestException(message, null, response.StatusCode);
    }

    private static async Task<string> ReadErrorBodySnippetAsync(
        HttpResponseMessage response,
        CancellationToken cancellationToken)
    {
        try
        {
            await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
            var buffer = new byte[HttpErrorBodyLimit + 1];
            var totalRead = 0;

            while (totalRead < buffer.Length)
            {
                var bytesRead = await stream.ReadAsync(buffer.AsMemory(totalRead, buffer.Length - totalRead), cancellationToken);
                if (bytesRead == 0)
                {
                    break;
                }

                totalRead += bytesRead;
            }

            if (totalRead == 0)
            {
                return "";
            }

            var truncated = totalRead > HttpErrorBodyLimit;
            var text = Encoding.UTF8.GetString(buffer, 0, Math.Min(totalRead, HttpErrorBodyLimit)).Trim();
            return truncated ? $"{text}..." : text;
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            return "";
        }
    }

    private static void MoveFileIntoPlace(string temporaryPath, string destinationPath)
    {
        if (File.Exists(destinationPath) || Directory.Exists(destinationPath))
        {
            var attributes = File.GetAttributes(destinationPath);
            if ((attributes & FileAttributes.Directory) != 0)
            {
                throw new IOException($"The download destination is a directory: {destinationPath}");
            }

            if ((attributes & FileAttributes.ReparsePoint) != 0)
            {
                throw new IOException($"The download destination is a reparse point: {destinationPath}");
            }

            if ((attributes & FileAttributes.ReadOnly) != 0)
            {
                File.SetAttributes(destinationPath, attributes & ~FileAttributes.ReadOnly);
            }

            try
            {
                File.Replace(temporaryPath, destinationPath, destinationBackupFileName: null, ignoreMetadataErrors: true);
            }
            catch
            {
                RestoreFileAttributes(destinationPath, attributes);
                throw;
            }

            return;
        }

        File.Move(temporaryPath, destinationPath);
    }

    private static void RestoreFileAttributes(string path, FileAttributes attributes)
    {
        try
        {
            if (File.Exists(path))
            {
                File.SetAttributes(path, attributes);
            }
        }
        catch
        {
            // Attribute restoration is best-effort after a failed replace.
        }
    }

    private static void VerifyFileLength(string path, long expectedLength, string description)
    {
        var actualLength = new FileInfo(path).Length;
        if (actualLength != expectedLength)
        {
            throw new IOException($"The {description} length did not match the bytes received. Expected {expectedLength:N0} bytes, found {actualLength:N0} bytes.");
        }
    }

    private static bool IsReleasePackageZip(GitHubAssetDto asset)
    {
        if (string.IsNullOrWhiteSpace(asset.Name) || string.IsNullOrWhiteSpace(asset.BrowserDownloadUrl))
        {
            return false;
        }

        if (asset.Size is <= 0 or > MaxDownloadBytes)
        {
            return false;
        }

        var name = asset.Name!;
        return name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)
            && name.StartsWith("muffmode-", StringComparison.OrdinalIgnoreCase)
            && !name.Contains("installer", StringComparison.OrdinalIgnoreCase)
            && !name.Contains("source", StringComparison.OrdinalIgnoreCase)
            && SemanticVersion.TryParse(name, out _);
    }

    private static ReleaseInfo? TryCreateReleaseInfo(GitHubReleaseDto release)
    {
        var versionText = string.Join(" ", new[] { release.TagName, release.Name }.Where(text => !string.IsNullOrWhiteSpace(text)));
        if (!SemanticVersion.TryParse(versionText, out var version))
        {
            return null;
        }

        var asset = release.Assets
            .Where(IsReleasePackageZip)
            .Where(asset => AssetNameMatchesRelease(asset.Name!, version, release.Prerelease))
            .OrderBy(asset => asset.Name, StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault();
        if (asset is null)
        {
            return null;
        }

        return new ReleaseInfo(
            version,
            release.TagName ?? version.ToString(),
            string.IsNullOrWhiteSpace(release.Name) ? $"MuffMode v{version}" : release.Name!,
            release.Body ?? "",
            release.HtmlUrl ?? $"https://github.com/{Repository}/releases",
            release.Prerelease,
            release.PublishedAt,
            asset.Name!,
            asset.BrowserDownloadUrl!,
            asset.Size);
    }

    private static Uri CreateDownloadUri(string url)
    {
        if (!Uri.TryCreate(url, UriKind.Absolute, out var uri) || uri.Scheme != Uri.UriSchemeHttps)
        {
            throw new InvalidOperationException("The selected release asset did not provide a trusted HTTPS download URL.");
        }

        if (string.Equals(uri.Host, "github.com", StringComparison.OrdinalIgnoreCase))
        {
            EnsureExpectedGitHubReleasePath(uri);
            return uri;
        }

        if (!string.Equals(uri.Host, "objects.githubusercontent.com", StringComparison.OrdinalIgnoreCase)
            && !uri.Host.EndsWith(".githubusercontent.com", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The selected release asset points at an unexpected host: {uri.Host}");
        }

        return uri;
    }

    private static void EnsureExpectedGitHubReleasePath(Uri uri)
    {
        var expectedPrefix = $"/{Repository}/releases/download/";
        if (!uri.AbsolutePath.StartsWith(expectedPrefix, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The selected release asset does not point at this repository's release downloads.");
        }
    }

    private static string GetSafeAssetFileName(string assetName)
    {
        if (string.IsNullOrWhiteSpace(assetName)
            || assetName.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || !string.Equals(Path.GetFileName(assetName), assetName, StringComparison.Ordinal)
            || !assetName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)
            || assetName.EndsWith(' ')
            || assetName.EndsWith('.')
            || ReservedWindowsFileNames.Contains(Path.GetFileNameWithoutExtension(assetName)))
        {
            throw new InvalidOperationException("The selected release asset does not have a safe zip file name.");
        }

        return assetName;
    }

    private static bool AssetNameMatchesRelease(string assetName, SemanticVersion version, bool prerelease)
    {
        if (!HasVersionToken(assetName, version))
        {
            return false;
        }

        var hasPrereleaseToken = HasPrereleaseChannelToken(assetName);
        return prerelease ? hasPrereleaseToken : !hasPrereleaseToken;
    }

    private static bool HasVersionToken(string assetName, SemanticVersion version)
    {
        var name = Path.GetFileNameWithoutExtension(assetName);
        var expectedVersion = version.ToString();
        var searchIndex = 0;

        while (searchIndex < name.Length)
        {
            var index = name.IndexOf(expectedVersion, searchIndex, StringComparison.OrdinalIgnoreCase);
            if (index < 0)
            {
                return false;
            }

            var beforeIndex = index - 1;
            var afterIndex = index + expectedVersion.Length;
            var startsAtBoundary = IsVersionStartBoundary(name, beforeIndex);
            var endsAtBoundary = afterIndex >= name.Length || IsVersionDelimiter(name[afterIndex]);
            if (startsAtBoundary && endsAtBoundary)
            {
                return true;
            }

            searchIndex = index + 1;
        }

        return false;
    }

    private static bool IsVersionStartBoundary(string name, int beforeIndex)
    {
        if (beforeIndex < 0)
        {
            return true;
        }

        if (name[beforeIndex] is 'v' or 'V')
        {
            return beforeIndex == 0 || IsVersionDelimiter(name[beforeIndex - 1]);
        }

        return IsVersionDelimiter(name[beforeIndex]);
    }

    private static bool IsVersionDelimiter(char value)
    {
        return !char.IsLetterOrDigit(value) && value != '.';
    }

    private static bool HasPrereleaseChannelToken(string assetName)
    {
        foreach (var token in Path.GetFileNameWithoutExtension(assetName)
            .Split(['-', '_', '.'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            if (token.Equals("alpha", StringComparison.OrdinalIgnoreCase)
                || token.Equals("beta", StringComparison.OrdinalIgnoreCase)
                || token.Equals("preview", StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }

            if (token.Length >= 2
                && token.StartsWith("rc", StringComparison.OrdinalIgnoreCase)
                && (token.Length == 2 || token[2..].All(char.IsDigit)))
            {
                return true;
            }
        }

        return false;
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
}
