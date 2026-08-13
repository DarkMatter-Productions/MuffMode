using System.Globalization;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace MuffMode.Updater;

internal sealed class AppSettings
{
    public string? InstallPath { get; set; }
    public bool AutoLaunchAfterUpdate { get; set; } = true;
    public bool IncludePrereleases { get; set; }
}

internal sealed record ReleaseInfo(
    SemanticVersion Version,
    long ReleaseId,
    string TagName,
    string Channel,
    string Name,
    string Changelog,
    string HtmlUrl,
    bool IsPrerelease,
    DateTimeOffset? PublishedAt,
    long AssetId,
    string AssetName,
    string AssetDownloadUrl,
    long? AssetSize,
    string? AssetContentType,
    DateTimeOffset? AssetUpdatedAt,
    string? AssetDigest);

internal sealed record LocalInstallVersion(SemanticVersion? Version, string DisplayText, string Source);

internal sealed record InstallCandidate(string Source, string Path)
{
    public override string ToString() => $"{Source} detected - {Path}";
}

internal sealed record UpdaterProgress(string Message, int? Percentage = null, bool CanCancel = true);

internal sealed class InstalledVersionMarker
{
    public string Version { get; set; } = "";
    public long ReleaseId { get; set; }
    public string? TagName { get; set; }
    public string? Channel { get; set; }
    public string? ReleaseUrl { get; set; }
    public bool ReleaseIsPrerelease { get; set; }
    public DateTimeOffset? ReleasePublishedAt { get; set; }
    public long AssetId { get; set; }
    public string? AssetName { get; set; }
    public long? AssetSize { get; set; }
    public string? AssetContentType { get; set; }
    public DateTimeOffset? AssetUpdatedAt { get; set; }
    public string? AssetDigest { get; set; }
    public string? PackageRootName { get; set; }
    public string? BackupFileName { get; set; }
    public string? BackupGameDllSha256 { get; set; }
    public string? ConfigBackupDirectoryName { get; set; }
    public string? PackageSha256 { get; set; }
    public DateTimeOffset? PackagedAtUtc { get; set; }
    public string? InstalledGameDllSha256 { get; set; }
    public string? InstalledManifestSha256 { get; set; }
    public string? InstalledDestinationManifestSha256 { get; set; }
    public int InstalledFileCount { get; set; }
    public long InstalledBytes { get; set; }
    public string Repository { get; set; } = "";
    public string? InstalledByUpdaterVersion { get; set; }
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
        return match.Success && TryReadComponents(match, out version);
    }

    // Release tags name download URLs and staged install paths, so a tag has to be the
    // whole version and nothing else. TryParse scans free-form release titles, so it also
    // matches a version embedded in surrounding text and is too loose for a tag.
    public static bool TryParseExact(string? value, out SemanticVersion version)
    {
        version = default;
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var match = ExactVersionRegex().Match(value);
        return match.Success && TryReadComponents(match, out version);
    }

    private static bool TryReadComponents(Match match, out SemanticVersion version)
    {
        version = default;
        if (!int.TryParse(match.Groups["major"].Value, NumberStyles.None, CultureInfo.InvariantCulture, out var major)
            || !int.TryParse(match.Groups["minor"].Value, NumberStyles.None, CultureInfo.InvariantCulture, out var minor)
            || !int.TryParse(match.Groups["patch"].Value, NumberStyles.None, CultureInfo.InvariantCulture, out var patch))
        {
            return false;
        }

        version = new SemanticVersion(major, minor, patch);
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

    // \A and \z rather than ^ and $ so a trailing newline cannot smuggle text past the anchor.
    // Components accept leading zeros because MuffMode shipped zero-padded tags (v0.22.00),
    // and are capped at nine digits so int.TryParse cannot overflow.
    [GeneratedRegex(@"\Av?(?<major>\d{1,9})\.(?<minor>\d{1,9})\.(?<patch>\d{1,9})\z", RegexOptions.IgnoreCase)]
    private static partial Regex ExactVersionRegex();
}

internal sealed class GitHubReleaseDto
{
    [JsonPropertyName("id")]
    public long? Id { get; set; }

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
    [JsonPropertyName("id")]
    public long? Id { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("browser_download_url")]
    public string? BrowserDownloadUrl { get; set; }

    [JsonPropertyName("size")]
    public long? Size { get; set; }

    [JsonPropertyName("content_type")]
    public string? ContentType { get; set; }

    [JsonPropertyName("state")]
    public string? State { get; set; }

    [JsonPropertyName("updated_at")]
    public DateTimeOffset? UpdatedAt { get; set; }

    [JsonPropertyName("digest")]
    public string? Digest { get; set; }
}
