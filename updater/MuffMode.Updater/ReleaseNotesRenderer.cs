using System.Net;
using System.Text.RegularExpressions;

namespace MuffMode.Updater;

internal static partial class ReleaseNotesRenderer
{
    private static readonly Color HeadingColor = Color.FromArgb(238, 189, 87);
    private static readonly Color BodyColor = Color.FromArgb(229, 232, 225);
    private static readonly Color MutedColor = Color.FromArgb(164, 175, 169);
    private static readonly Color LinkColor = Color.FromArgb(116, 178, 224);
    private static readonly Color QuoteColor = Color.FromArgb(184, 199, 177);
    private static readonly Color CodeColor = Color.FromArgb(235, 219, 175);
    private static readonly Color CodeBackground = Color.FromArgb(38, 42, 43);

    private const string UiFont = "Segoe UI";
    private const string CodeFont = "Cascadia Mono";

    internal static void Render(RichTextBox target, ReleaseInfo release, string markdown)
    {
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(release);

        target.SuspendLayout();
        try
        {
            target.Clear();
            target.DetectUrls = true;

            Append(target, release.Name, 15.5F, FontStyle.Bold, HeadingColor);
            AppendNewLine(target);
            Append(target, release.HtmlUrl, 9F, FontStyle.Underline, LinkColor);
            AppendNewLine(target);

            var channel = $"{release.Channel}{(release.IsPrerelease ? " prerelease" : "")}";
            var published = release.PublishedAt is null
                ? null
                : $"Published {release.PublishedAt:yyyy-MM-dd HH:mm} UTC";
            AppendMetadataLine(target, [channel, published]);

            var packageSize = release.AssetSize is > 0
                ? $"{release.AssetSize.Value / 1024D / 1024D:N1} MB"
                : null;
            var packageUpdated = release.AssetUpdatedAt is null
                ? null
                : $"updated {release.AssetUpdatedAt:yyyy-MM-dd HH:mm} UTC";
            AppendMetadataLine(target,
                [$"Package: {release.AssetName}", packageSize, packageUpdated]);

            if (!string.IsNullOrWhiteSpace(release.AssetDigest))
            {
                Append(target, $"Digest: {release.AssetDigest}", 8.75F, FontStyle.Regular, MutedColor, CodeFont);
                AppendNewLine(target);
            }

            Append(target, new string('─', 72), 8.5F, FontStyle.Regular, Color.FromArgb(78, 87, 85));
            AppendNewLine(target, 2);
            RenderMarkdown(target, markdown);

            target.SelectionStart = 0;
            target.SelectionLength = 0;
            target.ScrollToCaret();
        }
        finally
        {
            target.ResumeLayout();
        }
    }

    private static void AppendMetadataLine(RichTextBox target, IEnumerable<string?> parts)
    {
        var text = string.Join("  •  ", parts.Where(part => !string.IsNullOrWhiteSpace(part)));
        if (text.Length == 0)
        {
            return;
        }

        Append(target, text, 9F, FontStyle.Regular, MutedColor);
        AppendNewLine(target);
    }

