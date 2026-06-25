namespace MuffMode.Updater;

internal static class Program
{
    private const string SingleInstanceMutexName = @"Local\DarkMatterProductions.MuffMode.Updater";

    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        using var singleInstanceMutex = new Mutex(false, SingleInstanceMutexName);
        var ownsMutex = false;
        try
        {
            try
            {
                ownsMutex = singleInstanceMutex.WaitOne(TimeSpan.Zero);
            }
            catch (AbandonedMutexException)
            {
                ownsMutex = true;
            }

            if (!ownsMutex)
            {
                MessageBox.Show(
                    "Muff Mode Updater is already running.",
                    "Muff Mode Updater",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                return;
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
    }
}
