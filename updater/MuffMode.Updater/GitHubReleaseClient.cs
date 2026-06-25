using System.Net;
using System.Net.Http.Headers;
using System.Reflection;
using System.Text;
using System.Text.Json;

namespace MuffMode.Updater;

internal sealed class GitHubReleaseClient : IDisposable
{
    public const string Repository = "DarkMatter-Productions/MuffMode";

    private const string ReleasesEndpoint = $"https://api.github.com/repos/{Repository}/releases?per_page=30";
    private const long MinDownloadBytes = 1024L * 1024L;
    private const long MaxDownloadBytes = 1024L * 1024L * 1024L;
    private const long MaxReleaseMetadataBytes = 4L * 1024L * 1024L;
    private const long MinimumDownloadFreeSpaceHeadroomBytes = 64L * 1024L * 1024L;
    private const int HttpErrorBodyLimit = 4096;
    private const int MaxHttpAttempts = 3;
    private const int MaxReleaseCount = 30;
    private const int MaxReleaseAssetCount = 100;
    private const int MaxAssetFileNameCharacters = 128;
    private const int TransferBufferSize = 128 * 1024;
    private static readonly TimeSpan TemporaryDownloadMaxAge = TimeSpan.FromDays(2);
    private static readonly TimeSpan HttpRetryBaseDelay = TimeSpan.FromMilliseconds(750);
    private static readonly TimeSpan MetadataTimestampFutureTolerance = TimeSpan.FromMinutes(10);
    private static readonly Encoding StrictUtf8Encoding = new UTF8Encoding(
        encoderShouldEmitUTF8Identifier: false,
        throwOnInvalidBytes: true);
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
        _httpClient.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("MuffModeUpdater", GetUpdaterVersion()));
        _httpClient.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        _httpClient.DefaultRequestHeaders.Add("X-GitHub-Api-Version", "2022-11-28");
    }

    public async Task<ReleaseInfo> GetLatestReleaseAsync(bool includePrereleases, CancellationToken cancellationToken)
    {
        using var response = await GetWithRetryAsync(
            new Uri(ReleasesEndpoint),
            HttpCompletionOption.ResponseContentRead,
            cancellationToken);
        await EnsureSuccessStatusCodeAsync(response, "GitHub release lookup", cancellationToken);
        EnsureExpectedStatusCode(response, HttpStatusCode.OK, "GitHub release lookup");
        ValidateReleaseMetadataResponseUri(response);
        ValidateReleaseMetadataContentHeaders(response);

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

        if (releases.Count > MaxReleaseCount)
        {
            throw new InvalidOperationException($"GitHub returned more than {MaxReleaseCount:N0} releases for a bounded release lookup.");
        }

        var candidates = releases
            .Where(candidate => !candidate.Draft)
            .Select(TryCreateReleaseInfo)
            .OfType<ReleaseInfo>()
            .Where(candidate => includePrereleases || !candidate.IsPrerelease)
            .ToList();

        RejectDuplicateReleaseCandidates(candidates);

        var release = candidates
            .OrderByDescending(candidate => candidate.Version)
            .ThenByDescending(candidate => candidate.PublishedAt ?? DateTimeOffset.MinValue)
            .FirstOrDefault()
            ?? throw new InvalidOperationException(includePrereleases
                ? "No published MuffMode releases with a downloadable muffmode-<version>[-channel].zip package were found on GitHub."
                : "No stable MuffMode releases with a downloadable muffmode-<version>.zip package were found on GitHub.");

        return release;
    }

    public async Task<string> DownloadReleaseAssetAsync(
        ReleaseInfo release,
        string destinationDirectory,
        IProgress<UpdaterProgress>? progress,
        CancellationToken cancellationToken)
    {
        destinationDirectory = PrepareDownloadDirectory(destinationDirectory);
        CleanupOldDownloadFiles(destinationDirectory);
        EnsureSufficientDownloadDiskSpace(destinationDirectory, release.AssetSize);
        var destinationFileName = GetSafeAssetFileName(release.AssetName);
        var destinationPath = Path.Combine(destinationDirectory, destinationFileName);
        var temporaryPath = Path.Combine(destinationDirectory, $".{destinationFileName}.{Guid.NewGuid():N}.download");
        var downloadUri = CreateDownloadUri(release.AssetDownloadUrl, release.AssetName, release.TagName);

        progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", 0));

        try
        {
            using var response = await GetWithRetryAsync(
                downloadUri,
                HttpCompletionOption.ResponseHeadersRead,
                cancellationToken);
            await EnsureSuccessStatusCodeAsync(response, "Release package download", cancellationToken);
            EnsureExpectedStatusCode(response, HttpStatusCode.OK, "Release package download");
            ValidateFinalDownloadUri(response);
            ValidateDownloadContentHeaders(response);
            ValidateDownloadContentDisposition(response, release.AssetName);

            var totalBytes = response.Content.Headers.ContentLength;
            if (totalBytes is null)
            {
                throw new InvalidOperationException("The release package download did not include a Content-Length header.");
            }

            var expectedBytes = totalBytes.Value;
            if (expectedBytes < MinDownloadBytes)
            {
                throw new InvalidOperationException($"The release package is smaller than the supported minimum of {MinDownloadBytes / 1024 / 1024} MB.");
            }

            if (expectedBytes > MaxDownloadBytes)
            {
                throw new InvalidOperationException($"The release package is larger than the supported limit of {MaxDownloadBytes / 1024 / 1024} MB.");
            }

            if (release.AssetSize is > 0
                && expectedBytes != release.AssetSize.Value)
            {
                throw new IOException($"The release package download size changed after release lookup: GitHub listed {release.AssetSize.Value:N0} bytes, but the download response listed {expectedBytes:N0} bytes.");
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

                    var percentage = Math.Clamp((int)((double)totalRead / expectedBytes * 45), 0, 45);
                    progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", percentage));
                }

                destination.Flush(flushToDisk: true);
            }

            if (totalRead != expectedBytes)
            {
                throw new IOException($"The release package download was incomplete: expected {expectedBytes:N0} bytes, received {totalRead:N0} bytes.");
            }

            if (totalRead == 0)
            {
                throw new InvalidOperationException("The release package download was empty.");
            }

            if (totalRead < MinDownloadBytes)
            {
                throw new InvalidOperationException($"The release package is smaller than the supported minimum of {MinDownloadBytes / 1024 / 1024} MB.");
            }

            if (release.AssetSize is > 0 && totalRead != release.AssetSize.Value)
            {
                throw new IOException($"The release package download did not match the GitHub asset size: expected {release.AssetSize.Value:N0} bytes, received {totalRead:N0} bytes.");
            }

            if (IsReparsePoint(temporaryPath))
            {
                throw new IOException("The downloaded release package temporary file became a reparse point.");
            }

            VerifyFileLength(temporaryPath, totalRead, "downloaded release package");
            VerifyZipSignature(temporaryPath);
            MoveFileIntoPlace(temporaryPath, destinationPath);
            VerifyFileLength(destinationPath, totalRead, "downloaded release package after move");
            if (IsReparsePoint(destinationPath))
            {
                throw new IOException("The downloaded release package became a reparse point after it was moved into place.");
            }

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

    private static string GetUpdaterVersion()
    {
        var version = Assembly.GetExecutingAssembly().GetName().Version;
        return version is null ? "0.1.0" : version.ToString(3);
    }

    private async Task<HttpResponseMessage> GetWithRetryAsync(
        Uri requestUri,
        HttpCompletionOption completionOption,
        CancellationToken cancellationToken)
    {
        for (var attempt = 1; attempt <= MaxHttpAttempts; attempt++)
        {
            HttpResponseMessage? response = null;
            try
            {
                response = await _httpClient.GetAsync(requestUri, completionOption, cancellationToken);
                if (attempt >= MaxHttpAttempts || !IsTransientStatusCode(response.StatusCode))
                {
                    return response;
                }

                var retryDelay = GetRetryDelay(response, attempt);
                response.Dispose();
                response = null;
                await Task.Delay(retryDelay, cancellationToken);
            }
            catch (Exception ex) when (attempt < MaxHttpAttempts && IsTransientHttpException(ex, cancellationToken))
            {
                response?.Dispose();
                await Task.Delay(GetRetryDelay(null, attempt), cancellationToken);
            }
        }

        throw new InvalidOperationException("HTTP retry loop ended unexpectedly.");
    }

    private static string PrepareDownloadDirectory(string destinationDirectory)
    {
        if (string.IsNullOrWhiteSpace(destinationDirectory))
        {
            throw new InvalidOperationException("The download folder path was empty.");
        }

        if (!Path.IsPathFullyQualified(destinationDirectory))
        {
            throw new IOException("The download folder path must be fully qualified.");
        }

        string fullDestinationDirectory;
        try
        {
            fullDestinationDirectory = Path.GetFullPath(destinationDirectory);
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException or PathTooLongException)
        {
            throw new IOException("The download folder path was invalid.", ex);
        }

        if (File.Exists(fullDestinationDirectory))
        {
            throw new IOException("The download folder path points at a file.");
        }

        EnsureDirectoryPathHasNoReparsePoints(fullDestinationDirectory, "download folder");
        Directory.CreateDirectory(fullDestinationDirectory);
        EnsureDirectoryPathHasNoReparsePoints(fullDestinationDirectory, "download folder");
        return fullDestinationDirectory;
    }

    private static void EnsureSufficientDownloadDiskSpace(string destinationDirectory, long? assetSize)
    {
        if (assetSize is not > 0)
        {
            return;
        }

        try
        {
            var root = Path.GetPathRoot(Path.GetFullPath(destinationDirectory));
            if (string.IsNullOrWhiteSpace(root))
            {
                return;
            }

            var drive = new DriveInfo(root);
            if (!drive.IsReady)
            {
                return;
            }

            var requiredBytes = AddSaturating(assetSize.Value, MinimumDownloadFreeSpaceHeadroomBytes);
            if (drive.AvailableFreeSpace < requiredBytes)
            {
                throw new IOException(
                    $"The download drive has {FormatByteCount(drive.AvailableFreeSpace)} free, but the updater needs about {FormatByteCount(requiredBytes)} for the release package and working space.");
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

    private static void CleanupOldDownloadFiles(string destinationDirectory)
    {
        var cutoffUtc = DateTime.UtcNow - TemporaryDownloadMaxAge;
        foreach (var pattern in new[] { ".muffmode-*.download", "muffmode-*.zip" })
        {
            IEnumerable<string> files;
            try
            {
                files = Directory.EnumerateFiles(destinationDirectory, pattern).ToList();
            }
            catch
            {
                continue;
            }

            foreach (var file in files)
            {
                TryDeleteOldDownloadFile(file, cutoffUtc);
            }
        }
    }

    private static void TryDeleteOldDownloadFile(string path, DateTime cutoffUtc)
    {
        try
        {
            var attributes = File.GetAttributes(path);
            if ((attributes & FileAttributes.ReparsePoint) != 0
                || File.GetLastWriteTimeUtc(path) >= cutoffUtc)
            {
                return;
            }

            File.Delete(path);
        }
        catch
        {
            // Stale download cleanup failure is non-fatal.
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

        return StrictUtf8Encoding.GetString(destination.ToArray());
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
        var rateLimitDetail = GetGitHubRateLimitDetail(response);
        if (!string.IsNullOrWhiteSpace(rateLimitDetail))
        {
            message += $" {rateLimitDetail}";
        }

        if (!string.IsNullOrWhiteSpace(detail))
        {
            message += $" Response: {detail}";
        }

        throw new HttpRequestException(message, null, response.StatusCode);
    }

    private static void EnsureExpectedStatusCode(HttpResponseMessage response, HttpStatusCode expectedStatusCode, string operation)
    {
        if (response.StatusCode != expectedStatusCode)
        {
            throw new InvalidOperationException($"{operation} returned HTTP {(int)response.StatusCode}, but the updater expected {(int)expectedStatusCode}.");
        }
    }

    private static void ValidateReleaseMetadataResponseUri(HttpResponseMessage response)
    {
        var finalUri = response.RequestMessage?.RequestUri
            ?? throw new InvalidOperationException("GitHub release lookup did not expose its final response URL.");
        if (finalUri.Scheme != Uri.UriSchemeHttps
            || !finalUri.Host.Equals("api.github.com", StringComparison.OrdinalIgnoreCase)
            || !string.IsNullOrEmpty(finalUri.UserInfo)
            || !finalUri.IsDefaultPort
            || !finalUri.AbsolutePath.Equals($"/repos/{Repository}/releases", StringComparison.OrdinalIgnoreCase)
            || !finalUri.Query.Equals("?per_page=30", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("GitHub release lookup returned metadata from an unexpected URL.");
        }
    }

    private static void ValidateReleaseMetadataContentHeaders(HttpResponseMessage response)
    {
        if (response.Content.Headers.ContentEncoding.Any())
        {
            throw new InvalidOperationException("GitHub release metadata used unexpected HTTP content encoding.");
        }

        if (response.Content.Headers.ContentRange is not null)
        {
            throw new InvalidOperationException("GitHub release metadata returned a partial content range.");
        }

        var mediaType = response.Content.Headers.ContentType?.MediaType;
        if (string.IsNullOrWhiteSpace(mediaType)
            || (!mediaType.Equals("application/json", StringComparison.OrdinalIgnoreCase)
                && !mediaType.Equals("application/vnd.github+json", StringComparison.OrdinalIgnoreCase)))
        {
            throw new InvalidOperationException($"GitHub release metadata returned an unexpected content type: {mediaType ?? "none"}.");
        }
    }

    private static void ValidateFinalDownloadUri(HttpResponseMessage response)
    {
        var finalUri = response.RequestMessage?.RequestUri
            ?? throw new InvalidOperationException("The release package download did not expose its final response URL.");
        if (finalUri.Scheme != Uri.UriSchemeHttps)
        {
            throw new InvalidOperationException("The release package download redirected away from HTTPS.");
        }

        if (!string.IsNullOrEmpty(finalUri.UserInfo) || !finalUri.IsDefaultPort)
        {
            throw new InvalidOperationException("The release package download redirected to an unexpected URL authority.");
        }

        if (finalUri.Host.Equals("github.com", StringComparison.OrdinalIgnoreCase)
            || finalUri.Host.Equals("objects.githubusercontent.com", StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        throw new InvalidOperationException($"The release package download redirected to an unexpected host: {finalUri.Host}");
    }

    private static void ValidateDownloadContentHeaders(HttpResponseMessage response)
    {
        if (response.Content.Headers.ContentEncoding.Any())
        {
            throw new InvalidOperationException("The release package download used unexpected HTTP content encoding.");
        }

        if (response.Content.Headers.ContentRange is not null)
        {
            throw new InvalidOperationException("The release package download returned a partial content range.");
        }

        var mediaType = response.Content.Headers.ContentType?.MediaType;
        if (string.IsNullOrWhiteSpace(mediaType))
        {
            throw new InvalidOperationException("The release package download did not include a Content-Type header.");
        }

        if (IsAcceptablePackageContentType(mediaType))
        {
            return;
        }

        if (mediaType.StartsWith("text/", StringComparison.OrdinalIgnoreCase)
            || mediaType.Equals("application/json", StringComparison.OrdinalIgnoreCase)
            || mediaType.Equals("application/problem+json", StringComparison.OrdinalIgnoreCase)
            || mediaType.Equals("text/html", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The release package download returned {mediaType} instead of a zip file.");
        }

        throw new InvalidOperationException($"The release package download returned an unexpected content type: {mediaType}.");
    }

    private static void ValidateDownloadContentDisposition(HttpResponseMessage response, string expectedAssetName)
    {
        var contentDisposition = response.Content.Headers.ContentDisposition;
        var fileName = contentDisposition?.FileNameStar ?? contentDisposition?.FileName;
        if (string.IsNullOrWhiteSpace(fileName))
        {
            return;
        }

        var actualFileName = fileName.Trim().Trim('"');
        if (string.IsNullOrWhiteSpace(actualFileName)
            || actualFileName.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || !string.Equals(Path.GetFileName(actualFileName), actualFileName, StringComparison.Ordinal)
            || !actualFileName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The release package download returned an unsafe file name.");
        }

        if (!actualFileName.Equals(expectedAssetName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The release package download file name was {actualFileName}, but GitHub advertised {expectedAssetName}.");
        }
    }

    private static bool IsTransientStatusCode(HttpStatusCode statusCode)
    {
        return statusCode is HttpStatusCode.RequestTimeout
            or HttpStatusCode.TooManyRequests
            or HttpStatusCode.InternalServerError
            or HttpStatusCode.BadGateway
            or HttpStatusCode.ServiceUnavailable
            or HttpStatusCode.GatewayTimeout;
    }

    private static bool IsTransientHttpException(Exception exception, CancellationToken cancellationToken)
    {
        if (exception is OperationCanceledException && cancellationToken.IsCancellationRequested)
        {
            return false;
        }

        return exception is HttpRequestException or TaskCanceledException;
    }

    private static TimeSpan GetRetryDelay(HttpResponseMessage? response, int attempt)
    {
        if (response?.Headers.RetryAfter is { } retryAfter)
        {
            if (retryAfter.Delta is { } delta && delta > TimeSpan.Zero)
            {
                return ClampRetryDelay(delta);
            }

            if (retryAfter.Date is { } date)
            {
                var delay = date - DateTimeOffset.UtcNow;
                if (delay > TimeSpan.Zero)
                {
                    return ClampRetryDelay(delay);
                }
            }
        }

        var exponentialMilliseconds = HttpRetryBaseDelay.TotalMilliseconds * Math.Pow(2, attempt - 1);
        return TimeSpan.FromMilliseconds(exponentialMilliseconds);
    }

    private static TimeSpan ClampRetryDelay(TimeSpan delay)
    {
        var maxDelay = TimeSpan.FromSeconds(10);
        return delay <= maxDelay ? delay : maxDelay;
    }

    private static string GetGitHubRateLimitDetail(HttpResponseMessage response)
    {
        if (!response.Headers.TryGetValues("X-RateLimit-Remaining", out var remainingValues)
            || !remainingValues.Any(value => string.Equals(value, "0", StringComparison.Ordinal)))
        {
            return "";
        }

        if (response.Headers.TryGetValues("X-RateLimit-Reset", out var resetValues)
            && long.TryParse(resetValues.FirstOrDefault(), out var resetUnixSeconds))
        {
            try
            {
                var resetAt = DateTimeOffset.FromUnixTimeSeconds(resetUnixSeconds).ToLocalTime();
                return $"GitHub API rate limit is exhausted until {resetAt:yyyy-MM-dd HH:mm:ss zzz}.";
            }
            catch (ArgumentOutOfRangeException)
            {
                // Fall through to the generic rate-limit message.
            }
        }

        return "GitHub API rate limit is exhausted. Try again later.";
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
            var text = StrictUtf8Encoding.GetString(buffer, 0, Math.Min(totalRead, HttpErrorBodyLimit)).Trim();
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

    private static void VerifyZipSignature(string path)
    {
        Span<byte> signature = stackalloc byte[4];
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        if (stream.Read(signature) != signature.Length
            || signature[0] != 0x50
            || signature[1] != 0x4B
            || !((signature[2] == 0x03 && signature[3] == 0x04)
                || (signature[2] == 0x05 && signature[3] == 0x06)
                || (signature[2] == 0x07 && signature[3] == 0x08)))
        {
            throw new InvalidOperationException("The downloaded release package did not look like a valid zip file.");
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

    private static bool IsReleasePackageZip(GitHubAssetDto asset)
    {
        if (string.IsNullOrWhiteSpace(asset.Name) || string.IsNullOrWhiteSpace(asset.BrowserDownloadUrl))
        {
            return false;
        }

        if (asset.Size is null or < MinDownloadBytes or > MaxDownloadBytes)
        {
            return false;
        }

        if (!string.IsNullOrWhiteSpace(asset.State)
            && !asset.State.Equals("uploaded", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        if (string.IsNullOrWhiteSpace(asset.ContentType)
            || !IsAcceptablePackageContentType(asset.ContentType))
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

    private static bool IsAcceptablePackageContentType(string? contentType)
    {
        if (string.IsNullOrWhiteSpace(contentType))
        {
            return false;
        }

        return contentType.Equals("application/zip", StringComparison.OrdinalIgnoreCase)
            || contentType.Equals("application/x-zip-compressed", StringComparison.OrdinalIgnoreCase)
            || contentType.Equals("application/octet-stream", StringComparison.OrdinalIgnoreCase);
    }

    private static ReleaseInfo? TryCreateReleaseInfo(GitHubReleaseDto release)
    {
        var tagVersion = TryParseOptionalReleaseVersion(release.TagName);
        var nameVersion = TryParseOptionalReleaseVersion(release.Name);
        if (tagVersion is not null
            && nameVersion is not null
            && tagVersion.Value.CompareTo(nameVersion.Value) != 0)
        {
            throw new InvalidOperationException($"Release tag {release.TagName} and title {release.Name} disagree on the version.");
        }

        var version = tagVersion ?? nameVersion;
        if (version is null)
        {
            return null;
        }

        var releaseId = GetRequiredGitHubId(release.Id, "release");
        var releaseTag = ValidateReleaseTag(release.TagName, version.Value);
        if (release.PublishedAt is null)
        {
            throw new InvalidOperationException($"GitHub release {releaseTag} did not include a published timestamp.");
        }

        ValidateMetadataTimestamp(release.PublishedAt.Value, $"GitHub release {releaseTag} published timestamp");

        var assets = release.Assets
            ?? throw new InvalidOperationException($"GitHub release {releaseTag} did not include an asset list.");
        if (assets.Count > MaxReleaseAssetCount)
        {
            throw new InvalidOperationException($"GitHub release {releaseTag} listed more than {MaxReleaseAssetCount:N0} assets.");
        }

        var matchingAssets = assets
            .Where(IsReleasePackageZip)
            .Where(asset => AssetNameMatchesRelease(asset.Name!, version.Value, release.Prerelease))
            .OrderBy(asset => asset.Name, StringComparer.OrdinalIgnoreCase)
            .ToList();
        if (matchingAssets.Count == 0)
        {
            return null;
        }

        if (matchingAssets.Count > 1)
        {
            throw new InvalidOperationException($"Release {version.Value} has multiple matching MuffMode package zip assets. Keep exactly one package asset for this channel.");
        }

        var asset = matchingAssets[0];
        _ = GetSafeAssetFileName(asset.Name!);
        var downloadUri = CreateDownloadUri(asset.BrowserDownloadUrl!, asset.Name!, releaseTag);
        var assetId = GetRequiredGitHubId(asset.Id, "release asset");
        var assetChannel = GetAssetChannel(asset.Name!, version.Value)
            ?? throw new InvalidOperationException($"GitHub release {releaseTag} had an unexpected package asset name: {asset.Name}");
        if (asset.UpdatedAt is null)
        {
            throw new InvalidOperationException($"GitHub release asset {asset.Name} did not include an updated timestamp.");
        }

        ValidateMetadataTimestamp(asset.UpdatedAt.Value, $"GitHub release asset {asset.Name} updated timestamp");
        ValidateGitHubAssetDigestMetadata(asset.Digest, asset.Name!);
        var htmlUrl = GetTrustedReleaseHtmlUrl(release.HtmlUrl, releaseTag);
        return new ReleaseInfo(
            version.Value,
            releaseId,
            releaseTag,
            assetChannel,
            string.IsNullOrWhiteSpace(release.Name) ? $"MuffMode v{version.Value}" : release.Name!,
            release.Body ?? "",
            htmlUrl,
            release.Prerelease,
            release.PublishedAt,
            assetId,
            asset.Name!,
            downloadUri.ToString(),
            asset.Size,
            asset.ContentType,
            asset.UpdatedAt,
            asset.Digest);
    }

    private static void RejectDuplicateReleaseCandidates(IReadOnlyList<ReleaseInfo> candidates)
    {
        var duplicate = candidates
            .GroupBy(
                candidate => $"{candidate.Version}|{candidate.Channel}",
                StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault(group => group.Count() > 1);
        if (duplicate is null)
        {
            return;
        }

        var versions = string.Join(", ", duplicate.Select(candidate => candidate.TagName));
        throw new InvalidOperationException($"GitHub returned multiple MuffMode releases for {duplicate.First().Version} ({duplicate.First().Channel}): {versions}.");
    }

    private static SemanticVersion? TryParseOptionalReleaseVersion(string? text)
    {
        return SemanticVersion.TryParse(text, out var version) ? version : null;
    }

    private static long GetRequiredGitHubId(long? id, string description)
    {
        if (id is not > 0)
        {
            throw new InvalidOperationException($"GitHub {description} metadata did not include a usable numeric ID.");
        }

        return id.Value;
    }

    private static void ValidateMetadataTimestamp(DateTimeOffset timestamp, string description)
    {
        if (timestamp > DateTimeOffset.UtcNow + MetadataTimestampFutureTolerance)
        {
            throw new InvalidOperationException($"{description} is in the future.");
        }
    }

    private static void ValidateGitHubAssetDigestMetadata(string? assetDigest, string assetName)
    {
        if (string.IsNullOrWhiteSpace(assetDigest))
        {
            throw new InvalidOperationException($"GitHub release asset {assetName} did not include a SHA-256 digest.");
        }

        const string sha256Prefix = "sha256:";
        if (!assetDigest.StartsWith(sha256Prefix, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"GitHub release asset {assetName} used an unsupported digest format.");
        }

        var expected = assetDigest[sha256Prefix.Length..].Trim();
        if (expected.Length != 64 || expected.Any(value => !Uri.IsHexDigit(value)))
        {
            throw new InvalidOperationException($"GitHub release asset {assetName} included an invalid SHA-256 digest.");
        }
    }

    private static string ValidateReleaseTag(string? tagName, SemanticVersion version)
    {
        if (string.IsNullOrWhiteSpace(tagName))
        {
            throw new InvalidOperationException($"GitHub release {version} did not include a tag name.");
        }

        var tag = tagName.Trim();
        if (tag.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
            || tag.Contains('/')
            || tag.Contains('\\')
            || tag.Contains('?')
            || tag.Contains('#'))
        {
            throw new InvalidOperationException($"GitHub release {version} used an unsafe tag name.");
        }

        if (!SemanticVersion.TryParse(tag, out var tagVersion)
            || tagVersion.CompareTo(version) != 0)
        {
            throw new InvalidOperationException($"GitHub release tag {tag} does not match release version {version}.");
        }

        if (!tag.Equals(version.ToString(), StringComparison.Ordinal)
            && !tag.Equals($"v{version}", StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"GitHub release tag {tag} must be exactly {version} or v{version}.");
        }

        return tag;
    }

    private static string GetTrustedReleaseHtmlUrl(string? htmlUrl, string expectedTag)
    {
        if (string.IsNullOrWhiteSpace(htmlUrl))
        {
            return $"https://github.com/{Repository}/releases/tag/{Uri.EscapeDataString(expectedTag)}";
        }

        if (!Uri.TryCreate(htmlUrl, UriKind.Absolute, out var uri)
            || uri.Scheme != Uri.UriSchemeHttps
            || !uri.Host.Equals("github.com", StringComparison.OrdinalIgnoreCase)
            || !string.IsNullOrEmpty(uri.UserInfo)
            || !uri.IsDefaultPort
            || !string.IsNullOrEmpty(uri.Query)
            || !string.IsNullOrEmpty(uri.Fragment)
            || !ReleaseHtmlUrlMatchesTag(uri, expectedTag))
        {
            throw new InvalidOperationException("GitHub release metadata contained an unexpected release page URL.");
        }

        return uri.ToString();
    }

    private static Uri CreateDownloadUri(string url, string expectedAssetName, string expectedTag)
    {
        if (!Uri.TryCreate(url, UriKind.Absolute, out var uri) || uri.Scheme != Uri.UriSchemeHttps)
        {
            throw new InvalidOperationException("The selected release asset did not provide a trusted HTTPS download URL.");
        }

        if (!string.Equals(uri.Host, "github.com", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The selected release asset points at an unexpected host: {uri.Host}");
        }

        if (!string.IsNullOrEmpty(uri.UserInfo) || !uri.IsDefaultPort)
        {
            throw new InvalidOperationException("The selected release asset download URL used an unexpected authority.");
        }

        if (!string.IsNullOrEmpty(uri.Query) || !string.IsNullOrEmpty(uri.Fragment))
        {
            throw new InvalidOperationException("The selected release asset download URL contained unexpected query or fragment data.");
        }

        EnsureExpectedGitHubReleasePath(uri, expectedTag);
        EnsureDownloadUriFileNameMatchesAsset(uri, expectedAssetName);
        return uri;
    }

    private static bool ReleaseHtmlUrlMatchesTag(Uri uri, string expectedTag)
    {
        var repositorySegments = Repository.Split('/');
        var segments = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
        return segments.Length == 5
            && segments[0].Equals(repositorySegments[0], StringComparison.OrdinalIgnoreCase)
            && segments[1].Equals(repositorySegments[1], StringComparison.OrdinalIgnoreCase)
            && segments[2].Equals("releases", StringComparison.OrdinalIgnoreCase)
            && segments[3].Equals("tag", StringComparison.OrdinalIgnoreCase)
            && Uri.UnescapeDataString(segments[4]).Equals(expectedTag, StringComparison.Ordinal);
    }

    private static void EnsureExpectedGitHubReleasePath(Uri uri, string expectedTag)
    {
        var repositorySegments = Repository.Split('/');
        var segments = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length != 6
            || !segments[0].Equals(repositorySegments[0], StringComparison.OrdinalIgnoreCase)
            || !segments[1].Equals(repositorySegments[1], StringComparison.OrdinalIgnoreCase)
            || !segments[2].Equals("releases", StringComparison.OrdinalIgnoreCase)
            || !segments[3].Equals("download", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("The selected release asset does not point at this repository's release downloads.");
        }

        var tagSegment = Uri.UnescapeDataString(segments[4]);
        if (!tagSegment.Equals(expectedTag, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"The selected release asset belongs to tag {tagSegment}, but GitHub advertised release tag {expectedTag}.");
        }
    }

    private static void EnsureDownloadUriFileNameMatchesAsset(Uri uri, string expectedAssetName)
    {
        var fileName = Uri.UnescapeDataString(Path.GetFileName(uri.AbsolutePath));
        if (!fileName.Equals(expectedAssetName, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The selected release asset download URL ends with {fileName}, but GitHub advertised {expectedAssetName}.");
        }
    }

    private static string GetSafeAssetFileName(string assetName)
    {
        if (string.IsNullOrWhiteSpace(assetName)
            || assetName.Length > MaxAssetFileNameCharacters
            || assetName.Any(char.IsControl)
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
        var channel = GetAssetChannel(assetName, version);
        return prerelease
            ? channel is "alpha" or "beta" or "rc"
            : channel is "stable";
    }

    private static string? GetAssetChannel(string assetName, SemanticVersion version)
    {
        var fileName = Path.GetFileName(assetName);
        var versionText = version.ToString();
        if (fileName.Equals($"muffmode-{versionText}.zip", StringComparison.OrdinalIgnoreCase))
        {
            return "stable";
        }

        foreach (var channel in new[] { "alpha", "beta", "rc" })
        {
            if (fileName.Equals($"muffmode-{versionText}-{channel}.zip", StringComparison.OrdinalIgnoreCase))
            {
                return channel;
            }
        }

        return null;
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
