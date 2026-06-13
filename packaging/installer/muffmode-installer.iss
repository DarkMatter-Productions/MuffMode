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
AppPublisher=DarkMatter Productions
AppPublisherURL=https://github.com/DarkMatter-Productions/MuffMode
AppSupportURL=https://github.com/DarkMatter-Productions/MuffMode/issues
AppUpdatesURL=https://github.com/DarkMatter-Productions/MuffMode/releases
DefaultDirName={code:GetDefaultInstallDir}
AppendDefaultDirName=no
DisableProgramGroupPage=yes
DirExistsWarning=no
OutputDir={#OutputDir}
OutputBaseFilename={#InstallerBaseName}
SetupIconFile={#LauncherIconFile}
LicenseFile={#PackageRoot}\LICENSE
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UsePreviousAppDir=no
UsePreviousPrivileges=no
Uninstallable=no
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
AllowCancelDuringInstall=no
RestartIfNeededByRun=no
VersionInfoCompany=DarkMatter Productions
VersionInfoDescription=Muff Mode installer for Quake II Remastered
VersionInfoProductName=Muff Mode
VersionInfoProductVersion={#AppVersion}
VersionInfoVersion={#AppVersion}

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: "desktopshortcut"; Description: "Create a desktop shortcut for Muff Mode Updater & Launcher"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "startmenushortcut"; Description: "Create a Start menu shortcut for Muff Mode Updater & Launcher"

[Icons]
Name: "{autodesktop}\Muff Mode Updater & Launcher"; Filename: "{app}\MuffModeUpdater.exe"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: desktopshortcut; Check: UpdaterExists
Name: "{autoprograms}\Muff Mode\Muff Mode Updater & Launcher"; Filename: "{app}\MuffModeUpdater.exe"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: startmenushortcut; Check: UpdaterExists

[Run]
Filename: "{app}\README.html"; Description: "Open the Muff Mode install and usage guide"; Flags: shellexec postinstall skipifsilent unchecked
Filename: "{app}\MuffModeUpdater.exe"; Description: "Launch Muff Mode Updater & Launcher"; Flags: nowait postinstall skipifsilent unchecked; Check: UpdaterExists

[Messages]
WelcomeLabel2=This will install {#ReleaseLabel} for Quake II Remastered. Choose a detected store install or another location, confirm the outer Quake II folder, and the installer will place Muff Mode files under rerelease\baseq2.
SelectDirDesc=Select the outer Quake II installation folder. Do not select the rerelease or baseq2 subfolder.

[Code]
var
  StorePage: TInputOptionWizardPage;
  SteamInstallDirResolved: Boolean;
  EpicInstallDirResolved: Boolean;
  GogInstallDirResolved: Boolean;
  CachedSteamInstallDir: String;
  CachedEpicInstallDir: String;
  CachedGogInstallDir: String;

function NormalizeDetectedPath(Path: String): String;
var
  IsUncPath: Boolean;
begin
  Result := Path;
  StringChangeEx(Result, '/', '\', True);
  IsUncPath := (Length(Result) >= 2) and (Copy(Result, 1, 2) = '\\');
  StringChangeEx(Result, '\\', '\', True);
  if IsUncPath and ((Length(Result) < 2) or (Copy(Result, 1, 2) <> '\\')) then
    Result := '\' + Result;
end;

function LooksLikeQuake2Name(Value: String): Boolean;
var
  Normalized: String;
begin
  Normalized := Lowercase(Value);
  StringChangeEx(Normalized, '_', ' ', True);
  StringChangeEx(Normalized, '-', ' ', True);

  Result :=
    (Pos('quake 2', Normalized) > 0) or
    (Pos('quake2', Normalized) > 0) or
    (Pos('quake ii', Normalized) > 0);
end;

function IsQuake2RootPath(Path: String): Boolean;
begin
  Result := DirExists(AddBackslash(Path) + 'rerelease\baseq2');
end;

function FirstValidOrFallback(FirstPath: String; SecondPath: String; ThirdPath: String; FallbackPath: String): String;
begin
  if IsQuake2RootPath(FirstPath) then
    Result := FirstPath
  else if IsQuake2RootPath(SecondPath) then
    Result := SecondPath
  else if IsQuake2RootPath(ThirdPath) then
    Result := ThirdPath
  else
    Result := FallbackPath;
end;

function ReadJsonStringValue(Json: String; PropertyName: String): String;
var
  Key: String;
  KeyPos: Integer;
  ColonPos: Integer;
  ValueStart: Integer;
  ValueEnd: Integer;
begin
  Result := '';
  Key := '"' + PropertyName + '"';
  KeyPos := Pos(Key, Json);
  if KeyPos = 0 then
    Exit;

  ColonPos := KeyPos + Length(Key);
  while (ColonPos <= Length(Json)) and (Json[ColonPos] <> ':') do
    ColonPos := ColonPos + 1;
  if ColonPos > Length(Json) then
    Exit;

  ValueStart := ColonPos + 1;
  while (ValueStart <= Length(Json)) and (Json[ValueStart] <> '"') do
    ValueStart := ValueStart + 1;
  if ValueStart > Length(Json) then
    Exit;

  ValueEnd := ValueStart + 1;
  while ValueEnd <= Length(Json) do
  begin
    if (Json[ValueEnd] = '"') and ((ValueEnd = 1) or (Json[ValueEnd - 1] <> '\')) then
      Break;
    ValueEnd := ValueEnd + 1;
  end;
  if ValueEnd > Length(Json) then
    Exit;

  Result := Copy(Json, ValueStart + 1, ValueEnd - ValueStart - 1);
  Result := NormalizeDetectedPath(Result);
end;

function ReadNextQuotedValue(var Text: String; var QuotedValue: String): Boolean;
var
  ValueStart: Integer;
  ValueEnd: Integer;
begin
  Result := False;
  QuotedValue := '';

  ValueStart := 1;
  while (ValueStart <= Length(Text)) and (Text[ValueStart] <> '"') do
    ValueStart := ValueStart + 1;
  if ValueStart > Length(Text) then
    Exit;

  ValueEnd := ValueStart + 1;
  while ValueEnd <= Length(Text) do
  begin
    if (Text[ValueEnd] = '"') and (Text[ValueEnd - 1] <> '\') then
      Break;
    ValueEnd := ValueEnd + 1;
  end;
  if ValueEnd > Length(Text) then
    Exit;

  QuotedValue := Copy(Text, ValueStart + 1, ValueEnd - ValueStart - 1);
  Delete(Text, 1, ValueEnd);
  Result := True;
end;

function ReadNextLegacySteamLibraryPath(var Text: String; var PathValue: String): Boolean;
var
  KeyName: String;
  ValueText: String;
begin
  Result := False;
  PathValue := '';

  while ReadNextQuotedValue(Text, KeyName) do
  begin
    if not ReadNextQuotedValue(Text, ValueText) then
      Exit;

    if (StrToIntDef(KeyName, -1) >= 0) and (Pos('\', ValueText) > 0) then
    begin
      PathValue := NormalizeDetectedPath(ValueText);
      Result := PathValue <> '';
      Exit;
    end;
  end;
end;

function ReadNextVdfPath(var Text: String; var PathValue: String): Boolean;
var
  KeyPos: Integer;
  ValueStart: Integer;
  ValueEnd: Integer;
begin
  Result := False;
  PathValue := '';
  KeyPos := Pos('"path"', Text);
  if KeyPos = 0 then
  begin
    Result := ReadNextLegacySteamLibraryPath(Text, PathValue);
    Exit;
  end;

  Delete(Text, 1, KeyPos + Length('"path"') - 1);
  ValueStart := 1;
  while (ValueStart <= Length(Text)) and (Text[ValueStart] <> '"') do
    ValueStart := ValueStart + 1;
  if ValueStart > Length(Text) then
    Exit;

  ValueEnd := ValueStart + 1;
  while ValueEnd <= Length(Text) do
  begin
    if (Text[ValueEnd] = '"') and ((ValueEnd = 1) or (Text[ValueEnd - 1] <> '\')) then
      Break;
    ValueEnd := ValueEnd + 1;
  end;
  if ValueEnd > Length(Text) then
    Exit;

  PathValue := Copy(Text, ValueStart + 1, ValueEnd - ValueStart - 1);
  PathValue := NormalizeDetectedPath(PathValue);
  Delete(Text, 1, ValueEnd);
  Result := PathValue <> '';
end;

function GetSteamInstallDir(): String;
var
  SteamPath: String;
  LibraryBytes: AnsiString;
  LibraryText: String;
  LibraryPath: String;
  CandidatePath: String;
begin
  if SteamInstallDirResolved then
  begin
    Result := CachedSteamInstallDir;
    Exit;
  end;
  SteamInstallDirResolved := True;
  Result := ExpandConstant('{sd}\Program Files (x86)\Steam\steamapps\common\Quake 2');

  if not RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath', SteamPath) then
    if not RegQueryStringValue(HKLM, 'SOFTWARE\Valve\Steam', 'InstallPath', SteamPath) then
      if not RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamPath) then
        SteamPath := '';

  if SteamPath <> '' then
  begin
    SteamPath := NormalizeDetectedPath(SteamPath);
    CandidatePath := AddBackslash(SteamPath) + 'steamapps\common\Quake 2';
    if IsQuake2RootPath(CandidatePath) then
    begin
      Result := CandidatePath;
      CachedSteamInstallDir := Result;
      Exit;
    end;

    if LoadStringFromFile(AddBackslash(SteamPath) + 'steamapps\libraryfolders.vdf', LibraryBytes) then
    begin
      LibraryText := LibraryBytes;
      while ReadNextVdfPath(LibraryText, LibraryPath) do
      begin
        CandidatePath := AddBackslash(LibraryPath) + 'steamapps\common\Quake 2';
        if IsQuake2RootPath(CandidatePath) then
        begin
          Result := CandidatePath;
          CachedSteamInstallDir := Result;
          Exit;
        end;
      end;
    end;

    Result := AddBackslash(SteamPath) + 'steamapps\common\Quake 2';
  end;

  CachedSteamInstallDir := Result;
end;

function GetEpicInstallDir(): String;
var
  ManifestsRoot: String;
  FindRec: TFindRec;
  ManifestBytes: AnsiString;
  ManifestText: String;
  DisplayName: String;
  AppName: String;
  InstallLocation: String;
begin
  if EpicInstallDirResolved then
  begin
    Result := CachedEpicInstallDir;
    Exit;
  end;
  EpicInstallDirResolved := True;
  Result := FirstValidOrFallback(
    ExpandConstant('{sd}\Program Files\Epic Games\Quake 2'),
    ExpandConstant('{sd}\Program Files\Epic Games\QuakeII'),
    ExpandConstant('{sd}\Program Files\Epic Games\Quake2'),
    ExpandConstant('{sd}\Program Files\Epic Games\Quake 2'));

  ManifestsRoot := ExpandConstant('{commonappdata}\Epic\EpicGamesLauncher\Data\Manifests');
  if not DirExists(ManifestsRoot) then
  begin
    CachedEpicInstallDir := Result;
    Exit;
  end;

  if FindFirst(AddBackslash(ManifestsRoot) + '*.item', FindRec) then
  begin
    try
      repeat
        if LoadStringFromFile(AddBackslash(ManifestsRoot) + FindRec.Name, ManifestBytes) then
        begin
          ManifestText := ManifestBytes;
          DisplayName := ReadJsonStringValue(ManifestText, 'DisplayName');
          AppName := ReadJsonStringValue(ManifestText, 'AppName');
          InstallLocation := ReadJsonStringValue(ManifestText, 'InstallLocation');

          if (InstallLocation <> '') and
             (LooksLikeQuake2Name(DisplayName) or LooksLikeQuake2Name(AppName) or LooksLikeQuake2Name(InstallLocation)) and
             IsQuake2RootPath(InstallLocation) then
          begin
            Result := InstallLocation;
            CachedEpicInstallDir := Result;
            Exit;
          end;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;

  CachedEpicInstallDir := Result;
end;

function GetGogInstallDirFromRegistry(RootKey: HKEY; KeyName: String): String;
var
  SubKeyNames: TArrayOfString;
  Index: Integer;
  GameName: String;
  InstallPath: String;
begin
  Result := '';

  if not RegGetSubkeyNames(RootKey, KeyName, SubKeyNames) then
    Exit;

  for Index := 0 to GetArrayLength(SubKeyNames) - 1 do
  begin
    GameName := '';
    InstallPath := '';
    RegQueryStringValue(RootKey, KeyName + '\' + SubKeyNames[Index], 'gameName', GameName);
    if GameName = '' then
      RegQueryStringValue(RootKey, KeyName + '\' + SubKeyNames[Index], 'title', GameName);
    if GameName = '' then
      RegQueryStringValue(RootKey, KeyName + '\' + SubKeyNames[Index], 'name', GameName);
    RegQueryStringValue(RootKey, KeyName + '\' + SubKeyNames[Index], 'path', InstallPath);

    InstallPath := NormalizeDetectedPath(InstallPath);
    if (InstallPath <> '') and
       (LooksLikeQuake2Name(GameName) or LooksLikeQuake2Name(InstallPath)) and
       IsQuake2RootPath(InstallPath) then
    begin
      Result := InstallPath;
      Exit;
    end;
  end;
end;

function GetGogInstallDir(): String;
var
  RegistryPath: String;
begin
  if GogInstallDirResolved then
  begin
    Result := CachedGogInstallDir;
    Exit;
  end;
  GogInstallDirResolved := True;
  Result := FirstValidOrFallback(
    ExpandConstant('{sd}\GOG Games\Quake II'),
    ExpandConstant('{sd}\Program Files (x86)\GOG Galaxy\Games\Quake II'),
    ExpandConstant('{sd}\Program Files\GOG Galaxy\Games\Quake II'),
    ExpandConstant('{sd}\GOG Games\Quake II'));

  RegistryPath := GetGogInstallDirFromRegistry(HKLM, 'SOFTWARE\GOG.com\Games');
  if RegistryPath = '' then
    RegistryPath := GetGogInstallDirFromRegistry(HKLM, 'SOFTWARE\WOW6432Node\GOG.com\Games');
  if RegistryPath = '' then
    RegistryPath := GetGogInstallDirFromRegistry(HKCU, 'SOFTWARE\GOG.com\Games');

  if RegistryPath <> '' then
    Result := RegistryPath;

  CachedGogInstallDir := Result;
end;

function GetDefaultInstallDir(Param: String): String;
begin
  if IsQuake2RootPath(GetSteamInstallDir()) then
    Result := GetSteamInstallDir()
  else if IsQuake2RootPath(GetEpicInstallDir()) then
    Result := GetEpicInstallDir()
  else if IsQuake2RootPath(GetGogInstallDir()) then
    Result := GetGogInstallDir()
  else
    Result := ExpandConstant('{sd}\Games\Quake 2');
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
  Result := IsQuake2RootPath(Path);
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
    Result := 3;
end;

procedure InitializeWizard();
begin
  StorePage := CreateInputOptionPage(
    wpWelcome,
    'Choose Quake II Install Location',
    'Where should Muff Mode install?',
    'Detected installs are shown with their folder. If your game lives somewhere else, choose the Other location option and browse on the next page.',
    True,
    False);

  StorePage.Add(DescribeInstallChoice('Steam', GetSteamInstallDir()));
  StorePage.Add(DescribeInstallChoice('Epic Games Store', GetEpicInstallDir()));
  StorePage.Add(DescribeInstallChoice('GOG', GetGogInstallDir()));
  StorePage.Add('Other location - browse for your Quake II folder');
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
    if WizardSilent() then
      Exit;

    if StorePage.Values[0] then
      WizardForm.DirEdit.Text := GetSteamInstallDir()
    else if StorePage.Values[1] then
      WizardForm.DirEdit.Text := GetEpicInstallDir()
    else if StorePage.Values[2] then
      WizardForm.DirEdit.Text := GetGogInstallDir()
    else
      WizardForm.DirEdit.Text := ExpandConstant('{sd}\Games\Quake 2');
  end
  else if CurPageID = wpSelectDir then
  begin
    SelectedDir := WizardDirValue;

    if LooksLikeRereleaseDir(SelectedDir) then
    begin
      ParentDir := ExtractFileDir(RemoveBackslash(SelectedDir));
      WizardForm.DirEdit.Text := ParentDir;
      if WizardSilent() then
        Log('Adjusted silent install destination from rerelease to outer Quake II folder: ' + ParentDir)
      else
        MsgBox(
          'Muff Mode installs into the outer Quake II folder, not the rerelease subfolder. The destination has been adjusted to:' + #13#10#13#10 + ParentDir,
          mbInformation,
          MB_OK);
    end
    else if LooksLikeBaseq2Dir(SelectedDir) then
    begin
      ParentDir := ExtractFileDir(ExtractFileDir(RemoveBackslash(SelectedDir)));
      WizardForm.DirEdit.Text := ParentDir;
      if WizardSilent() then
        Log('Adjusted silent install destination from baseq2 to outer Quake II folder: ' + ParentDir)
      else
        MsgBox(
          'Muff Mode installs into the outer Quake II folder, not the baseq2 subfolder. The destination has been adjusted to:' + #13#10#13#10 + ParentDir,
          mbInformation,
          MB_OK);
    end
    else if not LooksLikeQuake2Root(SelectedDir) then
    begin
      if WizardSilent() then
      begin
        Log('Silent install destination does not contain rerelease\baseq2: ' + SelectedDir);
      end
      else if MsgBox(
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

function ValidateInstallTargetForInstall(): String;
begin
  Result := '';

  if WizardSilent() and not LooksLikeQuake2Root(WizardDirValue) then
  begin
    Result :=
      'Silent installation requires /DIR to point at the outer Quake II Remastered folder containing rerelease\baseq2.' + #13#10#13#10 +
      'Destination:' + #13#10 + WizardDirValue;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := ValidateInstallTargetForInstall();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  BackupError: String;
begin
  if CurStep = ssInstall then
  begin
    BackupError := BackupExistingGameDll();
    if BackupError <> '' then
    begin
      SuppressibleMsgBox(BackupError, mbError, MB_OK, IDOK);
      Abort;
    end;
  end;
end;
