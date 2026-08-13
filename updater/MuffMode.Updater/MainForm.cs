using System.ComponentModel;
using System.Diagnostics;
using System.Text;

namespace MuffMode.Updater;

internal sealed class MainForm : Form
{
    private const string OtherInstallLocationText = "Other location - choose or paste a folder";
    private const int MaxDisplayedChangelogCharacters = 120_000;
    private const int MaxDialogMessageCharacters = 4_000;
    private const int MaxStatusCharacters = 240;

    private readonly AppSettings _settings;
    private readonly GitHubReleaseClient _releaseClient = new();

    private readonly ComboBox _installSourceComboBox;
    private readonly TextBox _installPathTextBox;
    private readonly Button _browseButton;
    private readonly Label _statusLabel;
    private readonly Label _latestVersionLabel;
    private readonly Label _localVersionLabel;
    private readonly RichTextBox _changelogTextBox;
    private readonly CheckBox _autoLaunchCheckBox;
    private readonly CheckBox _includePrereleaseCheckBox;
    private readonly ProgressBar _progressBar;
    private readonly Button _updateButton;
    private readonly Button _refreshButton;
    private readonly Button _releasePageButton;
    private readonly Button _shortcutsButton;
    private readonly Button _diagnosticsButton;
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
            // Without this the ampersand is read as a mnemonic and renders as "Updater _Launcher".
            UseMnemonic = false,
            Font = new Font("Segoe UI Semibold", 21F),
            ForeColor = Color.FromArgb(238, 189, 87)
        };

        _statusLabel = new Label
        {
            AutoSize = false,
            Dock = DockStyle.Fill,
            Text = "Ready.",
            // Status text quotes install paths and release titles, which may contain an ampersand.
            UseMnemonic = false,
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

        _changelogTextBox = new RichTextBox
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            DetectUrls = true,
            WordWrap = true,
            ScrollBars = RichTextBoxScrollBars.Vertical,
            BackColor = Color.FromArgb(18, 20, 21),
            ForeColor = Color.FromArgb(229, 232, 225),
            BorderStyle = BorderStyle.FixedSingle,
            Font = new Font("Segoe UI", 9.5F),
            Text = "Release notes will appear here after the GitHub check completes."
        };
        _changelogTextBox.LinkClicked += (_, eventArgs) => OpenReleaseNotesLink(eventArgs.LinkText ?? "");

        _autoLaunchCheckBox = new CheckBox
        {
            AutoSize = true,
            Text = "Launch Quake II after update completes",
            Checked = _settings.AutoLaunchAfterUpdate,
            ForeColor = Color.FromArgb(235, 238, 232)
        };
        _autoLaunchCheckBox.CheckedChanged += (_, _) => SaveCurrentSettings();

        _includePrereleaseCheckBox = new CheckBox
        {
            AutoSize = true,
            Text = "Include prereleases",
            Checked = _settings.IncludePrereleases,
            ForeColor = Color.FromArgb(235, 238, 232),
            Margin = new Padding(22, 0, 0, 0)
        };
        _includePrereleaseCheckBox.CheckedChanged += async (_, _) =>
        {
            SaveCurrentSettings();
            if (!_busy)
            {
                await RefreshReleaseAsync();
            }
        };

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

        _releasePageButton = CreateButton("Release", Color.FromArgb(74, 86, 104));
        _releasePageButton.Click += (_, _) => OpenReleasePage();

        _shortcutsButton = CreateButton("Shortcuts", Color.FromArgb(84, 88, 105));
        _shortcutsButton.Click += (_, _) => OfferShortcutCreation();

        _diagnosticsButton = CreateButton("Diagnostics", Color.FromArgb(74, 82, 92));
        _diagnosticsButton.Click += (_, _) => OpenDiagnosticsLog();

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
        var options = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false
        };
        options.Controls.Add(_autoLaunchCheckBox);
        options.Controls.Add(_includePrereleaseCheckBox);
        layout.Controls.Add(options, 0, 0);
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
        layout.Controls.Add(_diagnosticsButton);
        layout.Controls.Add(_shortcutsButton);
        layout.Controls.Add(_releasePageButton);
        layout.Controls.Add(_refreshButton);
        layout.Controls.Add(_updateButton);
        return layout;
    }

    private static Label CreateInfoLabel(string text) => new()
    {
        AutoSize = false,
        Dock = DockStyle.Fill,
        Text = text,
        // These labels report install paths and release titles, which may contain an ampersand.
        UseMnemonic = false,
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
            UpdaterLog.WriteException("Shortcut creation failed.", ex);
            SetStatus("Could not create shortcuts.");
            ShowMessage(ex.Message, "Shortcut creation failed", MessageBoxIcon.Error);
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
        var includePrereleases = _includePrereleaseCheckBox.Checked;
        SetBusy(true, includePrereleases
            ? "Checking GitHub for the latest Muff Mode release, including prereleases..."
            : "Checking GitHub for the latest stable Muff Mode release...");
        _progressBar.Value = 0;

        try
        {
            _latestRelease = await _releaseClient.GetLatestReleaseAsync(includePrereleases, cancellationToken);
            var markdown = string.IsNullOrWhiteSpace(_latestRelease.Changelog)
                ? "No changelog text was published with this release."
                : TruncateForDisplay(
                    _latestRelease.Changelog.Trim(),
                    MaxDisplayedChangelogCharacters,
                    "[Release notes truncated for display.]");
            ReleaseNotesRenderer.Render(_changelogTextBox, _latestRelease, markdown);
            UpdateLocalInstallState();
            UpdateVersionLabels();
            SetStatus(GetReleaseStatusText());
            UpdaterLog.WriteInfo($"Selected release {_latestRelease.Version} ({_latestRelease.AssetName}, prerelease={_latestRelease.IsPrerelease}).");
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            SetStatus("Release check cancelled.");
        }
        catch (Exception ex)
        {
            _latestRelease = null;
            UpdaterLog.WriteException("Could not check GitHub releases.", ex);
            _changelogTextBox.Text = BuildErrorText("Could not check GitHub releases.", ex.Message);
            UpdateVersionLabels();
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

        var installPath = GetResolvedInstallPathForOperation();
        if (installPath is null || !InstallationManager.IsValidInstallPath(installPath))
        {
            ShowMessage(
                "Select the Quake 2 installation folder, its rerelease folder, or its baseq2 folder.",
                "Install path required",
                MessageBoxIcon.Warning);
            return;
        }

        if (IsSameVersionReinstall() && !ConfirmReinstall())
        {
            SetStatus("Reinstall cancelled.");
            return;
        }

        SaveCurrentSettings();
        _operationCancellation?.Dispose();
        _operationCancellation = new CancellationTokenSource();
        var cancellationToken = _operationCancellation.Token;
        SetBusy(true, GetInstallPreparationStatusText(_latestRelease));
        _progressBar.Value = 0;

        var downloadDirectory = Path.Combine(Path.GetTempPath(), "MuffModeUpdater", "downloads");
        string? downloadedZip = null;

        try
        {
            var progress = new Progress<UpdaterProgress>(ReportProgress);
            downloadedZip = await _releaseClient.DownloadReleaseAssetAsync(_latestRelease, downloadDirectory, progress, cancellationToken);
            var installResult = await InstallationManager.SyncReleaseToInstallAsync(
                _latestRelease,
                downloadedZip,
                installPath,
                progress,
                cancellationToken);

            if (installResult.SelfUpdateHandoffStarted)
            {
                SetStatus("Restarting the updated Muff Mode Updater...");
                UpdaterLog.WriteInfo(
                    $"Installed Muff Mode {_latestRelease.Version}; exiting for verified updater self-replacement.");
                _closeAfterOperation = true;
                return;
            }

            UpdateLocalInstallState();
            UpdateVersionLabels();
            SetStatus($"Updated Muff Mode to {_latestRelease.Version}.");
            UpdaterLog.WriteInfo($"Installed Muff Mode {_latestRelease.Version} from {_latestRelease.AssetName}.");

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
            UpdaterLog.WriteException("Muff Mode update failed.", ex);
            SetStatus("Update failed.");
            ShowMessage(ex.Message, "Muff Mode update failed", MessageBoxIcon.Error);
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
            InitialDirectory = GetBrowseInitialDirectory(),
            UseDescriptionForTitle = true
        };

        if (dialog.ShowDialog(this) != DialogResult.OK)
        {
            return;
        }

        _installPathTextBox.Text = InstallationManager.ResolveInstallRoot(dialog.SelectedPath) ?? dialog.SelectedPath;
        SaveCurrentSettings();
        UpdateLocalInstallState();
        UpdateVersionLabels();
        SetStatus(GetReleaseStatusText());
    }

    private void LaunchGame()
    {
        try
        {
            var installPath = GetResolvedInstallPathForOperation() ?? _installPathTextBox.Text.Trim();
            SaveCurrentSettings();
            var launchedTarget = InstallationManager.LaunchGame(installPath);
            SetStatus($"Launched {Path.GetFileName(launchedTarget)}.");
            UpdaterLog.WriteInfo($"Launched Quake II target: {launchedTarget}");
        }
        catch (Exception ex)
        {
            UpdaterLog.WriteException("Could not launch Quake II.", ex);
            ShowMessage(ex.Message, "Could not launch Quake II", MessageBoxIcon.Error);
        }
    }

    private void OpenDiagnosticsLog()
    {
        try
        {
            UpdaterLog.WriteInfo("Opening updater diagnostics.");
            Process.Start(new ProcessStartInfo(UpdaterLog.LogPath)
            {
                UseShellExecute = true
            });
            SetStatus("Opened updater diagnostics.");
        }
        catch (Exception ex)
        {
            UpdaterLog.WriteException("Could not open updater diagnostics.", ex);
            ShowMessage(ex.Message, "Could not open diagnostics", MessageBoxIcon.Error);
        }
    }

    private void OpenReleasePage()
    {
        if (_latestRelease is null)
        {
            return;
        }

        try
        {
            Process.Start(new ProcessStartInfo(_latestRelease.HtmlUrl)
            {
                UseShellExecute = true
            });
            SetStatus("Opened release page.");
            UpdaterLog.WriteInfo($"Opened release page: {_latestRelease.HtmlUrl}");
        }
        catch (Exception ex)
        {
            UpdaterLog.WriteException("Could not open release page.", ex);
            ShowMessage(ex.Message, "Could not open release page", MessageBoxIcon.Error);
        }
    }

    private void OpenReleaseNotesLink(string linkText)
    {
        if (!Uri.TryCreate(linkText, UriKind.Absolute, out var uri)
            || (uri.Scheme != Uri.UriSchemeHttps && uri.Scheme != Uri.UriSchemeHttp))
        {
            SetStatus("The selected release-note link is not a supported web address.");
            return;
        }

        try
        {
            Process.Start(new ProcessStartInfo(uri.AbsoluteUri)
            {
                UseShellExecute = true
            });
            SetStatus("Opened release-note link.");
            UpdaterLog.WriteInfo($"Opened release-note link: {uri.AbsoluteUri}");
        }
        catch (Exception ex)
        {
            UpdaterLog.WriteException("Could not open release-note link.", ex);
            ShowMessage(ex.Message, "Could not open link", MessageBoxIcon.Error);
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
            : $"Latest: {_latestRelease.Version} ({_latestRelease.Channel}{(_latestRelease.IsPrerelease ? ", prerelease" : "")})";

        _localVersionLabel.Text = string.IsNullOrWhiteSpace(_localVersion.Source)
            ? $"Local: {_localVersion.DisplayText}"
            : $"Local: {_localVersion.DisplayText} ({_localVersion.Source})";
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
            return $"Muff Mode {_latestRelease.Version} is already installed. Reinstall is available for repair.";
        }

        return $"Local Muff Mode {_localVersion.Version} is newer than GitHub release {_latestRelease.Version}.";
    }

    private bool IsInstallAllowed()
    {
        if (_latestRelease is null)
        {
            return false;
        }

        return _localVersion.Version is null
            || _latestRelease.Version.CompareTo(_localVersion.Version.Value) >= 0;
    }

    private string GetInstallButtonText()
    {
        if (_latestRelease is null || _localVersion.Version is null)
        {
            return "Update";
        }

        return _latestRelease.Version.CompareTo(_localVersion.Version.Value) == 0
            ? "Reinstall"
            : "Update";
    }

    private string GetInstallPreparationStatusText(ReleaseInfo release)
    {
        if (_localVersion.Version is not null && release.Version.CompareTo(_localVersion.Version.Value) == 0)
        {
            return $"Preparing to reinstall Muff Mode {release.Version}...";
        }

        return $"Preparing to install Muff Mode {release.Version}...";
    }

    private bool IsSameVersionReinstall()
    {
        return _latestRelease is not null
            && _localVersion.Version is not null
            && _latestRelease.Version.CompareTo(_localVersion.Version.Value) == 0;
    }

    private bool ConfirmReinstall()
    {
        if (_latestRelease is null)
        {
            return false;
        }

        return MessageBox.Show(
            this,
            $"Reinstall Muff Mode {_latestRelease.Version}? This will redownload the package and rewrite the installed files.",
            "Confirm reinstall",
            MessageBoxButtons.OKCancel,
            MessageBoxIcon.Question) == DialogResult.OK;
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

        _installSourceComboBox.Enabled = !busy;
        _installPathTextBox.Enabled = !busy;
        _browseButton.Enabled = !busy;
        _autoLaunchCheckBox.Enabled = !busy;
        _includePrereleaseCheckBox.Enabled = !busy;
        UseWaitCursor = busy;
        UpdateButtonStates();
    }

    private void UpdateButtonStates()
    {
        var validInstallPath = InstallationManager.IsValidInstallPath(_installPathTextBox.Text.Trim());
        _updateButton.Text = GetInstallButtonText();
        _updateButton.Enabled = !_busy && validInstallPath && IsInstallAllowed();
        _refreshButton.Enabled = !_busy;
        _releasePageButton.Enabled = !_busy && _latestRelease is not null;
        _shortcutsButton.Enabled = !_busy;
        _diagnosticsButton.Enabled = !_busy;
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

    private void SetStatus(string text) => _statusLabel.Text = TruncateSingleLine(CollapseWhitespace(text), MaxStatusCharacters);

    private void SaveCurrentSettings()
    {
        try
        {
            _settings.InstallPath = InstallationManager.ResolveInstallRoot(_installPathTextBox.Text.Trim())
                ?? _installPathTextBox.Text.Trim();
            _settings.AutoLaunchAfterUpdate = _autoLaunchCheckBox.Checked;
            _settings.IncludePrereleases = _includePrereleaseCheckBox.Checked;
            InstallationManager.SaveSettings(_settings);
        }
        catch (Exception ex)
        {
            UpdaterLog.WriteException("Could not save updater settings.", ex);
            SetStatus($"Could not save settings: {ex.Message}");
        }
    }

    private string? GetResolvedInstallPathForOperation()
    {
        var rawInstallPath = _installPathTextBox.Text.Trim();
        var resolvedInstallPath = InstallationManager.ResolveInstallRoot(rawInstallPath);
        if (!string.IsNullOrWhiteSpace(resolvedInstallPath)
            && !string.Equals(rawInstallPath, resolvedInstallPath, StringComparison.OrdinalIgnoreCase))
        {
            _installPathTextBox.Text = resolvedInstallPath;
        }

        return resolvedInstallPath;
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

    private static string BuildErrorText(string heading, string message)
    {
        var detail = string.IsNullOrWhiteSpace(message)
            ? "No details were provided."
            : TruncateForDisplay(message.Trim(), MaxDisplayedChangelogCharacters, "[Error details truncated for display.]");

        return string.Join(Environment.NewLine, heading, "", detail);
    }

    private string GetBrowseInitialDirectory()
    {
        if (TryGetExistingDirectory(_installPathTextBox.Text, out var currentDirectory))
        {
            return currentDirectory;
        }

        if (TryGetExistingDirectory(
            InstallationManager.ResolveInstallRoot(_installPathTextBox.Text),
            out var resolvedDirectory))
        {
            return resolvedDirectory;
        }

        if (TryGetExistingDirectory(
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            out var programFilesDirectory))
        {
            return programFilesDirectory;
        }

        return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    }

    private static bool TryGetExistingDirectory(string? path, out string directory)
    {
        directory = "";
        if (string.IsNullOrWhiteSpace(path))
        {
            return false;
        }

        try
        {
            var fullPath = Path.GetFullPath(path);
            if (Directory.Exists(fullPath) && !IsReparsePoint(fullPath))
            {
                directory = fullPath;
                return true;
            }
        }
        catch
        {
            return false;
        }

        return false;
    }

    private void ShowMessage(string message, string caption, MessageBoxIcon icon)
    {
        var detail = string.IsNullOrWhiteSpace(message)
            ? "No details were provided."
            : message.Trim();

        if (icon == MessageBoxIcon.Error && File.Exists(UpdaterLog.LogPath))
        {
            detail = $"{detail}{Environment.NewLine}{Environment.NewLine}Log: {UpdaterLog.LogPath}";
        }

        var displayMessage = TruncateForDisplay(detail, MaxDialogMessageCharacters, "[Message truncated.]");

        MessageBox.Show(this, displayMessage, caption, MessageBoxButtons.OK, icon);
    }

    private static string TruncateForDisplay(string text, int maxCharacters, string truncationNotice)
    {
        return text.Length <= maxCharacters
            ? text
            : $"{text[..maxCharacters]}{Environment.NewLine}{Environment.NewLine}{truncationNotice}";
    }

    private static string CollapseWhitespace(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return "";
        }

        var builder = new StringBuilder(text.Length);
        var previousWasWhitespace = false;
        foreach (var value in text.Trim())
        {
            if (char.IsWhiteSpace(value))
            {
                if (!previousWasWhitespace)
                {
                    builder.Append(' ');
                }

                previousWasWhitespace = true;
                continue;
            }

            builder.Append(value);
            previousWasWhitespace = false;
        }

        return builder.ToString();
    }

    private static string TruncateSingleLine(string text, int maxCharacters)
    {
        if (text.Length <= maxCharacters)
        {
            return text;
        }

        return maxCharacters <= 3
            ? text[..maxCharacters]
            : $"{text[..(maxCharacters - 3)]}...";
    }

    private static void TryDeleteFile(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        try
        {
            if (File.Exists(path) && !IsReparsePoint(path))
            {
                File.Delete(path);
            }
        }
        catch
        {
            // Temp cleanup failure is non-fatal.
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
}
