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

        var workingDirectory = Path.GetDirectoryName(executablePath) ?? AppContext.BaseDirectory;
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
        var shortcutDirectory = Path.GetDirectoryName(shortcutPath);
        if (!string.IsNullOrWhiteSpace(shortcutDirectory))
        {
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
        }
        catch
        {
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
        if (Directory.Exists(shortcutDirectory) && IsReparsePoint(shortcutDirectory))
        {
            throw new InvalidOperationException("The shortcut folder is a reparse point.");
        }
    }

    private static bool IsExistingShortcutFile(string shortcutPath)
    {
        try
        {
            return File.Exists(shortcutPath)
                && !Directory.Exists(shortcutPath)
                && !IsReparsePoint(shortcutPath)
                && string.Equals(Path.GetExtension(shortcutPath), ".lnk", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
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

    private static void ReleaseComObject(object? comObject)
    {
        if (comObject is not null && Marshal.IsComObject(comObject))
        {
            Marshal.FinalReleaseComObject(comObject);
        }
    }
}
