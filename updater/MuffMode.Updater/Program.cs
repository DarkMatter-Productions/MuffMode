namespace MuffMode.Updater;

internal static class Program
{
    private static readonly TimeSpan SelfUpdateCleanupMutexTimeout = TimeSpan.FromSeconds(30);

    [STAThread]
    private static int Main(string[] args)
    {
        if (InstallationManager.TryHandleSelfUpdateApplyStartup(args, out var selfUpdateExitCode))
        {
            return selfUpdateExitCode;
        }

        var selfUpdateCleanup = InstallationManager.IsSelfUpdateCleanupStartup(args);
        ApplicationConfiguration.Initialize();
        using var singleInstanceMutex = new Mutex(false, InstallationManager.SingleInstanceMutexName);
        var ownsMutex = false;
        try
        {
            try
            {
                ownsMutex = singleInstanceMutex.WaitOne(
                    selfUpdateCleanup ? SelfUpdateCleanupMutexTimeout : TimeSpan.Zero);
            }
            catch (AbandonedMutexException)
            {
                ownsMutex = true;
            }

            if (!ownsMutex)
            {
                if (selfUpdateCleanup)
                {
                    UpdaterLog.WriteInfo("The updated updater could not acquire the single-instance lock for staged-helper cleanup.");
                    return 2;
                }

                MessageBox.Show(
                    "Muff Mode Updater is already running.",
                    "Muff Mode Updater",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                return 0;
            }

            if (selfUpdateCleanup
                && InstallationManager.TryHandleSelfUpdateCleanupStartup(args, out selfUpdateExitCode)
                && selfUpdateExitCode != 0)
            {
                return selfUpdateExitCode;
            }

            Application.Run(new MainForm());
        }
        finally
        {
            if (ownsMutex)
            {
                singleInstanceMutex.ReleaseMutex();
            }
        }

        return 0;
    }
}
