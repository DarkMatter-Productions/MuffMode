using System.Reflection;
using System.Runtime.InteropServices;

namespace MuffMode.Updater;

internal static class ShortcutManager
{
    private const string ShortcutFileName = "Muff Mode Updater & Launcher.lnk";
    private const string ShortcutDescription = "Update Muff Mode and launch Quake II Remastered.";

    public static string DesktopShortcutPath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), ShortcutFileName);

    public static string StartMenuShortcutPath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.Programs),
            "Muff Mode",
            ShortcutFileName);

    public static bool DesktopShortcutExists => IsExistingShortcutFile(DesktopShortcutPath);

    public static bool StartMenuShortcutExists => IsExistingShortcutFile(StartMenuShortcutPath);

    public static void CreateShortcuts(bool desktop, bool startMenu)
    {
        var executablePath = NormalizeExecutablePath(Environment.ProcessPath ?? Application.ExecutablePath);
        if (!IsSafeExecutableTarget(executablePath))
        {
            throw new InvalidOperationException("Could not determine the updater and launcher executable path.");
        }

        var workingDirectory = NormalizeDirectoryPath(Path.GetDirectoryName(executablePath) ?? AppContext.BaseDirectory);
        if (!IsSafeWorkingDirectory(workingDirectory))
        {
            throw new InvalidOperationException("Could not determine a safe updater and launcher working folder.");
        }

        if (desktop)
        {
            CreateShortcut(DesktopShortcutPath, executablePath, workingDirectory);
        }

        if (startMenu)
        {
            CreateShortcut(StartMenuShortcutPath, executablePath, workingDirectory);
        }
    }

    private static void CreateShortcut(string shortcutPath, string targetPath, string workingDirectory)
    {
        ValidateShortcutPath(shortcutPath);
        var shortcutExisted = File.Exists(shortcutPath);
        var originalAttributes = PrepareExistingShortcutForReplace(shortcutPath);
        var shortcutDirectory = Path.GetDirectoryName(shortcutPath);
        if (!string.IsNullOrWhiteSpace(shortcutDirectory))
        {
            EnsureDirectoryPathHasNoReparsePoints(shortcutDirectory, "shortcut folder");
            Directory.CreateDirectory(shortcutDirectory);
            ValidateShortcutDirectory(shortcutDirectory);
        }

        var shellType = Type.GetTypeFromProgID("WScript.Shell")
            ?? throw new InvalidOperationException("Windows Script Host shortcut support is unavailable.");

        object? shell = null;
        object? shortcut = null;
        try
        {
            shell = Activator.CreateInstance(shellType)
                ?? throw new InvalidOperationException("Could not open Windows shortcut support.");

            shortcut = shellType.InvokeMember(
                "CreateShortcut",
                BindingFlags.InvokeMethod,
                null,
                shell,
                [shortcutPath])
                ?? throw new InvalidOperationException("Could not create the shortcut.");

            SetShortcutProperty(shortcut, "TargetPath", targetPath);
            SetShortcutProperty(shortcut, "WorkingDirectory", workingDirectory);
            SetShortcutProperty(shortcut, "Description", ShortcutDescription);
            SetShortcutProperty(shortcut, "IconLocation", $"{targetPath},0");
            shortcut.GetType().InvokeMember("Save", BindingFlags.InvokeMethod, null, shortcut, null);
            VerifyShortcutObject(shortcut, targetPath, workingDirectory);
            VerifyShortcutFileWritten(shortcutPath);
        }
        catch
        {
            RestoreShortcutAttributes(shortcutPath, originalAttributes);
            if (!shortcutExisted)
            {
                TryDeleteShortcut(shortcutPath);
            }

            throw;
        }
        finally
        {
            ReleaseComObject(shortcut);
            ReleaseComObject(shell);
        }

        RestoreShortcutAttributes(shortcutPath, originalAttributes);
    }

    private static string NormalizeExecutablePath(string? executablePath)
    {
        try
        {
            return string.IsNullOrWhiteSpace(executablePath)
                ? ""
                : Path.GetFullPath(executablePath);
        }
        catch
        {
            return "";
        }
    }

    private static string NormalizeDirectoryPath(string? directoryPath)
    {
        try
        {
            return string.IsNullOrWhiteSpace(directoryPath)
                ? ""
                : Path.GetFullPath(directoryPath);
        }
        catch
        {
            return "";
        }
    }

    private static bool IsSafeExecutableTarget(string executablePath)
    {
        if (string.IsNullOrWhiteSpace(executablePath) || !File.Exists(executablePath))
        {
            return false;
        }

        try
        {
            var attributes = File.GetAttributes(executablePath);
            return (attributes & (FileAttributes.Directory | FileAttributes.ReparsePoint)) == 0
                && string.Equals(Path.GetExtension(executablePath), ".exe", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    private static bool IsSafeWorkingDirectory(string workingDirectory)
    {
        if (string.IsNullOrWhiteSpace(workingDirectory) || !Path.IsPathFullyQualified(workingDirectory))
        {
            return false;
        }

        try
        {
            var attributes = File.GetAttributes(workingDirectory);
            return (attributes & (FileAttributes.Directory | FileAttributes.ReparsePoint)) == FileAttributes.Directory;
        }
        catch
        {
            return false;
        }
    }

    private static void ValidateShortcutPath(string shortcutPath)
    {
        if (string.IsNullOrWhiteSpace(shortcutPath)
            || !shortcutPath.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase)
            || Path.GetFileName(shortcutPath).IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
        {
            throw new InvalidOperationException("The shortcut path is not valid.");
        }

        var shortcutDirectory = Path.GetDirectoryName(shortcutPath);
        if (!string.IsNullOrWhiteSpace(shortcutDirectory)
            && Directory.Exists(shortcutDirectory)
            && IsReparsePoint(shortcutDirectory))
        {
            throw new InvalidOperationException("The shortcut folder is a reparse point.");
        }

        if ((File.Exists(shortcutPath) || Directory.Exists(shortcutPath)) && IsReparsePoint(shortcutPath))
        {
            throw new InvalidOperationException("The shortcut path is a reparse point.");
        }

        if (Directory.Exists(shortcutPath))
        {
            throw new InvalidOperationException("The shortcut path points at a directory.");
        }
    }

    private static void ValidateShortcutDirectory(string shortcutDirectory)
    {
        EnsureDirectoryPathHasNoReparsePoints(shortcutDirectory, "shortcut folder");
        if (Directory.Exists(shortcutDirectory) && IsReparsePoint(shortcutDirectory))
        {
            throw new InvalidOperationException("The shortcut folder is a reparse point.");
        }
    }

    private static FileAttributes? PrepareExistingShortcutForReplace(string shortcutPath)
    {
        if (!File.Exists(shortcutPath))
        {
            return null;
        }

        var attributes = File.GetAttributes(shortcutPath);
        if ((attributes & FileAttributes.ReadOnly) != 0)
        {
            File.SetAttributes(shortcutPath, attributes & ~FileAttributes.ReadOnly);
        }

        return attributes;
    }

    private static void VerifyShortcutFileWritten(string shortcutPath)
    {
        if (!File.Exists(shortcutPath)
            || Directory.Exists(shortcutPath)
            || IsReparsePoint(shortcutPath)
            || !string.Equals(Path.GetExtension(shortcutPath), ".lnk", StringComparison.OrdinalIgnoreCase)
            || new FileInfo(shortcutPath).Length == 0)
        {
            throw new IOException("Windows shortcut support did not write a usable shortcut file.");
        }
    }

    private static void VerifyShortcutObject(object shortcut, string expectedTargetPath, string expectedWorkingDirectory)
    {
        var targetPath = GetShortcutStringProperty(shortcut, "TargetPath");
        if (!string.Equals(targetPath, expectedTargetPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new IOException("Windows shortcut support wrote an unexpected shortcut target.");
        }

        var workingDirectory = GetShortcutStringProperty(shortcut, "WorkingDirectory");
        if (!string.Equals(workingDirectory, expectedWorkingDirectory, StringComparison.OrdinalIgnoreCase))
        {
            throw new IOException("Windows shortcut support wrote an unexpected shortcut working directory.");
        }
    }

    private static void RestoreShortcutAttributes(string shortcutPath, FileAttributes? attributes)
    {
        if (attributes is null || !File.Exists(shortcutPath))
        {
            return;
        }

        try
        {
            File.SetAttributes(shortcutPath, attributes.Value);
        }
        catch
        {
            // Attribute restoration is best-effort after shortcut creation.
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

    private static bool IsExistingShortcutFile(string shortcutPath)
    {
        try
        {
            return File.Exists(shortcutPath)
                && !Directory.Exists(shortcutPath)
                && !IsReparsePoint(shortcutPath)
                && string.Equals(Path.GetExtension(shortcutPath), ".lnk", StringComparison.OrdinalIgnoreCase)
                && new FileInfo(shortcutPath).Length > 0
                && ShortcutTargetsCurrentExecutable(shortcutPath);
        }
        catch
        {
            return false;
        }
    }

    private static bool ShortcutTargetsCurrentExecutable(string shortcutPath)
    {
        var executablePath = NormalizeExecutablePath(Environment.ProcessPath ?? Application.ExecutablePath);
        if (!IsSafeExecutableTarget(executablePath))
        {
            return false;
        }

        var shellType = Type.GetTypeFromProgID("WScript.Shell");
        if (shellType is null)
        {
            return false;
        }

        object? shell = null;
        object? shortcut = null;
        try
        {
            shell = Activator.CreateInstance(shellType);
            if (shell is null)
            {
                return false;
            }

            shortcut = shellType.InvokeMember(
                "CreateShortcut",
                BindingFlags.InvokeMethod,
                null,
                shell,
                [shortcutPath]);
            if (shortcut is null)
            {
                return false;
            }

            var targetPath = NormalizeExecutablePath(GetShortcutStringProperty(shortcut, "TargetPath"));
            return string.Equals(targetPath, executablePath, StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
        finally
        {
            ReleaseComObject(shortcut);
            ReleaseComObject(shell);
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

    private static void TryDeleteShortcut(string shortcutPath)
    {
        try
        {
            if (File.Exists(shortcutPath) && !IsReparsePoint(shortcutPath))
            {
                File.Delete(shortcutPath);
            }
        }
        catch
        {
            // Partial shortcut cleanup failure is non-fatal.
        }
    }

    private static void SetShortcutProperty(object shortcut, string propertyName, object value)
    {
        shortcut.GetType().InvokeMember(
            propertyName,
            BindingFlags.SetProperty,
            null,
            shortcut,
            [value]);
    }

    private static string GetShortcutStringProperty(object shortcut, string propertyName)
    {
        return shortcut.GetType().InvokeMember(
            propertyName,
            BindingFlags.GetProperty,
            null,
            shortcut,
            null) as string ?? "";
    }

    private static void ReleaseComObject(object? comObject)
    {
        if (comObject is not null && Marshal.IsComObject(comObject))
        {
            Marshal.FinalReleaseComObject(comObject);
        }
    }
}
