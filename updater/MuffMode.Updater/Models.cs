using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace MuffMode.Updater;

internal sealed class AppSettings
{
    public string? InstallPath { get; set; }
    public bool AutoLaunchAfterUpdate { get; set; } = true;
}

internal sealed record ReleaseInfo(
    SemanticVersion Version,
    string TagName,
    string Name,
    string Changelog,
    string HtmlUrl,
    bool IsPrerelease,
    DateTimeOffset? PublishedAt,
    string AssetName,
    string AssetDownloadUrl);

internal sealed record LocalInstallVersion(SemanticVersion? Version, string DisplayText, string Source);

internal sealed record InstallCandidate(string Source, string Path)
{
    public override string ToString() => $"{Source} detected - {Path}";
}

internal sealed record UpdaterProgress(string Message, int? Percentage = null, bool CanCancel = true);

internal sealed class InstalledVersionMarker
{
    public string Version { get; set; } = "";
    public string? TagName { get; set; }
    public string? ReleaseUrl { get; set; }
    public string? AssetName { get; set; }
    public string Repository { get; set; } = GitHubReleaseClient.Repository;
    public DateTimeOffset InstalledAtUtc { get; set; }
}

internal readonly partial record struct SemanticVersion(int Major, int Minor, int Patch) : IComparable<SemanticVersion>
{
    public static bool TryParse(string? value, out SemanticVersion version)
    {
        version = default;
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var match = VersionRegex().Match(value);
        if (!match.Success)
        {
            return false;
        }

        version = new SemanticVersion(
            int.Parse(match.Groups["major"].Value),
            int.Parse(match.Groups["minor"].Value),
            int.Parse(match.Groups["patch"].Value));
        return true;
    }

    public int CompareTo(SemanticVersion other)
    {
        var major = Major.CompareTo(other.Major);
        if (major != 0)
        {
            return major;
        }

        var minor = Minor.CompareTo(other.Minor);
        if (minor != 0)
        {
            return minor;
        }

        return Patch.CompareTo(other.Patch);
    }

    public override string ToString() => $"{Major}.{Minor}.{Patch}";

    [GeneratedRegex(@"v?(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)(?:[-+][0-9A-Za-z.-]+)?", RegexOptions.IgnoreCase)]
    private static partial Regex VersionRegex();
}

internal sealed class GitHubReleaseDto
{
    [JsonPropertyName("tag_name")]
    public string? TagName { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("body")]
    public string? Body { get; set; }

    [JsonPropertyName("html_url")]
    public string? HtmlUrl { get; set; }

    [JsonPropertyName("draft")]
    public bool Draft { get; set; }

    [JsonPropertyName("prerelease")]
    public bool Prerelease { get; set; }

    [JsonPropertyName("published_at")]
    public DateTimeOffset? PublishedAt { get; set; }

    [JsonPropertyName("assets")]
    public List<GitHubAssetDto> Assets { get; set; } = [];
}

internal sealed class GitHubAssetDto
{
    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("browser_download_url")]
    public string? BrowserDownloadUrl { get; set; }
}
