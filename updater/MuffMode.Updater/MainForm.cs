using System.ComponentModel;

namespace MuffMode.Updater;

internal sealed class MainForm : Form
{
    private const string OtherInstallLocationText = "Other location - choose or paste a folder";

    private readonly AppSettings _settings;
    private readonly GitHubReleaseClient _releaseClient = new();

    private readonly ComboBox _installSourceComboBox;
    private readonly TextBox _installPathTextBox;
    private readonly Button _browseButton;
    private readonly Label _statusLabel;
    private readonly Label _latestVersionLabel;
    private readonly Label _localVersionLabel;
    private readonly TextBox _changelogTextBox;
    private readonly CheckBox _autoLaunchCheckBox;
    private readonly ProgressBar _progressBar;
    private readonly Button _updateButton;
    private readonly Button _refreshButton;
    private readonly Button _shortcutsButton;
    private readonly Button _launchButton;
    private readonly Button _quitButton;

    private ReleaseInfo? _latestRelease;
    private LocalInstallVersion _localVersion = new(null, "Unknown", "Not checked");
    private CancellationTokenSource? _operationCancellation;
    private bool _updatingInstallSource;
    private bool _cancelAllowed = true;
    private bool _closeAfterOperation;
    private bool _busy;

    public MainForm()
    {
        _settings = InstallationManager.LoadSettings();

        Text = "Muff Mode Updater & Launcher";
        var appIcon = LoadApplicationIcon();
        if (appIcon is not null)
        {
            Icon = appIcon;
        }

        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(840, 650);
        ClientSize = new Size(920, 700);
        BackColor = Color.FromArgb(26, 29, 30);
        ForeColor = Color.FromArgb(235, 238, 232);
        Font = new Font("Segoe UI", 10F);

        var titleLabel = new Label
        {
            AutoSize = false,
            Dock = DockStyle.Fill,
            Text = "Muff Mode Updater & Launcher",
            Font = new Font("Segoe UI Semibold", 21F),
            ForeColor = Color.FromArgb(238, 189, 87)
        };

        _statusLabel = new Label
        {
            AutoSize = false,
            Dock = DockStyle.Fill,
            Text = "Ready.",
            ForeColor = Color.FromArgb(198, 211, 196)
        };

        _installSourceComboBox = new ComboBox
        {
            Anchor = AnchorStyles.Left | AnchorStyles.Right,
            DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = Color.FromArgb(42, 47, 49),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        _installSourceComboBox.SelectedIndexChanged += (_, _) => ApplySelectedInstallSource();

        _installPathTextBox = new TextBox
        {
            Anchor = AnchorStyles.Left | AnchorStyles.Right,
            BackColor = Color.FromArgb(42, 47, 49),
            ForeColor = Color.White,
            BorderStyle = BorderStyle.FixedSingle
        };
        _installPathTextBox.TextChanged += (_, _) =>
        {
            SelectInstallSourceForPath(_installPathTextBox.Text);
            UpdateLocalInstallState();
            UpdateButtonStates();
        };

        _browseButton = CreateButton("Browse");
        _browseButton.Click += (_, _) => BrowseForInstallPath();

        _latestVersionLabel = CreateInfoLabel("Latest: not checked");
        _localVersionLabel = CreateInfoLabel("Local: not checked");

        _changelogTextBox = new TextBox
        {
            Dock = DockStyle.Fill,
            Multiline = true,
            ReadOnly = true,
            ScrollBars = ScrollBars.Vertical,
            BackColor = Color.FromArgb(18, 20, 21),
            ForeColor = Color.FromArgb(229, 232, 225),
            BorderStyle = BorderStyle.FixedSingle,
            Font = new Font("Consolas", 9.5F),
            Text = "Release notes will appear here after the GitHub check completes."
        };

        _autoLaunchCheckBox = new CheckBox
        {
            AutoSize = true,
            Text = "Launch Quake II after update completes",
            Checked = _settings.AutoLaunchAfterUpdate,
            ForeColor = Color.FromArgb(235, 238, 232)
        };
        _autoLaunchCheckBox.CheckedChanged += (_, _) => SaveCurrentSettings();

        _progressBar = new ProgressBar
        {
            Dock = DockStyle.Fill,
            Minimum = 0,
            Maximum = 100,
            Value = 0
        };

        _updateButton = CreateButton("Update", Color.FromArgb(80, 132, 88));
        _updateButton.Click += async (_, _) => await RunUpdateAsync();

        _refreshButton = CreateButton("Refresh", Color.FromArgb(72, 88, 96));
        _refreshButton.Click += async (_, _) => await RefreshReleaseAsync();

        _shortcutsButton = CreateButton("Shortcuts", Color.FromArgb(84, 88, 105));
        _shortcutsButton.Click += (_, _) => OfferShortcutCreation();

        _launchButton = CreateButton("Launch", Color.FromArgb(140, 94, 57));
        _launchButton.Click += (_, _) => LaunchGame();

        _quitButton = CreateButton("Quit", Color.FromArgb(66, 69, 70));
        _quitButton.Click += (_, _) => RequestCancelOrClose();

        BuildLayout(titleLabel);

        PopulateInstallSources();
        var initialInstallPath = InstallationManager.ResolveInitialInstallPath(_settings.InstallPath)
            ?? _settings.InstallPath
            ?? "";
        _installPathTextBox.Text = initialInstallPath;
        SelectInstallSourceForPath(initialInstallPath);
        UpdateLocalInstallState();
        UpdateButtonStates();

        Load += async (_, _) => await RefreshReleaseAsync();
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        if (_busy)
        {
            e.Cancel = true;
            _closeAfterOperation = true;
            RequestCancelCurrentOperation();
            return;
        }

        SaveCurrentSettings();
        _operationCancellation?.Dispose();
        _releaseClient.Dispose();
        base.OnClosing(e);
    }

    private void BuildLayout(Label titleLabel)
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(22),
            ColumnCount = 1,
            RowCount = 7
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 106));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));

        root.Controls.Add(titleLabel, 0, 0);
        root.Controls.Add(_statusLabel, 0, 1);
        root.Controls.Add(BuildPathRow(), 0, 2);
        root.Controls.Add(BuildVersionRow(), 0, 3);
        root.Controls.Add(_changelogTextBox, 0, 4);
        root.Controls.Add(BuildProgressRow(), 0, 5);
        root.Controls.Add(BuildActionRow(), 0, 6);

        Controls.Add(root);
    }

    private Control BuildPathRow()
    {
        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 3,
            RowCount = 3
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 138));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 104));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 26));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
        layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));

        var label = CreateInfoLabel("Install location");
        layout.Controls.Add(label, 0, 0);
        layout.SetColumnSpan(label, 3);
        layout.Controls.Add(_installSourceComboBox, 0, 1);
        layout.SetColumnSpan(_installSourceComboBox, 3);
        layout.Controls.Add(_installPathTextBox, 0, 2);
        layout.SetColumnSpan(_installPathTextBox, 2);
        layout.Controls.Add(_browseButton, 2, 2);
        return layout;
    }

    private Control BuildVersionRow()
    {
        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        layout.Controls.Add(_latestVersionLabel, 0, 0);
        layout.Controls.Add(_localVersionLabel, 1, 0);
        return layout;
    }

    private Control BuildProgressRow()
    {
        var layout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1
        };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58));
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
        layout.Controls.Add(_autoLaunchCheckBox, 0, 0);
        layout.Controls.Add(_progressBar, 1, 0);
        return layout;
    }

    private Control BuildActionRow()
    {
        var layout = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false
        };

        layout.Controls.Add(_quitButton);
        layout.Controls.Add(_launchButton);
        layout.Controls.Add(_shortcutsButton);
        layout.Controls.Add(_refreshButton);
        layout.Controls.Add(_updateButton);
        return layout;
    }

    private static Label CreateInfoLabel(string text) => new()
    {
        AutoSize = false,
        Dock = DockStyle.Fill,
        Text = text,
        ForeColor = Color.FromArgb(207, 216, 205),
        TextAlign = ContentAlignment.MiddleLeft
    };

    private static Button CreateButton(string text) => CreateButton(text, Color.FromArgb(68, 73, 75));

    private static Button CreateButton(string text, Color backColor) => new()
    {
        Text = text,
        Width = 108,
        Height = 34,
        Margin = new Padding(8, 6, 0, 6),
        FlatStyle = FlatStyle.Flat,
        BackColor = backColor,
        ForeColor = Color.White,
        UseVisualStyleBackColor = false
    };

    private static Icon? LoadApplicationIcon()
    {
        try
        {
            return Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        }
        catch
        {
            return null;
        }
    }

    private void PopulateInstallSources()
    {
        _updatingInstallSource = true;
        try
        {
            _installSourceComboBox.Items.Clear();
            foreach (var candidate in InstallationManager.GetInstallCandidates(_settings.InstallPath))
            {
                _installSourceComboBox.Items.Add(candidate);
            }

            _installSourceComboBox.Items.Add(OtherInstallLocationText);
            _installSourceComboBox.SelectedItem = _installSourceComboBox.Items.Count > 1
                ? _installSourceComboBox.Items[0]
                : OtherInstallLocationText;
        }
        finally
        {
            _updatingInstallSource = false;
        }
    }

    private void ApplySelectedInstallSource()
    {
        if (_updatingInstallSource)
        {
            return;
        }

        if (_installSourceComboBox.SelectedItem is InstallCandidate candidate)
        {
            _installPathTextBox.Text = candidate.Path;
            SaveCurrentSettings();
            SetStatus($"{candidate.Source} install selected.");
        }
    }

    private void SelectInstallSourceForPath(string? path)
    {
        if (_updatingInstallSource)
        {
            return;
        }

        _updatingInstallSource = true;
        try
        {
            var normalizedPath = InstallationManager.ResolveInstallRoot(path) ?? path;
            foreach (var item in _installSourceComboBox.Items)
            {
                if (item is InstallCandidate candidate && PathsEqual(candidate.Path, normalizedPath))
                {
                    _installSourceComboBox.SelectedItem = candidate;
                    return;
                }
            }

            _installSourceComboBox.SelectedItem = OtherInstallLocationText;
        }
        finally
        {
            _updatingInstallSource = false;
        }
    }

    private static bool PathsEqual(string? left, string? right)
    {
        if (string.IsNullOrWhiteSpace(left) || string.IsNullOrWhiteSpace(right))
        {
            return false;
        }

        try
        {
            var normalizedLeft = Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var normalizedRight = Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            return string.Equals(normalizedLeft, normalizedRight, StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return string.Equals(left.Trim(), right.Trim(), StringComparison.OrdinalIgnoreCase);
        }
    }

    private void OfferShortcutCreation()
    {
        using var dialog = new Form
        {
            Text = "Create Updater & Launcher Shortcuts",
            StartPosition = FormStartPosition.CenterParent,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MinimizeBox = false,
            MaximizeBox = false,
            ClientSize = new Size(420, 170),
            BackColor = BackColor,
            ForeColor = ForeColor,
            Font = Font,
            Icon = this.Icon
        };

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(18),
            ColumnCount = 1,
            RowCount = 4
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        var heading = CreateInfoLabel("Create shortcuts to this updater and launcher.");
        var desktopShortcut = CreateShortcutCheckBox("Desktop shortcut", !ShortcutManager.DesktopShortcutExists);
        var startMenuShortcut = CreateShortcutCheckBox("Start menu shortcut", !ShortcutManager.StartMenuShortcutExists);
        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.RightToLeft,
            WrapContents = false
        };
        var createButton = CreateButton("Create", Color.FromArgb(80, 132, 88));
        createButton.DialogResult = DialogResult.OK;
        var cancelButton = CreateButton("Cancel");
        cancelButton.DialogResult = DialogResult.Cancel;
        buttons.Controls.Add(cancelButton);
        buttons.Controls.Add(createButton);

        root.Controls.Add(heading, 0, 0);
        root.Controls.Add(desktopShortcut, 0, 1);
        root.Controls.Add(startMenuShortcut, 0, 2);
        root.Controls.Add(buttons, 0, 3);
        dialog.Controls.Add(root);
        dialog.AcceptButton = createButton;
        dialog.CancelButton = cancelButton;

        if (dialog.ShowDialog(this) != DialogResult.OK)
        {
            return;
        }

        if (!desktopShortcut.Checked && !startMenuShortcut.Checked)
        {
            SetStatus("No shortcuts selected.");
            return;
        }

        try
        {
            ShortcutManager.CreateShortcuts(desktopShortcut.Checked, startMenuShortcut.Checked);
            SetStatus("Updater and launcher shortcuts created.");
        }
        catch (Exception ex)
        {
            SetStatus("Could not create shortcuts.");
            MessageBox.Show(this, ex.Message, "Shortcut creation failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private CheckBox CreateShortcutCheckBox(string text, bool isChecked) => new()
    {
        AutoSize = true,
        Text = text,
        Checked = isChecked,
        ForeColor = ForeColor
    };

    private async Task RefreshReleaseAsync()
    {
        if (_busy)
        {
            return;
        }

        _operationCancellation?.Dispose();
        _operationCancellation = new CancellationTokenSource();
        var cancellationToken = _operationCancellation.Token;
        SetBusy(true, "Checking GitHub for the latest Muff Mode release...");
        _progressBar.Value = 0;

        try
        {
            _latestRelease = await _releaseClient.GetLatestReleaseAsync(cancellationToken);
            _changelogTextBox.Text = BuildChangelogText(_latestRelease);
            UpdateLocalInstallState();
            UpdateVersionLabels();
            SetStatus(GetReleaseStatusText());
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            SetStatus("Release check cancelled.");
        }
        catch (Exception ex)
        {
            _latestRelease = null;
            _changelogTextBox.Text = ex.Message;
            SetStatus("Could not check GitHub releases.");
        }
        finally
        {
            SetBusy(false);
            _operationCancellation.Dispose();
            _operationCancellation = null;
            CloseIfRequested();
        }
    }

    private async Task RunUpdateAsync()
    {
        if (_busy || _latestRelease is null)
        {
            return;
        }

        var installPath = _installPathTextBox.Text.Trim();
        if (!InstallationManager.IsValidInstallPath(installPath))
        {
            MessageBox.Show(this, "Select the Quake 2 installation folder, its rerelease folder, or its baseq2 folder.", "Install path required", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        SaveCurrentSettings();
        _operationCancellation?.Dispose();
        _operationCancellation = new CancellationTokenSource();
        var cancellationToken = _operationCancellation.Token;
        SetBusy(true, $"Preparing to install Muff Mode {_latestRelease.Version}...");
        _progressBar.Value = 0;

        var downloadDirectory = Path.Combine(Path.GetTempPath(), "MuffModeUpdater", "downloads");
        string? downloadedZip = null;

        try
        {
            var progress = new Progress<UpdaterProgress>(ReportProgress);
            downloadedZip = await _releaseClient.DownloadReleaseAssetAsync(_latestRelease, downloadDirectory, progress, cancellationToken);
            await InstallationManager.SyncReleaseToInstallAsync(_latestRelease, downloadedZip, installPath, progress, cancellationToken);

            UpdateLocalInstallState();
            UpdateVersionLabels();
            SetStatus($"Updated Muff Mode to {_latestRelease.Version}.");

            if (_autoLaunchCheckBox.Checked && !_closeAfterOperation)
            {
                LaunchGame();
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            SetStatus("Update cancelled before completion.");
        }
        catch (Exception ex)
        {
            SetStatus("Update failed.");
            MessageBox.Show(this, ex.Message, "Muff Mode update failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally
        {
            TryDeleteFile(downloadedZip);
            SetBusy(false);
            _operationCancellation.Dispose();
            _operationCancellation = null;
            CloseIfRequested();
        }
    }

    private void BrowseForInstallPath()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Select the Quake 2 folder, its rerelease folder, or its baseq2 folder.",
            InitialDirectory = Directory.Exists(_installPathTextBox.Text) ? _installPathTextBox.Text : Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            UseDescriptionForTitle = true
        };

        if (dialog.ShowDialog(this) != DialogResult.OK)
        {
            return;
        }

        _installPathTextBox.Text = dialog.SelectedPath;
        SaveCurrentSettings();
        UpdateLocalInstallState();
        UpdateVersionLabels();
        SetStatus(GetReleaseStatusText());
    }

    private void LaunchGame()
    {
        try
        {
            SaveCurrentSettings();
            InstallationManager.LaunchGame(_installPathTextBox.Text.Trim());
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Could not launch Quake II", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void UpdateLocalInstallState()
    {
        _localVersion = InstallationManager.ReadLocalVersion(_installPathTextBox.Text.Trim());
        UpdateVersionLabels();
    }

    private void UpdateVersionLabels()
    {
        _latestVersionLabel.Text = _latestRelease is null
            ? "Latest: not checked"
            : $"Latest: {_latestRelease.Version} ({_latestRelease.TagName})";

        _localVersionLabel.Text = $"Local: {_localVersion.DisplayText}";
        UpdateButtonStates();
    }

    private string GetReleaseStatusText()
    {
        if (_latestRelease is null)
        {
            return "No release information is loaded.";
        }

        if (!InstallationManager.IsValidInstallPath(_installPathTextBox.Text.Trim()))
        {
            return "Choose a detected or custom Quake II install folder, then update or launch.";
        }

        if (_localVersion.Version is null)
        {
            return $"Muff Mode {_latestRelease.Version} is available. Local version is unknown, so update is recommended.";
        }

        var compare = _latestRelease.Version.CompareTo(_localVersion.Version.Value);
        if (compare > 0)
        {
            return $"Update available: {_localVersion.Version} -> {_latestRelease.Version}.";
        }

        if (compare == 0)
        {
            return $"Muff Mode {_latestRelease.Version} is already installed.";
        }

        return $"Local Muff Mode {_localVersion.Version} is newer than GitHub release {_latestRelease.Version}.";
    }

    private bool IsUpdateRequired()
    {
        if (_latestRelease is null)
        {
            return false;
        }

        return _localVersion.Version is null
            || _latestRelease.Version.CompareTo(_localVersion.Version.Value) > 0;
    }

    private void SetBusy(bool busy, string? status = null)
    {
        _busy = busy;
        if (busy)
        {
            _cancelAllowed = true;
            _closeAfterOperation = false;
        }

        if (!string.IsNullOrWhiteSpace(status))
        {
            SetStatus(status);
        }

        _installPathTextBox.Enabled = !busy;
        _browseButton.Enabled = !busy;
        _autoLaunchCheckBox.Enabled = !busy;
        UseWaitCursor = busy;
        UpdateButtonStates();
    }

    private void UpdateButtonStates()
    {
        var validInstallPath = InstallationManager.IsValidInstallPath(_installPathTextBox.Text.Trim());
        _updateButton.Enabled = !_busy && validInstallPath && IsUpdateRequired();
        _refreshButton.Enabled = !_busy;
        _shortcutsButton.Enabled = !_busy;
        _launchButton.Enabled = !_busy && validInstallPath;
        if (_busy)
        {
            var cancellationRequested = _operationCancellation?.IsCancellationRequested ?? false;
            _quitButton.Text = _cancelAllowed ? "Cancel" : "Installing";
            _quitButton.Enabled = _cancelAllowed && !cancellationRequested;
        }
        else
        {
            _quitButton.Text = "Quit";
            _quitButton.Enabled = true;
        }
    }

    private void ReportProgress(UpdaterProgress progress)
    {
        _cancelAllowed = progress.CanCancel;
        UpdateButtonStates();
        SetStatus(progress.Message);
        if (progress.Percentage is { } percentage)
        {
            _progressBar.Value = Math.Clamp(percentage, _progressBar.Minimum, _progressBar.Maximum);
        }
    }

    private void SetStatus(string text) => _statusLabel.Text = text;

    private void SaveCurrentSettings()
    {
        try
        {
            _settings.InstallPath = InstallationManager.ResolveInstallRoot(_installPathTextBox.Text.Trim())
                ?? _installPathTextBox.Text.Trim();
            _settings.AutoLaunchAfterUpdate = _autoLaunchCheckBox.Checked;
            InstallationManager.SaveSettings(_settings);
        }
        catch (Exception ex)
        {
            SetStatus($"Could not save settings: {ex.Message}");
        }
    }

    private void RequestCancelOrClose()
    {
        if (_busy)
        {
            RequestCancelCurrentOperation();
            return;
        }

        Close();
    }

    private void RequestCancelCurrentOperation()
    {
        if (_operationCancellation is null || _operationCancellation.IsCancellationRequested)
        {
            return;
        }

        if (!_cancelAllowed)
        {
            SetStatus(_closeAfterOperation
                ? "Finishing installation; the updater will close when done."
                : "Finishing installation. This step cannot be cancelled safely.");
            UpdateButtonStates();
            return;
        }

        _operationCancellation.Cancel();
        _quitButton.Enabled = false;
        _quitButton.Text = "Cancelling";
        SetStatus("Cancelling current operation...");
    }

    private void CloseIfRequested()
    {
        if (!_closeAfterOperation)
        {
            return;
        }

        _closeAfterOperation = false;
        Close();
    }

    private static string BuildChangelogText(ReleaseInfo release)
    {
        var lines = new List<string>
        {
            release.Name,
            release.HtmlUrl,
            release.IsPrerelease ? "Prerelease" : "Release",
            release.PublishedAt is null ? "" : $"Published: {release.PublishedAt:yyyy-MM-dd HH:mm} UTC",
            "",
            string.IsNullOrWhiteSpace(release.Changelog) ? "No changelog text was published with this release." : release.Changelog.Trim()
        };

        return string.Join(Environment.NewLine, lines.Where(line => line is not null));
    }

    private static void TryDeleteFile(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        try
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch
        {
            // Temp cleanup failure is non-fatal.
        }
    }
}
