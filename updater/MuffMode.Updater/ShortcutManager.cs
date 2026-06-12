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

    public static bool DesktopShortcutExists => File.Exists(DesktopShortcutPath);

    public static bool StartMenuShortcutExists => File.Exists(StartMenuShortcutPath);

    public static void CreateShortcuts(bool desktop, bool startMenu)
    {
        var executablePath = Environment.ProcessPath ?? Application.ExecutablePath;
        if (string.IsNullOrWhiteSpace(executablePath) || !File.Exists(executablePath))
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
        var shortcutDirectory = Path.GetDirectoryName(shortcutPath);
        if (!string.IsNullOrWhiteSpace(shortcutDirectory))
        {
            Directory.CreateDirectory(shortcutDirectory);
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
        finally
        {
            ReleaseComObject(shortcut);
            ReleaseComObject(shell);
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