    private static void RenderMarkdown(RichTextBox target, string markdown)
    {
        if (string.IsNullOrWhiteSpace(markdown))
        {
            Append(target, "No changelog text was published with this release.", 9.75F, FontStyle.Italic, MutedColor);
            return;
        }

        var normalized = WebUtility.HtmlDecode(markdown)
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n');
        var lines = normalized.Split('\n');

        for (var index = 0; index < lines.Length;)
        {
            var line = lines[index].TrimEnd();
            if (string.IsNullOrWhiteSpace(line))
            {
                EnsureParagraphBreak(target);
                index++;
                continue;
            }

            var fence = FenceRegex().Match(line);
            if (fence.Success)
            {
                index++;
                var codeLines = new List<string>();
                while (index < lines.Length && !FenceRegex().IsMatch(lines[index]))
                {
                    codeLines.Add(lines[index]);
                    index++;
                }
                if (index < lines.Length)
                {
                    index++;
                }
                AppendCodeBlock(target, codeLines);
                continue;
            }

            var heading = HeadingRegex().Match(line);
            if (heading.Success)
            {
                EnsureParagraphBreak(target);
                var level = heading.Groups[1].Value.Length;
                var size = level switch
                {
                    1 => 16F,
                    2 => 13.5F,
                    3 => 11.5F,
                    _ => 10.5F
                };
                AppendInline(target, heading.Groups[2].Value.Trim(), size, FontStyle.Bold, HeadingColor);
                AppendNewLine(target, 2);
                index++;
                continue;
            }

            if (HorizontalRuleRegex().IsMatch(line))
            {
                Append(target, new string('─', 54), 8.5F, FontStyle.Regular, Color.FromArgb(78, 87, 85));
                AppendNewLine(target, 2);
                index++;
                continue;
            }

            var list = ListRegex().Match(line);
            if (list.Success)
            {
                var indentation = Math.Min(list.Groups[1].Value.Length * 6, 48);
                var marker = list.Groups[2].Value;
                var body = list.Groups[3].Value;
                var check = ChecklistRegex().Match(body);
                if (check.Success)
                {
                    marker = check.Groups[1].Value.Equals("x", StringComparison.OrdinalIgnoreCase) ? "☑" : "☐";
                    body = check.Groups[2].Value;
                }
                else if (marker is "-" or "+" or "*")
                {
                    marker = "•";
                }

                target.SelectionIndent = 20 + indentation;
                target.SelectionHangingIndent = -14;
                Append(target, $"{marker} ", 9.75F, FontStyle.Bold, HeadingColor);
                AppendInline(target, body, 9.75F, FontStyle.Regular, BodyColor);
                AppendNewLine(target);
                ResetParagraph(target);
                index++;
                continue;
            }

            var quote = QuoteRegex().Match(line);
            if (quote.Success)
            {
                target.SelectionIndent = 18;
                target.SelectionHangingIndent = -12;
                Append(target, "▌ ", 9.75F, FontStyle.Bold, HeadingColor);
                AppendInline(target, quote.Groups[1].Value.Trim(), 9.75F, FontStyle.Italic, QuoteColor);
                AppendNewLine(target);
                ResetParagraph(target);
                index++;
                continue;
            }

            var paragraph = new List<string> { line.Trim() };
            index++;
            while (index < lines.Length && !IsBlockBoundary(lines[index]))
            {
                paragraph.Add(lines[index].Trim());
                index++;
            }
            AppendInline(target, string.Join(' ', paragraph), 9.75F, FontStyle.Regular, BodyColor);
            AppendNewLine(target, 2);
        }
    }

    private static bool IsBlockBoundary(string line)
    {
        var trimmed = line.TrimEnd();
        return string.IsNullOrWhiteSpace(trimmed)
            || FenceRegex().IsMatch(trimmed)
            || HeadingRegex().IsMatch(trimmed)
            || HorizontalRuleRegex().IsMatch(trimmed)
            || ListRegex().IsMatch(trimmed)
            || QuoteRegex().IsMatch(trimmed);
    }

    private static void AppendCodeBlock(RichTextBox target, IReadOnlyList<string> lines)
    {
        target.SelectionIndent = 12;
        for (var index = 0; index < lines.Count; index++)
        {
            Append(target, lines[index], 9F, FontStyle.Regular, CodeColor, CodeFont, CodeBackground);
            AppendNewLine(target);
        }
        ResetParagraph(target);
        AppendNewLine(target);
    }

    private static void AppendInline(
        RichTextBox target,
        string markdown,
        float size,
        FontStyle style,
        Color color)
    {
        for (var index = 0; index < markdown.Length;)
        {
            if (markdown[index] == '\\' && index + 1 < markdown.Length)
            {
                Append(target, markdown[index + 1].ToString(), size, style, color);
                index += 2;
                continue;
            }

            if (TryAppendDelimited(target, markdown, ref index, "**", size, style | FontStyle.Bold, color)
                || TryAppendDelimited(target, markdown, ref index, "__", size, style | FontStyle.Bold, color)
                || TryAppendDelimited(target, markdown, ref index, "~~", size, style | FontStyle.Strikeout, color)
                || TryAppendDelimited(target, markdown, ref index, "*", size, style | FontStyle.Italic, color)
                || TryAppendDelimited(target, markdown, ref index, "_", size, style | FontStyle.Italic, color))
            {
                continue;
            }

            if (markdown[index] == '`')
            {
                var end = markdown.IndexOf('`', index + 1);
                if (end > index + 1)
                {
                    Append(target, markdown[(index + 1)..end], size - 0.25F, FontStyle.Regular, CodeColor, CodeFont, CodeBackground);
                    index = end + 1;
                    continue;
                }
            }

            var link = LinkRegex().Match(markdown, index);
            if (link.Success && link.Index == index)
            {
                var label = link.Groups[1].Value;
                var url = link.Groups[2].Value;
                if (string.Equals(label, url, StringComparison.OrdinalIgnoreCase))
                {
                    Append(target, url, size, style | FontStyle.Underline, LinkColor);
                }
                else
                {
                    Append(target, label, size, style, LinkColor);
                    Append(target, " — ", size, style, MutedColor);
                    Append(target, url, size - 0.5F, FontStyle.Underline, LinkColor);
                }
                index += link.Length;
                continue;
            }

            var next = FindNextInlineMarker(markdown, index + 1);
            Append(target, markdown[index..next], size, style, color);
            index = next;
        }
    }

