using System.Text;

namespace MuffMode.Updater;

internal static class UpdaterLog
{
    private const long MaxLogBytes = 1024L * 1024L;
    private const int MaxLogEntryCharacters = 16_000;
    private const int LogWriteBufferSize = 16 * 1024;
    private const string LogFileName = "updater.log";
    private static readonly Encoding StrictUtf8Encoding = new UTF8Encoding(
        encoderShouldEmitUTF8Identifier: false,
        throwOnInvalidBytes: true);

    public static string LogPath => Path.Combine(InstallationManager.SettingsDirectory, LogFileName);

    public static void WriteInfo(string message)
    {
        Write("INFO", message);
    }

    public static void WriteException(string message, Exception exception)
    {
        Write("ERROR", $"{message}{Environment.NewLine}{exception}");
    }

    private static void Write(string level, string message)
    {
        try
        {
            EnsureDirectoryPathHasNoReparsePoints(InstallationManager.SettingsDirectory);
            Directory.CreateDirectory(InstallationManager.SettingsDirectory);
            EnsureDirectoryPathHasNoReparsePoints(InstallationManager.SettingsDirectory);
            EnsureSafeLogFile(LogPath, "diagnostics log");
            RotateIfNeeded();
            EnsureSafeLogFile(LogPath, "diagnostics log");
            var line = $"{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss zzz} [{level}] {TruncateForLog(message)}{Environment.NewLine}";
            AppendLogLineWithFlush(LogPath, line);
            VerifyLogWrite(LogPath);
        }
        catch
        {
            // Diagnostics must never keep the updater from doing useful work.
        }
    }

    private static void RotateIfNeeded()
    {
        var logPath = LogPath;
        if (!File.Exists(logPath))
        {
            return;
        }

        EnsureSafeLogFile(logPath, "diagnostics log");
        if (new FileInfo(logPath).Length <= MaxLogBytes)
        {
            return;
        }

        var previousLogPath = $"{logPath}.1";
        EnsureSafeLogFile(previousLogPath, "rotated diagnostics log");
        if (File.Exists(previousLogPath))
        {
            File.Delete(previousLogPath);
        }

        File.Move(logPath, previousLogPath);
        VerifyLogWrite(previousLogPath);
    }

    private static void AppendLogLineWithFlush(string path, string line)
    {
        using var destination = new FileStream(
            path,
            FileMode.Append,
            FileAccess.Write,
            FileShare.ReadWrite,
            LogWriteBufferSize,
            FileOptions.WriteThrough);
        var bytes = StrictUtf8Encoding.GetBytes(line);
        destination.Write(bytes);
        destination.Flush(flushToDisk: true);
    }

    private static string TruncateForLog(string message)
    {
        if (message.Length <= MaxLogEntryCharacters)
        {
            return message;
        }

        return message[..MaxLogEntryCharacters] + Environment.NewLine + "[Log entry truncated.]";
    }

    private static void EnsureSafeLogFile(string path, string description)
    {
        var fileName = Path.GetFileName(path);
        if (!fileName.Equals(LogFileName, StringComparison.OrdinalIgnoreCase)
            && !fileName.Equals($"{LogFileName}.1", StringComparison.OrdinalIgnoreCase))
        {
            throw new IOException($"The {description} file name was unexpected.");
        }

        if (Directory.Exists(path))
        {
            throw new IOException($"The {description} path points at a directory.");
        }

        if (File.Exists(path)
            && (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
        {
            throw new IOException($"The {description} is a reparse point.");
        }
    }

    private static void VerifyLogWrite(string path)
    {
        EnsureSafeLogFile(path, "diagnostics log");
        if (!File.Exists(path) || new FileInfo(path).Length == 0)
        {
            throw new IOException("The diagnostics log could not be verified after writing.");
        }
    }

    private static void EnsureDirectoryPathHasNoReparsePoints(string directoryPath)
    {
        var current = Path.GetFullPath(directoryPath);
        while (!string.IsNullOrWhiteSpace(current))
        {
            if (Directory.Exists(current)
                && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
            {
                throw new IOException($"The diagnostics folder contains a reparse point: {current}");
            }

            var parent = Directory.GetParent(current);
            if (parent is null)
            {
                break;
            }

            current = parent.FullName;
        }
    }
}
