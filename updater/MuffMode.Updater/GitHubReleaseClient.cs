using System.Net.Http.Headers;
using System.Text.Json;

namespace MuffMode.Updater;

internal sealed class GitHubReleaseClient : IDisposable
{
    public const string Repository = "DarkMatter-Productions/MuffMode";

    private const string ReleasesEndpoint = $"https://api.github.com/repos/{Repository}/releases?per_page=30";
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
            .OrderByDescending(candidate => candidate.PublishedAt ?? DateTimeOffset.MinValue)
            .FirstOrDefault()
            ?? throw new InvalidOperationException("No published MuffMode releases were found on GitHub.");

        var versionText = string.Join(" ", new[] { release.TagName, release.Name }.Where(text => !string.IsNullOrWhiteSpace(text)));
        if (!SemanticVersion.TryParse(versionText, out var version))
        {
            throw new InvalidOperationException($"The latest release does not contain a semantic version: {versionText}");
        }

        var asset = release.Assets
            .Where(asset => !string.IsNullOrWhiteSpace(asset.Name) && !string.IsNullOrWhiteSpace(asset.BrowserDownloadUrl))
            .Where(asset => asset.Name!.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
            .OrderByDescending(asset => asset.Name!.Contains("muffmode", StringComparison.OrdinalIgnoreCase))
            .ThenBy(asset => asset.Name, StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault()
            ?? throw new InvalidOperationException("The latest MuffMode release does not include a downloadable .zip asset.");

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

    public async Task<string> DownloadReleaseAssetAsync(
        ReleaseInfo release,
        string destinationDirectory,
        IProgress<UpdaterProgress>? progress,
        CancellationToken cancellationToken)
    {
        Directory.CreateDirectory(destinationDirectory);
        var destinationPath = Path.Combine(destinationDirectory, Path.GetFileName(release.AssetName));

        progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", 0));
        using var response = await _httpClient.GetAsync(release.AssetDownloadUrl, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
        response.EnsureSuccessStatusCode();

        var totalBytes = response.Content.Headers.ContentLength;
        await using var source = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using var destination = File.Create(destinationPath);

        var buffer = new byte[128 * 1024];
        long totalRead = 0;
        while (true)
        {
            var bytesRead = await source.ReadAsync(buffer, cancellationToken);
            if (bytesRead == 0)
            {
                break;
            }

            await destination.WriteAsync(buffer.AsMemory(0, bytesRead), cancellationToken);
            totalRead += bytesRead;

            if (totalBytes is > 0)
            {
                var percentage = Math.Clamp((int)((double)totalRead / totalBytes.Value * 45), 0, 45);
                progress?.Report(new UpdaterProgress($"Downloading {release.AssetName}...", percentage));
            }
        }

        progress?.Report(new UpdaterProgress("Download complete.", 45));
        return destinationPath;
    }

    public void Dispose() => _httpClient.Dispose();
}