    private static bool TryAppendDelimited(
        RichTextBox target,
        string markdown,
        ref int index,
        string delimiter,
        float size,
        FontStyle style,
        Color color)
    {
        if (!markdown.AsSpan(index).StartsWith(delimiter, StringComparison.Ordinal))
        {
            return false;
        }

        var end = markdown.IndexOf(delimiter, index + delimiter.Length, StringComparison.Ordinal);
        if (end <= index + delimiter.Length)
        {
            return false;
        }

        AppendInline(target, markdown[(index + delimiter.Length)..end], size, style, color);
        index = end + delimiter.Length;
        return true;
    }

    private static int FindNextInlineMarker(string text, int start)
    {
        for (var index = start; index < text.Length; index++)
        {
            if (text[index] is '\\' or '*' or '_' or '~' or '`' or '[')
            {
                return index;
            }
        }
        return text.Length;
    }

    private static void EnsureParagraphBreak(RichTextBox target)
    {
        if (target.TextLength == 0)
        {
            return;
        }

        if (target.Text.EndsWith(Environment.NewLine + Environment.NewLine, StringComparison.Ordinal))
        {
            return;
        }

        AppendNewLine(target, target.Text.EndsWith(Environment.NewLine, StringComparison.Ordinal) ? 1 : 2);
    }

    private static void AppendNewLine(RichTextBox target, int count = 1)
    {
        Append(target, string.Concat(Enumerable.Repeat(Environment.NewLine, count)), 9.75F, FontStyle.Regular, BodyColor);
    }

    private static void ResetParagraph(RichTextBox target)
    {
        target.SelectionIndent = 0;
        target.SelectionHangingIndent = 0;
    }

    private static void Append(
        RichTextBox target,
        string text,
        float size,
        FontStyle style,
        Color color,
        string fontFamily = UiFont,
        Color? background = null)
    {
        if (text.Length == 0)
        {
            return;
        }

        target.SelectionStart = target.TextLength;
        target.SelectionLength = 0;
        using var font = CreateFont(fontFamily, size, style);
        target.SelectionFont = font;
        target.SelectionColor = color;
        target.SelectionBackColor = background ?? target.BackColor;
        target.AppendText(text);
    }

    private static Font CreateFont(string family, float size, FontStyle style)
    {
        try
        {
            return new Font(family, size, style, GraphicsUnit.Point);
        }
        catch (ArgumentException)
        {
            return new Font(FontFamily.GenericSansSerif, size, style, GraphicsUnit.Point);
        }
    }

    [GeneratedRegex(@"^\s*(```|~~~)")]
    private static partial Regex FenceRegex();

    [GeneratedRegex(@"^\s*(#{1,6})\s+(.+?)\s*#*\s*$")]
    private static partial Regex HeadingRegex();

    [GeneratedRegex(@"^(\s*)((?:[-+*])|(?:\d+[.)]))\s+(.+)$")]
    private static partial Regex ListRegex();

    [GeneratedRegex(@"^\s*\[([ xX])\]\s+(.+)$")]
    private static partial Regex ChecklistRegex();

    [GeneratedRegex(@"^\s*>\s?(.*)$")]
    private static partial Regex QuoteRegex();

    [GeneratedRegex(@"^\s*((-{3,})|(\*{3,})|(_{3,}))\s*$")]
    private static partial Regex HorizontalRuleRegex();

    [GeneratedRegex(@"\[([^\]\r\n]+)\]\((https?://[^\s)]+)\)", RegexOptions.IgnoreCase)]
    private static partial Regex LinkRegex();
}
