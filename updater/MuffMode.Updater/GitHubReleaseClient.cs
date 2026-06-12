using System.Net.Http.Headers;
using System.Text.Json;

namespace MuffMode.Updater;

internal sealed class GitHubReleaseClient : IDisposable
{
    public const string Repository = "DarkMatter-Productions/MuffMode";

    private const string ReleasesEndpoint = $"https://api.github.com/repos/{Repository}/releases?per_page=30";
    private const long MaxDownloadBytes = 1024L * 1024L * 1024L;
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
        response.EnsureSuccessStatusCode();

        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        var releases = await JsonSerializer.DeserializeAsync<List<GitHubReleaseDto>>(stream, cancellationToken: cancellationToken)
            ?? [];

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
        Directory.CreateDirectory(destinationDirectory);
        var destinationFileName = Path.GetFileName(release.AssetName);
        if (string.IsNullOrWhiteSpace(destinationFileName))
        {
            throw new InvalidOperationException("The selected release asset does not have a valid file name.");
        }

        var destinationPath = Path.Combine(destinationDirectory, destinationFileName);
        var temporaryPath = Path.Combine(destinationDirectory, $".{destinationFileName}.{Guid.NewGuid():N}.download");
        var downloadUri = CreateDownloadUri(release.AssetDownloadUrl);

        progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", 0));

        try
        {
            using var response = await _httpClient.GetAsync(downloadUri, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
            response.EnsureSuccessStatusCode();

            var totalBytes = response.Content.Headers.ContentLength;
            if (totalBytes is > MaxDownloadBytes)
            {
                throw new InvalidOperationException($"The release package is larger than the supported limit of {MaxDownloadBytes / 1024 / 1024} MB.");
            }

            long totalRead = 0;
            await using (var source = await response.Content.ReadAsStreamAsync(cancellationToken))
            await using (var destination = new FileStream(
                temporaryPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                128 * 1024,
                FileOptions.Asynchronous | FileOptions.SequentialScan))
            {
                var buffer = new byte[128 * 1024];
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

            File.Move(temporaryPath, destinationPath, overwrite: true);
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

    private static bool IsReleasePackageZip(GitHubAssetDto asset)
    {
        if (string.IsNullOrWhiteSpace(asset.Name) || string.IsNullOrWhiteSpace(asset.BrowserDownloadUrl))
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
            .OrderByDescending(asset => AssetNameMatchesRelease(asset.Name!, version, release.Prerelease))
            .ThenBy(asset => asset.Name, StringComparer.OrdinalIgnoreCase)
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
            asset.BrowserDownloadUrl!);
    }

    private static Uri CreateDownloadUri(string url)
    {
        if (!Uri.TryCreate(url, UriKind.Absolute, out var uri) || uri.Scheme != Uri.UriSchemeHttps)
        {
            throw new InvalidOperationException("The selected release asset did not provide a trusted HTTPS download URL.");
        }

        if (!string.Equals(uri.Host, "github.com", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(uri.Host, "objects.githubusercontent.com", StringComparison.OrdinalIgnoreCase)
            && !uri.Host.EndsWith(".githubusercontent.com", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"The selected release asset points at an unexpected host: {uri.Host}");
        }

        return uri;
    }

    private static bool AssetNameMatchesRelease(string assetName, SemanticVersion version, bool prerelease)
    {
        var expectedVersion = version.ToString();
        if (!assetName.Contains(expectedVersion, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return prerelease
            ? assetName.Contains("beta", StringComparison.OrdinalIgnoreCase)
                || assetName.Contains("alpha", StringComparison.OrdinalIgnoreCase)
                || assetName.Contains("rc", StringComparison.OrdinalIgnoreCase)
            : !assetName.Contains("beta", StringComparison.OrdinalIgnoreCase)
                && !assetName.Contains("alpha", StringComparison.OrdinalIgnoreCase)
                && !assetName.Contains("rc", StringComparison.OrdinalIgnoreCase);
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
}
