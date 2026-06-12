#ifndef AppVersion
  #error AppVersion must be passed by scripts/release.ps1.
#endif
#ifndef Channel
  #define Channel "beta"
#endif
#ifndef ReleaseLabel
  #define ReleaseLabel "Muff Mode"
#endif
#ifndef PackageRoot
  #error PackageRoot must be passed by scripts/release.ps1.
#endif
#ifndef OutputDir
  #error OutputDir must be passed by scripts/release.ps1.
#endif
#ifndef InstallerBaseName
  #error InstallerBaseName must be passed by scripts/release.ps1.
#endif
#ifndef LauncherIconFile
  #define LauncherIconFile "..\..\updater\MuffMode.Updater\Assets\MuffModeLauncher.ico"
#endif

[Setup]
AppId={{C2A5B6D7-EC58-4F1D-A43A-18F8C79C0D91}
AppName=Muff Mode
AppVersion={#AppVersion}
AppVerName={#ReleaseLabel}
DefaultDirName={code:GetDefaultInstallDir}
AppendDefaultDirName=no
DisableProgramGroupPage=yes
DirExistsWarning=no
OutputDir={#OutputDir}
OutputBaseFilename={#InstallerBaseName}
SetupIconFile={#LauncherIconFile}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UsePreviousAppDir=no
UsePreviousPrivileges=no
Uninstallable=no
SetupLogging=yes
CloseApplications=no
RestartIfNeededByRun=no

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: "desktopshortcut"; Description: "Create a desktop shortcut for Muff Mode Updater & Launcher"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "startmenushortcut"; Description: "Create a Start menu shortcut for Muff Mode Updater & Launcher"

[Icons]
Name: "{autodesktop}\Muff Mode Updater & Launcher"; Filename: "{app}\MuffModeUpdater.exe"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: desktopshortcut; Check: UpdaterExists
Name: "{autoprograms}\Muff Mode Updater & Launcher"; Filename: "{app}\MuffModeUpdater.exe"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: startmenushortcut; Check: UpdaterExists

[Run]
Filename: "{app}\README.html"; Description: "Open the Muff Mode install and usage guide"; Flags: shellexec postinstall skipifsilent unchecked
Filename: "{app}\MuffModeUpdater.exe"; Description: "Launch Muff Mode Updater & Launcher"; Flags: nowait postinstall skipifsilent unchecked; Check: UpdaterExists

[Messages]
WelcomeLabel2=This will install {#ReleaseLabel} for Quake II Remastered. Choose a detected store install or another location, confirm the outer Quake II folder, and the installer will place Muff Mode files under rerelease\baseq2.
SelectDirDesc=Select the outer Quake II installation folder. Do not select the rerelease or baseq2 subfolder.

[Code]
var
  StorePage: TInputOptionWizardPage;

function FirstExistingOrFallback(FirstPath: String; SecondPath: String; ThirdPath: String; FallbackPath: String): String;
begin
  if DirExists(FirstPath) then
    Result := FirstPath
  else if DirExists(SecondPath) then
    Result := SecondPath
  else if DirExists(ThirdPath) then
    Result := ThirdPath
  else
    Result := FallbackPath;
end;

function GetSteamInstallDir(): String;
var
  SteamPath: String;
begin
  Result := ExpandConstant('{sd}\Program Files (x86)\Steam\steamapps\common\Quake 2');

  if not RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath', SteamPath) then
    RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamPath);

  if SteamPath <> '' then
  begin
    StringChangeEx(SteamPath, '/', '\', True);
    Result := AddBackslash(SteamPath) + 'steamapps\common\Quake 2';
  end;
end;

function GetEpicInstallDir(): String;
begin
  Result := FirstExistingOrFallback(
    ExpandConstant('{sd}\Program Files\Epic Games\Quake 2'),
    ExpandConstant('{sd}\Program Files\Epic Games\QuakeII'),
    ExpandConstant('{sd}\Program Files\Epic Games\Quake2'),
    ExpandConstant('{sd}\Program Files\Epic Games\Quake 2'));
end;

function GetGogInstallDir(): String;
begin
  Result := FirstExistingOrFallback(
    ExpandConstant('{sd}\GOG Games\Quake II'),
    ExpandConstant('{sd}\Program Files (x86)\GOG Galaxy\Games\Quake II'),
    ExpandConstant('{sd}\Program Files\GOG Galaxy\Games\Quake II'),
    ExpandConstant('{sd}\GOG Games\Quake II'));
end;

function GetDefaultInstallDir(Param: String): String;
begin
  Result := GetSteamInstallDir();
end;

function LooksLikeRereleaseDir(Path: String): Boolean;
begin
  Result := (Lowercase(ExtractFileName(RemoveBackslash(Path))) = 'rerelease') and
    DirExists(AddBackslash(Path) + 'baseq2');
end;

function LooksLikeBaseq2Dir(Path: String): Boolean;
var
  ParentDir: String;
begin
  Result := False;

  if Lowercase(ExtractFileName(RemoveBackslash(Path))) <> 'baseq2' then
    Exit;

  ParentDir := ExtractFileDir(RemoveBackslash(Path));
  Result := Lowercase(ExtractFileName(RemoveBackslash(ParentDir))) = 'rerelease';
end;

function LooksLikeQuake2Root(Path: String): Boolean;
begin
  Result := DirExists(AddBackslash(Path) + 'rerelease\baseq2');
end;

function UpdaterExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\MuffModeUpdater.exe'));
end;

function DescribeInstallChoice(StoreName: String; InstallDir: String): String;
begin
  if LooksLikeQuake2Root(InstallDir) then
    Result := StoreName + ' detected - ' + InstallDir
  else
    Result := StoreName + ' default path - ' + InstallDir;
end;

function GetInitialStoreIndex(): Integer;
begin
  if LooksLikeQuake2Root(GetSteamInstallDir()) then
    Result := 0
  else if LooksLikeQuake2Root(GetEpicInstallDir()) then
    Result := 1
  else if LooksLikeQuake2Root(GetGogInstallDir()) then
    Result := 2
  else
    Result := 0;
end;

procedure InitializeWizard();
begin
  StorePage := CreateInputOptionPage(
    wpWelcome,
    'Choose Quake II Install Location',
    'Where should Muff Mode install?',
    'Detected installs are shown with their folder. If your game lives somewhere else, choose the other-location option and browse on the next page.',
    True,
    False);

  StorePage.Add(DescribeInstallChoice('Steam', GetSteamInstallDir()));
  StorePage.Add(DescribeInstallChoice('Epic Online Store', GetEpicInstallDir()));
  StorePage.Add(DescribeInstallChoice('GOG', GetGogInstallDir()));
  StorePage.Add('Other location - choose a folder on the next page');
  StorePage.SelectedValueIndex := GetInitialStoreIndex();
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  SelectedDir: String;
  ParentDir: String;
begin
  Result := True;

  if CurPageID = StorePage.ID then
  begin
    if StorePage.Values[0] then
      WizardForm.DirEdit.Text := GetSteamInstallDir()
    else if StorePage.Values[1] then
      WizardForm.DirEdit.Text := GetEpicInstallDir()
    else if StorePage.Values[2] then
      WizardForm.DirEdit.Text := GetGogInstallDir();
  end
  else if CurPageID = wpSelectDir then
  begin
    SelectedDir := WizardDirValue;

    if LooksLikeRereleaseDir(SelectedDir) then
    begin
      ParentDir := ExtractFileDir(RemoveBackslash(SelectedDir));
      WizardForm.DirEdit.Text := ParentDir;
      MsgBox(
        'Muff Mode installs into the outer Quake II folder, not the rerelease subfolder. The destination has been adjusted to:' + #13#10#13#10 + ParentDir,
        mbInformation,
        MB_OK);
    end
    else if LooksLikeBaseq2Dir(SelectedDir) then
    begin
      ParentDir := ExtractFileDir(ExtractFileDir(RemoveBackslash(SelectedDir)));
      WizardForm.DirEdit.Text := ParentDir;
      MsgBox(
        'Muff Mode installs into the outer Quake II folder, not the baseq2 subfolder. The destination has been adjusted to:' + #13#10#13#10 + ParentDir,
        mbInformation,
        MB_OK);
    end
    else if not LooksLikeQuake2Root(SelectedDir) then
    begin
      if MsgBox(
        'This folder does not look like the outer Quake II Remastered folder because rerelease\baseq2 was not found.' + #13#10#13#10 +
        'Install here anyway?',
        mbConfirmation,
        MB_YESNO) = IDNO then
        Result := False;
    end;
  end;
end;

function GetUniqueBackupFile(BackupDir: String): String;
var
  Counter: Integer;
  Timestamp: String;
  Candidate: String;
begin
  Counter := 0;
  Timestamp := GetDateTimeString('yyyy-mm-dd-hhnnss', '-', '-');

  repeat
    if Counter = 0 then
      Candidate := AddBackslash(BackupDir) + 'game_x64-' + Timestamp + '.dll'
    else
      Candidate := AddBackslash(BackupDir) + 'game_x64-' + Timestamp + '-' + IntToStr(Counter) + '.dll';

    Counter := Counter + 1;
  until not FileExists(Candidate);

  Result := Candidate;
end;

function BackupExistingGameDll(): String;
var
  ExistingDll: String;
  BackupDir: String;
  BackupFile: String;
begin
  Result := '';
  ExistingDll := AddBackslash(WizardDirValue) + 'rerelease\baseq2\game_x64.dll';

  if FileExists(ExistingDll) then
  begin
    BackupDir := AddBackslash(WizardDirValue) + 'rerelease\baseq2\MuffModeBackups';
    if not ForceDirectories(BackupDir) then
    begin
      Result := 'Could not create the Muff Mode backup folder:' + #13#10#13#10 + BackupDir;
      Exit;
    end;

    BackupFile := GetUniqueBackupFile(BackupDir);
    if not CopyFile(ExistingDll, BackupFile, False) then
    begin
      Result :=
        'Could not back up the existing Quake II game_x64.dll before installing Muff Mode.' + #13#10#13#10 +
        'Close Quake II, check folder permissions, then run this installer again.' + #13#10#13#10 +
        'Backup target:' + #13#10 + BackupFile;
      Exit;
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := BackupExistingGameDll();
end;
