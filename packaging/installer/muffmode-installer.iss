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
#define Baseq2RelativePath "rerelease\baseq2"
#define MapsRelativePath "rerelease\maps"
#define GameDllRelativePath Baseq2RelativePath + "\game_x64.dll"
#define OriginalReadmesRelativePath Baseq2RelativePath + "\docs\muffmode\maps\original-readmes"

#if DirExists(PackageRoot) == 0
  #error PackageRoot must point at an existing staged release package directory.
#endif
#if FileExists(PackageRoot + "\README.html") == 0
  #error PackageRoot is missing README.html.
#endif
#ifdef IncludeLocalizedReadmes
  #if FileExists(PackageRoot + "\README.de.html") == 0
    #error PackageRoot is missing README.de.html.
  #endif
  #if FileExists(PackageRoot + "\README.pl.html") == 0
    #error PackageRoot is missing README.pl.html.
  #endif
  #if FileExists(PackageRoot + "\README.fr.html") == 0
    #error PackageRoot is missing README.fr.html.
  #endif
  #if FileExists(PackageRoot + "\README.hu.html") == 0
    #error PackageRoot is missing README.hu.html.
  #endif
  #if FileExists(PackageRoot + "\README.bg.html") == 0
    #error PackageRoot is missing README.bg.html.
  #endif
#endif
#if FileExists(PackageRoot + "\README.md") == 0
  #error PackageRoot is missing README.md.
#endif
#if FileExists(PackageRoot + "\CHANGELOG.md") == 0
  #error PackageRoot is missing CHANGELOG.md.
#endif
#if FileExists(PackageRoot + "\LICENSE") == 0
  #error PackageRoot is missing LICENSE.
#endif
#if FileExists(PackageRoot + "\THIRD_PARTY_NOTICES.md") == 0
  #error PackageRoot is missing THIRD_PARTY_NOTICES.md.
#endif
#if FileExists(PackageRoot + "\MuffModeUpdater.exe") == 0
  #error PackageRoot is missing MuffModeUpdater.exe.
#endif
#if FileExists(PackageRoot + "\" + GameDllRelativePath) == 0
  #error PackageRoot is missing rerelease\baseq2\game_x64.dll.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\muffmode-version.json") == 0
  #error PackageRoot is missing rerelease\baseq2\muffmode-version.json.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\muffmode.version") == 0
  #error PackageRoot is missing rerelease\baseq2\muffmode.version.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\README.md") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\README.md.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\2box4-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\2box4-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\aerowalk-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\aerowalk-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\broken2-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\broken2-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\fleshref-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\fleshref-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\grind-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\grind-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm1-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm1-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm2-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm2-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm3-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm3-readme.txt.
#endif
#if FileExists(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm5-readme.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\docs\muffmode\maps\original-readmes\ztn2dm5-readme.txt.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\CONFIGS_README.md") == 0
  #error PackageRoot is missing rerelease\baseq2\CONFIGS_README.md.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\muffmode-map-cycle.example.txt") == 0
  #error PackageRoot is missing rerelease\baseq2\muffmode-map-cycle.example.txt.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\muffmode-map-pool.example.json") == 0
  #error PackageRoot is missing rerelease\baseq2\muffmode-map-pool.example.json.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\server-base.cfg") == 0
  #error PackageRoot is missing rerelease\baseq2\server-base.cfg.
#endif
#if FileExists(PackageRoot + "\rerelease\baseq2\factories.cfg") == 0
  #error PackageRoot is missing rerelease\baseq2\factories.cfg.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-aerow.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-aerow.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-coldzero.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-coldzero.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-crucible.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-crucible.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-kmachine.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-kmachine.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-powertrip.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-powertrip.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-rage.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-rage.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-railgun101.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-railgun101.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-reclamation.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-reclamation.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\mm-recycler.bsp") == 0
  #error PackageRoot is missing rerelease\maps\mm-recycler.bsp.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\2box4.ent") == 0
  #error PackageRoot is missing rerelease\maps\2box4.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\aerowalk.ent") == 0
  #error PackageRoot is missing rerelease\maps\aerowalk.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\grom_dm3.ent") == 0
  #error PackageRoot is missing rerelease\maps\grom_dm3.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\kmachine.ent") == 0
  #error PackageRoot is missing rerelease\maps\kmachine.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\koldduel1.ent") == 0
  #error PackageRoot is missing rerelease\maps\koldduel1.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\paradm4.ent") == 0
  #error PackageRoot is missing rerelease\maps\paradm4.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\trdm04a.ent") == 0
  #error PackageRoot is missing rerelease\maps\trdm04a.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\vd6dm2.ent") == 0
  #error PackageRoot is missing rerelease\maps\vd6dm2.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\ven_dm2.ent") == 0
  #error PackageRoot is missing rerelease\maps\ven_dm2.ent.
#endif
#if FileExists(PackageRoot + "\" + MapsRelativePath + "\ztn2dm5.ent") == 0
  #error PackageRoot is missing rerelease\maps\ztn2dm5.ent.
#endif
#if FileExists(LauncherIconFile) == 0
  #error LauncherIconFile must point at an existing .ico file.
#endif
#define PackageGameDllHash GetSHA256OfFile(PackageRoot + "\" + GameDllRelativePath)
#define PackageUpdaterHash GetSHA256OfFile(PackageRoot + "\MuffModeUpdater.exe")
#define PackageReadmeHash GetSHA256OfFile(PackageRoot + "\README.html")
#ifdef IncludeLocalizedReadmes
  #define PackageReadmeDeHash GetSHA256OfFile(PackageRoot + "\README.de.html")
  #define PackageReadmePlHash GetSHA256OfFile(PackageRoot + "\README.pl.html")
  #define PackageReadmeFrHash GetSHA256OfFile(PackageRoot + "\README.fr.html")
  #define PackageReadmeHuHash GetSHA256OfFile(PackageRoot + "\README.hu.html")
  #define PackageReadmeBgHash GetSHA256OfFile(PackageRoot + "\README.bg.html")
#endif
#define PackageAssetReadmeHash GetSHA256OfFile(PackageRoot + "\README.md")
#define PackageChangelogHash GetSHA256OfFile(PackageRoot + "\CHANGELOG.md")
#define PackageLicenseHash GetSHA256OfFile(PackageRoot + "\LICENSE")
#define PackageNoticesHash GetSHA256OfFile(PackageRoot + "\THIRD_PARTY_NOTICES.md")
#define PackageVersionManifestHash GetSHA256OfFile(PackageRoot + "\rerelease\baseq2\muffmode-version.json")
#define PackageOriginalReadmesIndexHash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\README.md")
#define PackageOriginalReadme2Box4Hash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\2box4-readme.txt")
#define PackageOriginalReadmeAerowalkHash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\aerowalk-readme.txt")
#define PackageOriginalReadmeBroken2Hash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\broken2-readme.txt")
#define PackageOriginalReadmeFleshrefHash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\fleshref-readme.txt")
#define PackageOriginalReadmeGrindHash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\grind-readme.txt")
#define PackageOriginalReadmeZtn2dm1Hash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm1-readme.txt")
#define PackageOriginalReadmeZtn2dm2Hash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm2-readme.txt")
#define PackageOriginalReadmeZtn2dm3Hash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm3-readme.txt")
#define PackageOriginalReadmeZtn2dm5Hash GetSHA256OfFile(PackageRoot + "\" + OriginalReadmesRelativePath + "\ztn2dm5-readme.txt")
#define PackageConfigReadmeHash GetSHA256OfFile(PackageRoot + "\rerelease\baseq2\CONFIGS_README.md")
#define PackageMapCycleExampleHash GetSHA256OfFile(PackageRoot + "\rerelease\baseq2\muffmode-map-cycle.example.txt")
#define PackageMapPoolExampleHash GetSHA256OfFile(PackageRoot + "\rerelease\baseq2\muffmode-map-pool.example.json")
#define PackageServerBaseHash GetSHA256OfFile(PackageRoot + "\rerelease\baseq2\server-base.cfg")
#define PackageFactoriesHash GetSHA256OfFile(PackageRoot + "\rerelease\baseq2\factories.cfg")
#define PackageMapAerowalkHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-aerow.bsp")
#define PackageMapColdzeroHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-coldzero.bsp")
#define PackageMapCrucibleHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-crucible.bsp")
#define PackageMapKmachineHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-kmachine.bsp")
#define PackageMapPowertripHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-powertrip.bsp")
#define PackageMapRageHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-rage.bsp")
#define PackageMapRailgun101Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-railgun101.bsp")
#define PackageMapReclamationHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-reclamation.bsp")
#define PackageMapRecyclerHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\mm-recycler.bsp")
#define PackageEntity2Box4Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\2box4.ent")
#define PackageEntityAerowalkHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\aerowalk.ent")
#define PackageEntityGromDm3Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\grom_dm3.ent")
#define PackageEntityKmachineHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\kmachine.ent")
#define PackageEntityKoldduel1Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\koldduel1.ent")
#define PackageEntityParadm4Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\paradm4.ent")
#define PackageEntityTrdm04aHash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\trdm04a.ent")
#define PackageEntityVd6dm2Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\vd6dm2.ent")
#define PackageEntityVenDm2Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\ven_dm2.ent")
#define PackageEntityZtn2dm5Hash GetSHA256OfFile(PackageRoot + "\" + MapsRelativePath + "\ztn2dm5.ent")

[Setup]
AppId={{C2A5B6D7-EC58-4F1D-A43A-18F8C79C0D91}
AppName=Muff Mode
AppVersion={#AppVersion}
AppVerName={#ReleaseLabel}
AppPublisher=DarkMatter Productions
AppPublisherURL=https://github.com/DarkMatter-Productions/MuffMode
AppSupportURL=https://github.com/DarkMatter-Productions/MuffMode/issues
AppUpdatesURL=https://github.com/DarkMatter-Productions/MuffMode/releases
AppContact=https://github.com/DarkMatter-Productions/MuffMode/issues
AppComments=Installs Muff Mode into Quake II Remastered rerelease\baseq2 and backs up existing server configs and game_x64.dll.
AppReadmeFile={app}\README.html
DefaultDirName={code:GetDefaultInstallDir}
AppendDefaultDirName=no
DisableProgramGroupPage=yes
DirExistsWarning=no
AllowRootDirectory=no
AlwaysShowDirOnReadyPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#InstallerBaseName}
SetupIconFile={#LauncherIconFile}
LicenseFile={#PackageRoot}\LICENSE
Compression=lzma2/ultra64
SolidCompression=yes
ExtraDiskSpaceRequired=167772160
WizardStyle=modern
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
UsePreviousAppDir=no
UsePreviousPrivileges=no
Uninstallable=no
UninstallDisplayIcon={app}\MuffModeUpdater.exe
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
AllowCancelDuringInstall=no
RestartIfNeededByRun=no
VersionInfoCompany=DarkMatter Productions
VersionInfoDescription=Muff Mode installer for Quake II Remastered
VersionInfoCopyright=Copyright (C) 2026 DarkMatter Productions
VersionInfoOriginalFileName={#InstallerBaseName}.exe
VersionInfoProductName=Muff Mode
VersionInfoProductVersion={#AppVersion}
VersionInfoVersion={#AppVersion}

[Files]
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Excludes: "rerelease\baseq2\MuffModeBackups\*,rerelease\baseq2\muffmode-installer-write-test-*.tmp,rerelease\baseq2\muffmode-install-receipt.txt"; Flags: ignoreversion recursesubdirs createallsubdirs; BeforeInstall: ValidateCurrentPayloadDestination

[Tasks]
Name: "desktopshortcut"; Description: "Create a desktop shortcut for Muff Mode Updater & Launcher"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "startmenushortcut"; Description: "Create Start menu shortcuts for Muff Mode"

[Icons]
Name: "{autodesktop}\Muff Mode Updater & Launcher"; Filename: "{app}\MuffModeUpdater.exe"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: desktopshortcut; Check: UpdaterExists
Name: "{autoprograms}\Muff Mode\Muff Mode Updater & Launcher"; Filename: "{app}\MuffModeUpdater.exe"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: startmenushortcut; Check: UpdaterExists
Name: "{autoprograms}\Muff Mode\Muff Mode Install & Usage Guide"; Filename: "{app}\README.html"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: startmenushortcut; Check: ReadmeExists
Name: "{autoprograms}\Muff Mode\Muff Mode Release Changelog"; Filename: "{app}\CHANGELOG.md"; WorkingDir: "{app}"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: startmenushortcut; Check: ChangelogExists
Name: "{autoprograms}\Muff Mode\Muff Mode Server Config Guide"; Filename: "{app}\rerelease\baseq2\CONFIGS_README.md"; WorkingDir: "{app}\rerelease\baseq2"; IconFilename: "{app}\MuffModeUpdater.exe"; Tasks: startmenushortcut; Check: ServerConfigGuideExists

[Run]
Filename: "{app}\README.html"; Description: "Open the Muff Mode install and usage guide"; Flags: shellexec postinstall skipifsilent unchecked; Check: ReadmeExists
Filename: "{app}\CHANGELOG.md"; Description: "Open the Muff Mode release changelog"; Flags: shellexec postinstall skipifsilent unchecked; Check: ChangelogExists
Filename: "{app}\rerelease\baseq2\CONFIGS_README.md"; Description: "Open the Muff Mode server config guide"; Flags: shellexec postinstall skipifsilent unchecked; Check: ServerConfigGuideExists
Filename: "{app}\MuffModeUpdater.exe"; Description: "Launch Muff Mode Updater & Launcher"; Flags: nowait postinstall skipifsilent unchecked; Check: UpdaterExists

[InstallDelete]
Type: files; Name: "{app}\rerelease\baseq2\muffmode-installer-write-test-*.tmp"

[Messages]
WelcomeLabel2=This will install {#ReleaseLabel} for Quake II Remastered. Choose a detected store install or another location, confirm the outer Quake II folder, and the installer will place Muff Mode files under rerelease\baseq2.
SelectDirDesc=Select the outer Quake II installation folder. It must contain rerelease\baseq2 and a Quake II launcher executable; do not select the rerelease, baseq2, or extracted Muff Mode package folder.

[Code]
const
  { Keep explicit checks for Inno/Windows versions without RedirectionGuard. }
  MAX_INSTALL_TREE_SCAN_DEPTH = 64;
  MAX_INSTALL_TREE_SCAN_ENTRIES = 100000;
  MAX_SERVER_CONFIG_BACKUP_FILES = 128;
  MAX_SERVER_CONFIG_BACKUP_BYTES = 16777216;
  MAX_GAME_DLL_BACKUP_BYTES = 134217728;
  MAX_VERSION_MARKER_BACKUP_BYTES = 1048576;
  MAX_BACKUP_NAME_ATTEMPTS = 1000;
  OBSOLETE_AEROWALK_MAP_BYTES = 761416;
  OBSOLETE_AEROWALK_MAP_SHA256 = '8fd4ab55fe63e3ac4f0fa0f117c64a5d4610a386979750a6cb09e361b1d37904';

var
  StorePage: TInputOptionWizardPage;
  SteamInstallDirResolved: Boolean;
  EpicInstallDirResolved: Boolean;
  GogInstallDirResolved: Boolean;
  XboxInstallDirResolved: Boolean;
  CachedSteamInstallDir: String;
  CachedEpicInstallDir: String;
  CachedGogInstallDir: String;
  CachedXboxInstallDir: String;
  LastBackupFile: String;
  LastConfigBackupDir: String;

function GetFileAttributesW(FileName: String): DWORD;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function InitializeSetup(): Boolean;
begin
  Result := True;
  Log('Preparing Muff Mode installer {#AppVersion} ({#Channel}).');
  Log('Packaged game_x64.dll SHA256: {#PackageGameDllHash}');
  Log('Packaged MuffModeUpdater.exe SHA256: {#PackageUpdaterHash}');
end;

function NormalizeDetectedPath(Path: String): String;
var
  IsUncPath: Boolean;
begin
  Result := Trim(Path);
  if (Length(Result) >= 2) and (Copy(Result, 1, 1) = '"') and (Copy(Result, Length(Result), 1) = '"') then
    Result := Copy(Result, 2, Length(Result) - 2);
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
  Path := NormalizeDetectedPath(Path);
  if Path = '' then
  begin
    Result := False;
    Exit;
  end;

  Result := DirExists(AddBackslash(Path) + '{#Baseq2RelativePath}');
end;

function HasKnownQuake2Executable(Path: String): Boolean;
var
  RootDir: String;
begin
  RootDir := AddBackslash(NormalizeDetectedPath(Path));
  Result :=
    FileExists(RootDir + 'quake2.exe') or
    FileExists(RootDir + 'q2.exe') or
    FileExists(RootDir + 'rerelease\quake2.exe') or
    FileExists(RootDir + 'rerelease\Quake2.exe') or
    FileExists(RootDir + 'rerelease\quake2rerelease.exe') or
    FileExists(RootDir + 'rerelease\Quake2Rerelease.exe') or
    FileExists(RootDir + 'rerelease\quake2ex.exe') or
    FileExists(RootDir + 'rerelease\quake2ex_steam.exe') or
    FileExists(RootDir + 'rerelease\Quake II.exe') or
    FileExists(RootDir + 'rerelease\Quake II Rerelease.exe');
end;

function LooksLikeMuffModePackageRoot(Path: String): Boolean;
var
  RootDir: String;
begin
  RootDir := AddBackslash(NormalizeDetectedPath(Path));
  Result :=
    FileExists(RootDir + 'MuffModeUpdater.exe') and
    (FileExists(RootDir + 'README.md') or FileExists(RootDir + 'README.html')) and
    FileExists(RootDir + '{#GameDllRelativePath}') and
    FileExists(RootDir + '{#Baseq2RelativePath}\muffmode-version.json');
end;

function GetBaseq2Dir(Quake2Root: String): String;
begin
  Result := AddBackslash(NormalizeDetectedPath(Quake2Root)) + '{#Baseq2RelativePath}';
end;

function GetGameDllPath(Quake2Root: String): String;
begin
  Result := AddBackslash(NormalizeDetectedPath(Quake2Root)) + '{#GameDllRelativePath}';
end;

function SameNormalizedPath(FirstPath: String; SecondPath: String): Boolean;
begin
  Result :=
    Lowercase(RemoveBackslash(NormalizeDetectedPath(FirstPath))) =
    Lowercase(RemoveBackslash(NormalizeDetectedPath(SecondPath)));
end;

function IsExistingReparsePoint(Path: String): Boolean;
var
  Attributes: DWORD;
begin
  Attributes := GetFileAttributesW(Path);
  Result :=
    (Attributes <> $FFFFFFFF) and
    ((Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0);
end;

function FindReparsePointOnExistingPath(Path: String): String;
var
  CurrentPath: String;
  ParentPath: String;
begin
  Result := '';
  CurrentPath := NormalizeDetectedPath(Path);

  while CurrentPath <> '' do
  begin
    if IsExistingReparsePoint(CurrentPath) then
    begin
      Result := CurrentPath;
      Exit;
    end;

    ParentPath := ExtractFileDir(CurrentPath);
    if (ParentPath = '') or SameNormalizedPath(ParentPath, CurrentPath) then
      Exit;

    CurrentPath := ParentPath;
  end;
end;

function ValidateExistingPayloadTree(
  RootDir: String;
  Depth: Integer;
  var EntryCount: Integer): String;
var
  FindRec: TFindRec;
  EntryPath: String;
  UnsafePath: String;
begin
  Result := '';
  if Depth = 0 then
  begin
    UnsafePath := FindReparsePointOnExistingPath(RootDir);
    if UnsafePath <> '' then
    begin
      Result :=
        'The selected Quake II installation path traverses a reparse point and cannot be modified safely:' +
        #13#10#13#10 + UnsafePath;
      Exit;
    end;
  end;

  if not DirExists(RootDir) then
    Exit;

  if Depth > MAX_INSTALL_TREE_SCAN_DEPTH then
  begin
    Result :=
      'The selected Quake II installation contains a payload directory tree deeper than the installer can validate safely:' +
      #13#10#13#10 + RootDir;
    Exit;
  end;

  if IsExistingReparsePoint(RootDir) then
  begin
    Result :=
      'The selected Quake II installation contains a reparse point in a Muff Mode destination path:' +
      #13#10#13#10 + RootDir;
    Exit;
  end;

  if not FindFirst(AddBackslash(RootDir) + '*', FindRec) then
    Exit;

  try
    repeat
      if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
      begin
        EntryCount := EntryCount + 1;
        if EntryCount > MAX_INSTALL_TREE_SCAN_ENTRIES then
        begin
          Result :=
            'The selected Quake II installation contains too many existing payload entries to validate safely.' +
            #13#10#13#10 +
            'Close the installer and inspect the rerelease maps, bots, and Muff Mode documentation folders before trying again.';
          Exit;
        end;

        EntryPath := AddBackslash(RootDir) + FindRec.Name;
        if (FindRec.Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0 then
        begin
          Result :=
            'The selected Quake II installation contains a reparse point in a Muff Mode destination path:' +
            #13#10#13#10 + EntryPath;
          Exit;
        end;

        if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
        begin
          Result := ValidateExistingPayloadTree(EntryPath, Depth + 1, EntryCount);
          if Result <> '' then
            Exit;
        end;
      end;
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

function ValidatePayloadFileIsNotReparsePoint(Path: String): String;
begin
  Result := '';
  if IsExistingReparsePoint(Path) then
    Result :=
      'The selected Quake II installation contains a reparse-point file that Muff Mode would replace:' +
      #13#10#13#10 + Path;
end;

function AddServerConfigToBackupBudget(
  ConfigPath: String;
  var ConfigCount: Integer;
  var TotalBytes: Int64): String;
var
  ConfigBytes: Int64;
begin
  Result := '';
  if not FileSize64(ConfigPath, ConfigBytes) then
  begin
    Result :=
      'An existing server config selected for backup could not be measured safely:' +
      #13#10#13#10 + ConfigPath;
    Exit;
  end;

  if ConfigCount >= MAX_SERVER_CONFIG_BACKUP_FILES then
  begin
    Result :=
      'The selected Quake II installation contains more than 128 server configs selected for automatic backup.' +
      #13#10#13#10 +
      'Archive old server config files manually, then run this installer again.';
    Exit;
  end;

  if (ConfigBytes < 0) or
    (ConfigBytes > MAX_SERVER_CONFIG_BACKUP_BYTES - TotalBytes) then
  begin
    Result :=
      'The existing server configs selected for automatic backup exceed the safe 16 MiB limit.' +
      #13#10#13#10 +
      'Archive large or obsolete server config files manually, then run this installer again.';
    Exit;
  end;

  ConfigCount := ConfigCount + 1;
  TotalBytes := TotalBytes + ConfigBytes;
end;

function ValidateServerConfigBackupSources(Baseq2Dir: String): String;
var
  ConfigPath: String;
  ConfigCount: Integer;
  TotalBytes: Int64;
begin
  Result := '';
  ConfigCount := 0;
  TotalBytes := 0;

  ConfigPath := AddBackslash(Baseq2Dir) + 'server-base.cfg';
  if FileExists(ConfigPath) then
  begin
    Result := AddServerConfigToBackupBudget(ConfigPath, ConfigCount, TotalBytes);
    if Result <> '' then
      Exit;
  end;

  ConfigPath := AddBackslash(Baseq2Dir) + 'factories.cfg';
  if FileExists(ConfigPath) then
  begin
    if IsExistingReparsePoint(ConfigPath) then
    begin
      Result :=
        'The existing factories.cfg is a reparse point and cannot be read safely:' +
        #13#10#13#10 + ConfigPath;
      Exit;
    end;

    Result := AddServerConfigToBackupBudget(ConfigPath, ConfigCount, TotalBytes);
    if Result <> '' then
      Exit;
  end;
end;

function ValidateBackupFileSize(
  Path: String;
  Description: String;
  MaximumBytes: Int64): String;
var
  Size: Int64;
begin
  Result := '';
  if not FileExists(Path) then
    Exit;

  if (not FileSize64(Path, Size)) or (Size < 0) or (Size > MaximumBytes) then
    Result :=
      'The existing ' + Description + ' is too large or could not be measured safely for automatic backup:' +
      #13#10#13#10 + Path;
end;

function ValidateInstallTreeForReparsePoints(): String;
var
  InstallRoot: String;
  RereleaseDir: String;
  Baseq2Dir: String;
  EntryCount: Integer;
begin
  Result := '';
  InstallRoot := NormalizeDetectedPath(WizardDirValue);
  RereleaseDir := AddBackslash(InstallRoot) + 'rerelease';
  Baseq2Dir := AddBackslash(RereleaseDir) + 'baseq2';

  Result := FindReparsePointOnExistingPath(InstallRoot);
  if Result <> '' then
  begin
    Result :=
      'The selected Quake II installation path traverses a reparse point and cannot be modified safely:' +
      #13#10#13#10 + Result;
    Exit;
  end;

  Result := FindReparsePointOnExistingPath(Baseq2Dir);
  if Result <> '' then
  begin
    Result :=
      'The selected Quake II installation path traverses a reparse point and cannot be modified safely:' +
      #13#10#13#10 + Result;
    Exit;
  end;

  Result := FindReparsePointOnExistingPath(AddBackslash(Baseq2Dir) + 'MuffModeBackups');
  if Result <> '' then
  begin
    Result :=
      'The Muff Mode backup destination traverses a reparse point and cannot be modified safely:' +
      #13#10#13#10 + Result;
    Exit;
  end;

  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.html');
  if Result <> '' then Exit;
#ifdef IncludeLocalizedReadmes
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.de.html');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.pl.html');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.fr.html');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.hu.html');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.bg.html');
  if Result <> '' then Exit;
#endif
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'README.md');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'CHANGELOG.md');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'LICENSE');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'THIRD_PARTY_NOTICES.md');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'MuffModeUpdater.exe');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'MuffMode.version');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(InstallRoot) + 'VERSION');
  if Result <> '' then Exit;

  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'game_x64.dll');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'muffmode-version.json');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'muffmode.version');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'muffmode-install-receipt.txt');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'CONFIGS_README.md');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'muffmode-map-cycle.example.txt');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'muffmode-map-pool.example.json');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'server-base.cfg');
  if Result <> '' then Exit;
  Result := ValidatePayloadFileIsNotReparsePoint(AddBackslash(Baseq2Dir) + 'factories.cfg');
  if Result <> '' then Exit;

  Result := ValidateServerConfigBackupSources(Baseq2Dir);
  if Result <> '' then Exit;

  Result := ValidateBackupFileSize(
    AddBackslash(Baseq2Dir) + 'game_x64.dll',
    'game_x64.dll',
    MAX_GAME_DLL_BACKUP_BYTES);
  if Result <> '' then Exit;
  Result := ValidateBackupFileSize(
    AddBackslash(Baseq2Dir) + 'muffmode.version',
    'Muff Mode text version marker',
    MAX_VERSION_MARKER_BACKUP_BYTES);
  if Result <> '' then Exit;
  Result := ValidateBackupFileSize(
    AddBackslash(Baseq2Dir) + 'muffmode-version.json',
    'Muff Mode JSON version marker',
    MAX_VERSION_MARKER_BACKUP_BYTES);
  if Result <> '' then Exit;

  EntryCount := 0;
  Result := ValidateExistingPayloadTree(AddBackslash(RereleaseDir) + 'maps', 0, EntryCount);
  if Result <> '' then Exit;
  Result := ValidateExistingPayloadTree(AddBackslash(RereleaseDir) + 'bots', 0, EntryCount);
  if Result <> '' then Exit;
  Result := ValidateExistingPayloadTree(
    AddBackslash(Baseq2Dir) + 'docs\muffmode\maps\original-readmes',
    0,
    EntryCount);
end;

procedure ValidateCurrentPayloadDestination;
var
  DestinationPath: String;
  UnsafePath: String;
begin
  DestinationPath := CurrentFileName;
  UnsafePath := FindReparsePointOnExistingPath(DestinationPath);
  if UnsafePath <> '' then
    RaiseException(
      'A Muff Mode payload destination became a reparse point before it could be installed safely:' +
      #13#10#13#10 + UnsafePath);
end;

function PathIsAtOrUnder(ChildPath: String; ParentPath: String): Boolean;
var
  Child: String;
  Parent: String;
begin
  Child := Lowercase(RemoveBackslash(NormalizeDetectedPath(ChildPath)));
  Parent := Lowercase(RemoveBackslash(NormalizeDetectedPath(ParentPath)));

  if (Child = '') or (Parent = '') then
  begin
    Result := False;
    Exit;
  end;

  Result := (Child = Parent) or (Pos(AddBackslash(Parent), AddBackslash(Child)) = 1);
end;

function IsDriveRootPath(Path: String): Boolean;
var
  Normalized: String;
begin
  Normalized := RemoveBackslash(NormalizeDetectedPath(Path));
  Result := (Length(Normalized) = 2) and (Copy(Normalized, 2, 1) = ':');
end;

function RejectExactInstallDir(SelectedDir: String; ForbiddenDir: String; Description: String): String;
begin
  Result := '';

  if (ForbiddenDir <> '') and SameNormalizedPath(SelectedDir, ForbiddenDir) then
  begin
    Result :=
      'Do not install Muff Mode directly into ' + Description + '. Choose the outer Quake II Remastered installation folder instead.' +
      #13#10#13#10 +
      'Selected folder:' + #13#10 + SelectedDir;
  end;
end;

function TryGetFileSha256(FileName: String; var Hash: String): Boolean;
begin
  Hash := '';
  Result := False;

  if not FileExists(FileName) then
    Exit;

  try
    Hash := Lowercase(GetSHA256OfFile(FileName));
    Result := True;
  except
    Log('Could not calculate SHA256 for: ' + FileName);
  end;
end;

procedure CleanupObsoleteAerowalkMap();
var
  ObsoleteFile: String;
  UnsafePath: String;
  Attributes: DWORD;
  Size: Int64;
  ActualHash: String;
begin
  ObsoleteFile := AddBackslash(WizardDirValue) + '{#MapsRelativePath}\mm-aerowalk.bsp';

  try
    Attributes := GetFileAttributesW(ObsoleteFile);
    if Attributes = $FFFFFFFF then
    begin
      Log('Obsolete Aerowalk map is not present or its attributes are unavailable; no cleanup attempted: ' + ObsoleteFile);
      Exit;
    end;

    if (Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
    begin
      Log('Preserving obsolete Aerowalk path because it is not a regular file: ' + ObsoleteFile);
      Exit;
    end;

    if (Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0 then
    begin
      Log('Preserving obsolete Aerowalk map because it is a reparse point: ' + ObsoleteFile);
      Exit;
    end;

    UnsafePath := FindReparsePointOnExistingPath(ObsoleteFile);
    if UnsafePath <> '' then
    begin
      Log('Preserving obsolete Aerowalk map because its path traverses a reparse point: ' + UnsafePath);
      Exit;
    end;

    if not FileSize64(ObsoleteFile, Size) then
    begin
      Log('Preserving obsolete Aerowalk map because its length could not be measured safely: ' + ObsoleteFile);
      Exit;
    end;

    if Size <> OBSOLETE_AEROWALK_MAP_BYTES then
    begin
      Log('Preserving obsolete Aerowalk map because its length does not match the known invalid payload: ' + ObsoleteFile);
      Exit;
    end;

    if not TryGetFileSha256(ObsoleteFile, ActualHash) then
    begin
      Log('Preserving obsolete Aerowalk map because its SHA256 could not be calculated: ' + ObsoleteFile);
      Exit;
    end;

    if ActualHash <> OBSOLETE_AEROWALK_MAP_SHA256 then
    begin
      Log('Preserving obsolete Aerowalk map because its SHA256 does not match the known invalid payload: ' + ObsoleteFile);
      Exit;
    end;

    { Recheck the path immediately before deletion so a newly introduced reparse point is never followed. }
    UnsafePath := FindReparsePointOnExistingPath(ObsoleteFile);
    Attributes := GetFileAttributesW(ObsoleteFile);
    if (UnsafePath <> '') or (Attributes = $FFFFFFFF) or
      ((Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) or
      ((Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0) then
    begin
      Log('Preserving obsolete Aerowalk map because its path or file type changed during cleanup: ' + ObsoleteFile);
      Exit;
    end;

    if (not FileSize64(ObsoleteFile, Size)) or
      (Size <> OBSOLETE_AEROWALK_MAP_BYTES) or
      (not TryGetFileSha256(ObsoleteFile, ActualHash)) or
      (ActualHash <> OBSOLETE_AEROWALK_MAP_SHA256) then
    begin
      Log('Preserving obsolete Aerowalk map because it changed immediately before deletion: ' + ObsoleteFile);
      Exit;
    end;

    if DeleteFile(ObsoleteFile) then
      Log('Removed the exact known invalid obsolete Aerowalk map after validating its replacement: ' + ObsoleteFile)
    else
      Log('Could not remove the exact known invalid obsolete Aerowalk map; installation will continue: ' + ObsoleteFile);
  except
    Log(
      'Unexpected error while checking the obsolete Aerowalk map; preserving it and continuing installation (' +
      GetExceptionMessage + '): ' + ObsoleteFile);
  end;
end;

function VerifyInstalledFileHash(FileName: String; ExpectedHash: String; FriendlyName: String): String;
var
  ActualHash: String;
begin
  Result := '';

  if not TryGetFileSha256(FileName, ActualHash) then
  begin
    Result := 'The installed ' + FriendlyName + ' could not be found or hashed:' + #13#10#13#10 + FileName;
    Exit;
  end;

  if ActualHash <> Lowercase(ExpectedHash) then
  begin
    Result :=
      'The installed ' + FriendlyName + ' does not match the packaged release file.' + #13#10#13#10 +
      'Destination:' + #13#10 + FileName;
  end;
end;

function RequireInstalledRelativeFile(RelativePath: String; FriendlyName: String): String;
var
  FileName: String;
begin
  Result := '';
  FileName := AddBackslash(WizardDirValue) + RelativePath;

  if not FileExists(FileName) then
  begin
    Result := 'The installed ' + FriendlyName + ' is missing:' + #13#10#13#10 + FileName;
  end;
end;

function ReadTrimmedFile(FileName: String; var Value: String): Boolean;
var
  Bytes: AnsiString;
begin
  Value := '';
  Result := False;

  if not FileExists(FileName) then
    Exit;

  if LoadStringFromFile(FileName, Bytes) then
  begin
    Value := Bytes;
    Value := Trim(Value);
    Result := True;
  end;
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

function ReadJsonRawStringValue(Json: String; PropertyName: String): String;
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
end;

function ReadJsonStringValue(Json: String; PropertyName: String): String;
begin
  Result := NormalizeDetectedPath(ReadJsonRawStringValue(Json, PropertyName));
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

function ReadVdfStringValue(Text: String; PropertyName: String): String;
var
  Key: String;
  KeyPos: Integer;
  Value: String;
begin
  Result := '';
  Key := '"' + PropertyName + '"';
  KeyPos := Pos(Key, Text);
  if KeyPos = 0 then
    Exit;

  Delete(Text, 1, KeyPos + Length(Key) - 1);
  if ReadNextQuotedValue(Text, Value) then
    Result := Value;
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

function GetSteamQuake2InstallDirFromLibrary(LibraryRoot: String): String;
var
  ManifestBytes: AnsiString;
  ManifestText: String;
  InstallDir: String;
  CandidatePath: String;
begin
  Result := '';
  LibraryRoot := NormalizeDetectedPath(LibraryRoot);
  if LibraryRoot = '' then
    Exit;

  if LoadStringFromFile(AddBackslash(LibraryRoot) + 'steamapps\appmanifest_2320.acf', ManifestBytes) then
  begin
    ManifestText := ManifestBytes;
    InstallDir := NormalizeDetectedPath(ReadVdfStringValue(ManifestText, 'installdir'));
    if InstallDir <> '' then
    begin
      CandidatePath := AddBackslash(LibraryRoot) + 'steamapps\common\' + InstallDir;
      if IsQuake2RootPath(CandidatePath) then
      begin
        Result := CandidatePath;
        Exit;
      end;
    end;
  end;

  CandidatePath := AddBackslash(LibraryRoot) + 'steamapps\common\Quake 2';
  if IsQuake2RootPath(CandidatePath) then
    Result := CandidatePath;
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
      if not RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'InstallPath', SteamPath) then
        if not RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', SteamPath) then
          SteamPath := '';

  if SteamPath <> '' then
  begin
    SteamPath := NormalizeDetectedPath(SteamPath);
    CandidatePath := GetSteamQuake2InstallDirFromLibrary(SteamPath);
    if CandidatePath <> '' then
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
        CandidatePath := GetSteamQuake2InstallDirFromLibrary(LibraryPath);
        if CandidatePath <> '' then
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

function GetXboxInstallDir(): String;
begin
  if XboxInstallDirResolved then
  begin
    Result := CachedXboxInstallDir;
    Exit;
  end;
  XboxInstallDirResolved := True;
  Result := FirstValidOrFallback(
    ExpandConstant('{sd}\XboxGames\Quake II\Content'),
    ExpandConstant('{sd}\XboxGames\Quake 2\Content'),
    ExpandConstant('{pf}\ModifiableWindowsApps\Quake II'),
    ExpandConstant('{sd}\XboxGames\Quake II\Content'));

  if not IsQuake2RootPath(Result) then
    if IsQuake2RootPath(ExpandConstant('{pf}\ModifiableWindowsApps\Quake 2')) then
      Result := ExpandConstant('{pf}\ModifiableWindowsApps\Quake 2');

  CachedXboxInstallDir := Result;
end;

function GetDefaultInstallDir(Param: String): String;
begin
  if IsQuake2RootPath(GetSteamInstallDir()) then
    Result := GetSteamInstallDir()
  else if IsQuake2RootPath(GetEpicInstallDir()) then
    Result := GetEpicInstallDir()
  else if IsQuake2RootPath(GetGogInstallDir()) then
    Result := GetGogInstallDir()
  else if IsQuake2RootPath(GetXboxInstallDir()) then
    Result := GetXboxInstallDir()
  else
    Result := ExpandConstant('{sd}\Games\Quake 2');
end;

function LooksLikeRereleaseDir(Path: String): Boolean;
begin
  Path := NormalizeDetectedPath(Path);
  Result := (Lowercase(ExtractFileName(RemoveBackslash(Path))) = 'rerelease') and
    DirExists(AddBackslash(Path) + 'baseq2');
end;

function LooksLikeBaseq2Dir(Path: String): Boolean;
var
  ParentDir: String;
begin
  Path := NormalizeDetectedPath(Path);
  Result := False;

  if Lowercase(ExtractFileName(RemoveBackslash(Path))) <> 'baseq2' then
    Exit;

  ParentDir := ExtractFileDir(RemoveBackslash(Path));
  Result := Lowercase(ExtractFileName(RemoveBackslash(ParentDir))) = 'rerelease';
end;

function GetOuterQuake2RootFromSelection(Path: String): String;
begin
  Result := NormalizeDetectedPath(Path);

  if LooksLikeRereleaseDir(Result) then
    Result := ExtractFileDir(RemoveBackslash(Result))
  else if LooksLikeBaseq2Dir(Result) then
    Result := ExtractFileDir(ExtractFileDir(RemoveBackslash(Result)));
end;

function LooksLikeQuake2Root(Path: String): Boolean;
begin
  Result := IsQuake2RootPath(Path) and HasKnownQuake2Executable(Path);
end;

function UpdaterExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\MuffModeUpdater.exe'));
end;

function ReadmeExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\README.html'));
end;

function ChangelogExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\CHANGELOG.md'));
end;

function ServerConfigGuideExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\rerelease\baseq2\CONFIGS_README.md'));
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
  else if LooksLikeQuake2Root(GetXboxInstallDir()) then
    Result := 3
  else
    Result := 4;
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
  StorePage.Add(DescribeInstallChoice('Xbox app / Microsoft Store', GetXboxInstallDir()));
  StorePage.Add('Other location - browse for your Quake II folder');
  StorePage.SelectedValueIndex := GetInitialStoreIndex();
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  SelectedDir: String;
  AdjustedDir: String;
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
    else if StorePage.Values[3] then
      WizardForm.DirEdit.Text := GetXboxInstallDir()
    else
      WizardForm.DirEdit.Text := ExpandConstant('{sd}\Games\Quake 2');
  end
  else if CurPageID = wpSelectDir then
  begin
    SelectedDir := NormalizeDetectedPath(WizardDirValue);
    AdjustedDir := GetOuterQuake2RootFromSelection(SelectedDir);

    if not SameNormalizedPath(SelectedDir, AdjustedDir) then
    begin
      WizardForm.DirEdit.Text := AdjustedDir;
      if WizardSilent() then
        Log('Adjusted silent install destination to outer Quake II folder: ' + AdjustedDir)
      else
        MsgBox(
          'Muff Mode installs into the outer Quake II folder, not the rerelease or baseq2 subfolder. The destination has been adjusted to:' + #13#10#13#10 + AdjustedDir,
          mbInformation,
          MB_OK);
      SelectedDir := AdjustedDir;
    end;

    if not IsQuake2RootPath(SelectedDir) then
    begin
      if WizardSilent() then
      begin
        Log('Silent install destination does not contain rerelease\baseq2: ' + SelectedDir);
      end
      else
        MsgBox(
          'This folder does not look like the outer Quake II Remastered folder because rerelease\baseq2 was not found. Choose the folder that contains the game''s rerelease folder.',
          mbError,
          MB_OK);
      Result := False;
    end
    else if not HasKnownQuake2Executable(SelectedDir) then
    begin
      if LooksLikeMuffModePackageRoot(SelectedDir) then
      begin
        if WizardSilent() then
          Log('Silent install destination looks like an extracted Muff Mode package, not a Quake II install: ' + SelectedDir)
        else
          MsgBox(
            'This folder looks like an extracted Muff Mode package, not the outer Quake II Remastered installation folder. Choose the folder that contains the Quake II launcher executable.',
            mbError,
            MB_OK);
      end
      else
      begin
        if WizardSilent() then
          Log('Silent install destination does not contain a known Quake II launcher executable: ' + SelectedDir)
        else
          MsgBox(
            'This folder has rerelease\baseq2, but it does not contain a known Quake II launcher executable. Choose the outer Quake II Remastered installation folder.',
            mbError,
            MB_OK);
      end;
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
    if (Counter >= MAX_BACKUP_NAME_ATTEMPTS) and FileExists(Candidate) then
    begin
      Result := '';
      Exit;
    end;
  until not FileExists(Candidate);

  Result := Candidate;
end;

function GetUniqueConfigBackupDir(BackupRoot: String): String;
var
  Counter: Integer;
  Timestamp: String;
  Candidate: String;
begin
  Counter := 0;
  Timestamp := GetDateTimeString('yyyy-mm-dd-hhnnss', '-', '-');

  repeat
    if Counter = 0 then
      Candidate := AddBackslash(BackupRoot) + 'server-configs-before-muffmode-' + Timestamp
    else
      Candidate :=
        AddBackslash(BackupRoot) + 'server-configs-before-muffmode-' + Timestamp + '-' + IntToStr(Counter);

    Counter := Counter + 1;
    if (Counter >= MAX_BACKUP_NAME_ATTEMPTS) and
      (DirExists(Candidate) or FileExists(Candidate)) then
    begin
      Result := '';
      Exit;
    end;
  until (not DirExists(Candidate)) and (not FileExists(Candidate));

  Result := Candidate;
end;

function CopyServerConfigBackup(SourceFile: String; BackupFile: String): Boolean;
var
  SourceHash: String;
  BackupHash: String;
begin
  Result :=
    (not IsExistingReparsePoint(SourceFile)) and
    (FindReparsePointOnExistingPath(BackupFile) = '') and
    CopyFile(SourceFile, BackupFile, False) and
    (not IsExistingReparsePoint(BackupFile)) and
    TryGetFileSha256(SourceFile, SourceHash) and
    TryGetFileSha256(BackupFile, BackupHash) and
    (SourceHash = BackupHash);

  if not Result and FileExists(BackupFile) then
    DeleteFile(BackupFile);
end;

function BackupExistingServerConfigs(): String;
var
  Baseq2Dir: String;
  BackupRoot: String;
  BackupDir: String;
  SourceFile: String;
  BackupFile: String;
  HasConfigFiles: Boolean;
  BackupCount: Integer;
  BackupBytes: Int64;
  BudgetError: String;
begin
  Result := '';
  Baseq2Dir := AddBackslash(GetBaseq2Dir(WizardDirValue));
  HasConfigFiles :=
    FileExists(Baseq2Dir + 'server-base.cfg') or
    FileExists(Baseq2Dir + 'factories.cfg');

  if not HasConfigFiles then
    Exit;

  BackupRoot := Baseq2Dir + 'MuffModeBackups';
  if not ForceDirectories(BackupRoot) then
  begin
    Result := 'Could not create the Muff Mode backup folder:' + #13#10#13#10 + BackupRoot;
    Exit;
  end;

  BackupDir := GetUniqueConfigBackupDir(BackupRoot);
  if BackupDir = '' then
  begin
    Result :=
      'Could not select a unique server-config backup folder after 1000 attempts:' +
      #13#10#13#10 + BackupRoot;
    Exit;
  end;
  if not ForceDirectories(BackupDir) then
  begin
    Result := 'Could not create the server-config backup folder:' + #13#10#13#10 + BackupDir;
    Exit;
  end;
  if IsExistingReparsePoint(BackupDir) then
  begin
    Result := 'The server-config backup folder became a reparse point:' + #13#10#13#10 + BackupDir;
    Exit;
  end;

  BackupCount := 0;
  BackupBytes := 0;
  SourceFile := Baseq2Dir + 'server-base.cfg';
  if FileExists(SourceFile) then
  begin
    if IsExistingReparsePoint(SourceFile) then
    begin
      Result := 'The existing server-base.cfg became a reparse point before backup:' + #13#10#13#10 + SourceFile;
      Exit;
    end;
    BudgetError := AddServerConfigToBackupBudget(SourceFile, BackupCount, BackupBytes);
    if BudgetError <> '' then
    begin
      Result := BudgetError;
      Exit;
    end;

    BackupFile := AddBackslash(BackupDir) + 'server-base.cfg';
    if not CopyServerConfigBackup(SourceFile, BackupFile) then
    begin
      Result :=
        'Could not back up the existing server-base.cfg before installing Muff Mode.' + #13#10#13#10 +
        'Close any tools using this file, check folder permissions, then run this installer again.' + #13#10#13#10 +
        'Backup target:' + #13#10 + BackupFile;
      Exit;
    end;
  end;

  SourceFile := Baseq2Dir + 'factories.cfg';
  if FileExists(SourceFile) then
  begin
    if IsExistingReparsePoint(SourceFile) then
    begin
      Result := 'The existing factories.cfg became a reparse point before backup:' + #13#10#13#10 + SourceFile;
      Exit;
    end;
    BudgetError := AddServerConfigToBackupBudget(SourceFile, BackupCount, BackupBytes);
    if BudgetError <> '' then
    begin
      Result := BudgetError;
      Exit;
    end;

    BackupFile := AddBackslash(BackupDir) + 'factories.cfg';
    if not CopyServerConfigBackup(SourceFile, BackupFile) then
    begin
      Result :=
        'Could not back up the existing factories.cfg before installing Muff Mode.' + #13#10#13#10 +
        'Close any tools using this file, check folder permissions, then run this installer again.' + #13#10#13#10 +
        'Backup target:' + #13#10 + BackupFile;
      Exit;
    end;
  end;

  LastConfigBackupDir := BackupDir;
  Log('Backed up ' + IntToStr(BackupCount) + ' existing server config(s) to: ' + BackupDir);
end;

function BackupExistingGameDll(): String;
var
  ExistingDll: String;
  BackupDir: String;
  BackupFile: String;
  BackupPrefix: String;
  ExistingHash: String;
  BackupHash: String;
  ExistingVersionFile: String;
  ExistingVersionJson: String;
  ValidationError: String;
begin
  Result := '';
  ExistingDll := GetGameDllPath(WizardDirValue);

  if FileExists(ExistingDll) then
  begin
    if TryGetFileSha256(ExistingDll, ExistingHash) and (ExistingHash = Lowercase('{#PackageGameDllHash}')) then
    begin
      Log('Existing game_x64.dll already matches the packaged Muff Mode DLL; skipping duplicate backup.');
      Exit;
    end;

    ValidationError := ValidateBackupFileSize(
      ExistingDll,
      'game_x64.dll',
      MAX_GAME_DLL_BACKUP_BYTES);
    if ValidationError <> '' then
    begin
      Result := ValidationError;
      Exit;
    end;

    ExistingVersionFile := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode.version';
    ExistingVersionJson := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode-version.json';
    ValidationError := ValidateBackupFileSize(
      ExistingVersionFile,
      'Muff Mode text version marker',
      MAX_VERSION_MARKER_BACKUP_BYTES);
    if ValidationError <> '' then
    begin
      Result := ValidationError;
      Exit;
    end;
    ValidationError := ValidateBackupFileSize(
      ExistingVersionJson,
      'Muff Mode JSON version marker',
      MAX_VERSION_MARKER_BACKUP_BYTES);
    if ValidationError <> '' then
    begin
      Result := ValidationError;
      Exit;
    end;

    BackupDir := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'MuffModeBackups';
    if not ForceDirectories(BackupDir) then
    begin
      Result := 'Could not create the Muff Mode backup folder:' + #13#10#13#10 + BackupDir;
      Exit;
    end;
    if IsExistingReparsePoint(BackupDir) then
    begin
      Result := 'The Muff Mode backup folder became a reparse point:' + #13#10#13#10 + BackupDir;
      Exit;
    end;

    BackupFile := GetUniqueBackupFile(BackupDir);
    if BackupFile = '' then
    begin
      Result :=
        'Could not select a unique game_x64.dll backup name after 1000 attempts:' +
        #13#10#13#10 + BackupDir;
      Exit;
    end;
    if (FindReparsePointOnExistingPath(BackupFile) <> '') or
      (not CopyFile(ExistingDll, BackupFile, False)) or
      IsExistingReparsePoint(BackupFile) or
      (not TryGetFileSha256(ExistingDll, ExistingHash)) or
      (not TryGetFileSha256(BackupFile, BackupHash)) or
      (ExistingHash <> BackupHash) then
    begin
      if FileExists(BackupFile) then
        DeleteFile(BackupFile);
      Result :=
        'Could not back up the existing Quake II game_x64.dll before installing Muff Mode.' + #13#10#13#10 +
        'Close Quake II, check folder permissions, then run this installer again.' + #13#10#13#10 +
        'Backup target:' + #13#10 + BackupFile;
      Exit;
    end;

    BackupPrefix := Copy(BackupFile, 1, Length(BackupFile) - 4);
    if FileExists(ExistingVersionFile) then
    begin
      if CopyServerConfigBackup(ExistingVersionFile, BackupPrefix + '.version.txt') then
        Log('Backed up existing Muff Mode version marker to: ' + BackupPrefix + '.version.txt')
      else
        Log('Could not back up existing Muff Mode version marker: ' + ExistingVersionFile);
    end;
    if FileExists(ExistingVersionJson) then
    begin
      if CopyServerConfigBackup(ExistingVersionJson, BackupPrefix + '.version.json') then
        Log('Backed up existing Muff Mode version manifest to: ' + BackupPrefix + '.version.json')
      else
        Log('Could not back up existing Muff Mode version manifest: ' + ExistingVersionJson);
    end;

    LastBackupFile := BackupFile;
    Log('Backed up existing Quake II game_x64.dll to: ' + BackupFile);
  end;
end;

procedure CanonicalizeSelectedInstallDirForInstall();
var
  SelectedDir: String;
  AdjustedDir: String;
begin
  SelectedDir := NormalizeDetectedPath(WizardDirValue);
  AdjustedDir := GetOuterQuake2RootFromSelection(SelectedDir);

  if not SameNormalizedPath(SelectedDir, AdjustedDir) then
  begin
    WizardForm.DirEdit.Text := AdjustedDir;
    Log('Adjusted install destination to outer Quake II folder before install: ' + AdjustedDir);
  end;
end;

function ValidateSafeInstallDir(): String;
var
  SelectedDir: String;
begin
  Result := '';
  SelectedDir := NormalizeDetectedPath(WizardDirValue);

  if SelectedDir = '' then
  begin
    Result := 'Choose the outer Quake II Remastered installation folder before installing Muff Mode.';
    Exit;
  end;

  if IsDriveRootPath(SelectedDir) then
  begin
    Result :=
      'Do not install Muff Mode directly into a drive root. Choose the outer Quake II Remastered installation folder instead.' +
      #13#10#13#10 +
      'Selected folder:' + #13#10 + SelectedDir;
    Exit;
  end;

  if PathIsAtOrUnder(SelectedDir, ExpandConstant('{win}')) then
  begin
    Result :=
      'The selected folder is inside the Windows system directory. Choose the outer Quake II Remastered installation folder instead.' + #13#10#13#10 +
      'Selected folder:' + #13#10 + SelectedDir;
    Exit;
  end;

  if PathIsAtOrUnder(SelectedDir, ExpandConstant('{sys}')) then
  begin
    Result :=
      'The selected folder is inside the Windows system directory. Choose the outer Quake II Remastered installation folder instead.' + #13#10#13#10 +
      'Selected folder:' + #13#10 + SelectedDir;
    Exit;
  end;

  if PathIsAtOrUnder(SelectedDir, ExpandConstant('{tmp}')) then
  begin
    Result :=
      'The selected folder is inside a temporary installer folder. Choose the outer Quake II Remastered installation folder instead.' + #13#10#13#10 +
      'Selected folder:' + #13#10 + SelectedDir;
    Exit;
  end;

  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{pf}'), 'the Program Files folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{pf32}'), 'the 32-bit Program Files folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{pf64}'), 'the 64-bit Program Files folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{commonappdata}'), 'the shared application data folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{userappdata}'), 'your roaming application data folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{localappdata}'), 'your local application data folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{userdesktop}'), 'the Desktop');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{commondesktop}'), 'the shared Desktop');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{userdocs}'), 'your Documents folder');
  if Result <> '' then Exit;
  Result := RejectExactInstallDir(SelectedDir, ExpandConstant('{commondocs}'), 'the shared Documents folder');
  if Result <> '' then Exit;

  if Lowercase(ExtractFileName(RemoveBackslash(SelectedDir))) = 'muffmodebackups' then
  begin
    Result :=
      'Do not install Muff Mode into the backup folder. Choose the outer Quake II Remastered installation folder instead.' + #13#10#13#10 +
      'Selected folder:' + #13#10 + SelectedDir;
  end;
end;

function ValidateInstallTargetForInstall(): String;
begin
  Result := '';

  if not IsQuake2RootPath(WizardDirValue) then
  begin
    Result :=
      'Installation requires the outer Quake II Remastered folder containing rerelease\baseq2.' + #13#10#13#10 +
      'Destination:' + #13#10 + WizardDirValue;
    Exit;
  end;

  if not HasKnownQuake2Executable(WizardDirValue) then
  begin
    if LooksLikeMuffModePackageRoot(WizardDirValue) then
      Result :=
        'The selected folder looks like an extracted Muff Mode package, not a Quake II Remastered install. Choose the folder that contains the Quake II launcher executable.' + #13#10#13#10 +
        'Destination:' + #13#10 + WizardDirValue
    else
      Result :=
        'The selected folder has rerelease\baseq2, but it does not contain a known Quake II launcher executable. Choose the outer Quake II Remastered installation folder.' + #13#10#13#10 +
        'Destination:' + #13#10 + WizardDirValue;
  end;
end;

procedure RegisterCloseApplicationResources();
var
  DllPath: String;
  UpdaterPath: String;
begin
  DllPath := GetGameDllPath(WizardDirValue);
  if FileExists(DllPath) then
  begin
    if RegisterExtraCloseApplicationsResource(DllPath) then
      Log('Registered existing game DLL for close-applications handling: ' + DllPath)
    else
      Log('Could not register existing game DLL for close-applications handling: ' + DllPath);
  end;

  UpdaterPath := AddBackslash(WizardDirValue) + 'MuffModeUpdater.exe';
  if FileExists(UpdaterPath) then
  begin
    if RegisterExtraCloseApplicationsResource(UpdaterPath) then
      Log('Registered existing updater for close-applications handling: ' + UpdaterPath)
    else
      Log('Could not register existing updater for close-applications handling: ' + UpdaterPath);
  end;
end;

procedure RegisterExtraCloseApplicationsResources();
begin
  RegisterCloseApplicationResources();
end;

function IsKnownQuake2WindowOpen(): Boolean;
begin
  Result :=
    (FindWindowByWindowName('Quake II') <> 0) or
    (FindWindowByWindowName('Quake II Remastered') <> 0) or
    (FindWindowByWindowName('Quake II Enhanced') <> 0);
end;

function ValidateQuake2NotRunning(): String;
begin
  Result := '';

  if IsKnownQuake2WindowOpen() then
  begin
    Result :=
      'Quake II appears to be running. Close the game before installing Muff Mode so game_x64.dll can be backed up and replaced cleanly.';
  end;
end;

function ValidateTargetWritable(): String;
var
  TargetDir: String;
  TestFile: String;
begin
  Result := '';
  TargetDir := GetBaseq2Dir(WizardDirValue);

  if not DirExists(TargetDir) then
    Exit;

  TestFile :=
    AddBackslash(TargetDir) +
    'muffmode-installer-write-test-' +
    GetDateTimeString('yyyymmddhhnnss', '', '') +
    '.tmp';

  if not SaveStringToFile(TestFile, 'Muff Mode installer write test', False) then
  begin
    Result :=
      'The installer cannot write to the target Quake II baseq2 folder.' + #13#10#13#10 +
      'Close Quake II, check folder permissions, or rerun the installer with elevated permissions.' + #13#10#13#10 +
      'Folder:' + #13#10 + TargetDir;
    Exit;
  end;

  if not DeleteFile(TestFile) then
    Log('Could not remove installer write-test file: ' + TestFile);
end;

function ValidateVersionManifestContents(): String;
var
  ManifestFile: String;
  ManifestBytes: AnsiString;
  ManifestText: String;
  ManifestVersion: String;
  ManifestChannel: String;
  ManifestRepository: String;
  ManifestTagName: String;
begin
  Result := '';
  ManifestFile := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode-version.json';

  if not LoadStringFromFile(ManifestFile, ManifestBytes) then
  begin
    Result := 'The installed Muff Mode JSON version manifest could not be read:' + #13#10#13#10 + ManifestFile;
    Exit;
  end;

  ManifestText := ManifestBytes;
  ManifestVersion := ReadJsonRawStringValue(ManifestText, 'Version');
  ManifestChannel := ReadJsonRawStringValue(ManifestText, 'Channel');
  ManifestRepository := ReadJsonRawStringValue(ManifestText, 'Repository');
  ManifestTagName := ReadJsonRawStringValue(ManifestText, 'TagName');

  if ManifestVersion <> '{#AppVersion}' then
  begin
    Result :=
      'The installed JSON version manifest does not match this installer.' + #13#10#13#10 +
      'Expected version: {#AppVersion}' + #13#10 +
      'Found version: ' + ManifestVersion;
    Exit;
  end;

  if Lowercase(ManifestChannel) <> Lowercase('{#Channel}') then
  begin
    Result :=
      'The installed JSON version manifest has the wrong release channel.' + #13#10#13#10 +
      'Expected channel: {#Channel}' + #13#10 +
      'Found channel: ' + ManifestChannel;
    Exit;
  end;

  if ManifestRepository <> 'DarkMatter-Productions/MuffMode' then
  begin
    Result :=
      'The installed JSON version manifest has the wrong source repository.' + #13#10#13#10 +
      'Found repository: ' + ManifestRepository;
    Exit;
  end;

  if ManifestTagName <> 'v{#AppVersion}' then
  begin
    Result :=
      'The installed JSON version manifest has the wrong release tag.' + #13#10#13#10 +
      'Expected tag: v{#AppVersion}' + #13#10 +
      'Found tag: ' + ManifestTagName;
  end;
end;

function ValidateServerConfigBundle(): String;
var
  Baseq2Dir: String;
begin
  Baseq2Dir := AddBackslash(GetBaseq2Dir(WizardDirValue));

  Result := VerifyInstalledFileHash(
    Baseq2Dir + 'CONFIGS_README.md',
    '{#PackageConfigReadmeHash}',
    'server config guide');
  if Result <> '' then
    Exit;

  Result := VerifyInstalledFileHash(
    Baseq2Dir + 'muffmode-map-cycle.example.txt',
    '{#PackageMapCycleExampleHash}',
    'structured map-cycle example');
  if Result <> '' then
    Exit;

  Result := VerifyInstalledFileHash(
    Baseq2Dir + 'muffmode-map-pool.example.json',
    '{#PackageMapPoolExampleHash}',
    'structured map-pool example');
  if Result <> '' then
    Exit;

  Result := VerifyInstalledFileHash(
    Baseq2Dir + 'server-base.cfg',
    '{#PackageServerBaseHash}',
    'base server config');
  if Result <> '' then
    Exit;

  Result := VerifyInstalledFileHash(
    Baseq2Dir + 'factories.cfg',
    '{#PackageFactoriesHash}',
    'factory registry');

end;

function ValidateOriginalMapReadmeBundle(): String;
var
  ReadmesDir: String;
begin
  ReadmesDir := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'docs\muffmode\maps\original-readmes\';

  Result := VerifyInstalledFileHash(
    ReadmesDir + 'README.md',
    '{#PackageOriginalReadmesIndexHash}',
    'original map readme index');
  if Result <> '' then
    Exit;

  Result := VerifyInstalledFileHash(ReadmesDir + '2box4-readme.txt', '{#PackageOriginalReadme2Box4Hash}', '2box4 original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'aerowalk-readme.txt', '{#PackageOriginalReadmeAerowalkHash}', 'Aerowalk original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'broken2-readme.txt', '{#PackageOriginalReadmeBroken2Hash}', 'Broken2 original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'fleshref-readme.txt', '{#PackageOriginalReadmeFleshrefHash}', 'Flesh Refinery original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'grind-readme.txt', '{#PackageOriginalReadmeGrindHash}', 'Grind original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'ztn2dm1-readme.txt', '{#PackageOriginalReadmeZtn2dm1Hash}', 'Painkiller original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'ztn2dm2-readme.txt', '{#PackageOriginalReadmeZtn2dm2Hash}', 'Kmachine original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'ztn2dm3-readme.txt', '{#PackageOriginalReadmeZtn2dm3Hash}', 'The Rage original map readme');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(ReadmesDir + 'ztn2dm5-readme.txt', '{#PackageOriginalReadmeZtn2dm5Hash}', 'The Crucible original map readme');
end;

function ValidateCustomMapBundle(): String;
var
  MapsDir: String;
begin
  MapsDir := AddBackslash(WizardDirValue) + '{#MapsRelativePath}\';

  Result := VerifyInstalledFileHash(MapsDir + 'mm-aerow.bsp', '{#PackageMapAerowalkHash}', 'Aerowalk map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-coldzero.bsp', '{#PackageMapColdzeroHash}', 'Cold Zero map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-crucible.bsp', '{#PackageMapCrucibleHash}', 'Crucible map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-kmachine.bsp', '{#PackageMapKmachineHash}', 'Kmachine map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-powertrip.bsp', '{#PackageMapPowertripHash}', 'Powertrip map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-rage.bsp', '{#PackageMapRageHash}', 'Rage map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-railgun101.bsp', '{#PackageMapRailgun101Hash}', 'Railgun 101 map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-reclamation.bsp', '{#PackageMapReclamationHash}', 'Reclamation map BSP');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'mm-recycler.bsp', '{#PackageMapRecyclerHash}', 'Recycler map BSP');
  if Result <> '' then Exit;

  Result := VerifyInstalledFileHash(MapsDir + '2box4.ent', '{#PackageEntity2Box4Hash}', '2box4 entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'aerowalk.ent', '{#PackageEntityAerowalkHash}', 'Aerowalk entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'grom_dm3.ent', '{#PackageEntityGromDm3Hash}', 'Grom DM3 entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'kmachine.ent', '{#PackageEntityKmachineHash}', 'Kmachine entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'koldduel1.ent', '{#PackageEntityKoldduel1Hash}', 'Koldduel1 entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'paradm4.ent', '{#PackageEntityParadm4Hash}', 'Paradm4 entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'trdm04a.ent', '{#PackageEntityTrdm04aHash}', 'TRDM04A entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'vd6dm2.ent', '{#PackageEntityVd6dm2Hash}', 'VD6DM2 entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'ven_dm2.ent', '{#PackageEntityVenDm2Hash}', 'Ven DM2 entity override');
  if Result <> '' then Exit;
  Result := VerifyInstalledFileHash(MapsDir + 'ztn2dm5.ent', '{#PackageEntityZtn2dm5Hash}', 'ZTN2DM5 entity override');
end;

function ValidateInstalledPayload(): String;
var
  InstalledGameDll: String;
  InstalledUpdater: String;
  VersionFile: String;
  VersionValue: String;
  InstalledHash: String;
  ManifestError: String;
  ValidationError: String;
begin
  Result := '';

  InstalledGameDll := GetGameDllPath(WizardDirValue);
  if not TryGetFileSha256(InstalledGameDll, InstalledHash) then
  begin
    Result := 'The installed game_x64.dll could not be found or hashed:' + #13#10#13#10 + InstalledGameDll;
    Exit;
  end;
  if InstalledHash <> Lowercase('{#PackageGameDllHash}') then
  begin
    Result :=
      'The installed game_x64.dll does not match the packaged Muff Mode DLL.' + #13#10#13#10 +
      'Destination:' + #13#10 + InstalledGameDll;
    Exit;
  end;

  InstalledUpdater := AddBackslash(WizardDirValue) + 'MuffModeUpdater.exe';
  if not TryGetFileSha256(InstalledUpdater, InstalledHash) then
  begin
    Result := 'The installed MuffModeUpdater.exe could not be found or hashed:' + #13#10#13#10 + InstalledUpdater;
    Exit;
  end;
  if InstalledHash <> Lowercase('{#PackageUpdaterHash}') then
  begin
    Result :=
      'The installed MuffModeUpdater.exe does not match the packaged updater.' + #13#10#13#10 +
      'Destination:' + #13#10 + InstalledUpdater;
    Exit;
  end;

  VersionFile := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode.version';
  if not ReadTrimmedFile(VersionFile, VersionValue) then
  begin
    Result := 'The installed Muff Mode version marker could not be read:' + #13#10#13#10 + VersionFile;
    Exit;
  end;
  if VersionValue <> '{#AppVersion}' then
  begin
    Result :=
      'The installed Muff Mode version marker does not match this installer.' + #13#10#13#10 +
      'Expected: {#AppVersion}' + #13#10 +
      'Found: ' + VersionValue;
    Exit;
  end;

  if not FileExists(AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode-version.json') then
  begin
    Result := 'The installed Muff Mode JSON version manifest is missing.';
    Exit;
  end;
  ValidationError := VerifyInstalledFileHash(
    AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode-version.json',
    '{#PackageVersionManifestHash}',
    'JSON version manifest');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ManifestError := ValidateVersionManifestContents();
  if ManifestError <> '' then
  begin
    Result := ManifestError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.html',
    '{#PackageReadmeHash}',
    'README.html');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

#ifdef IncludeLocalizedReadmes
  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.de.html',
    '{#PackageReadmeDeHash}',
    'README.de.html');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.pl.html',
    '{#PackageReadmePlHash}',
    'README.pl.html');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.fr.html',
    '{#PackageReadmeFrHash}',
    'README.fr.html');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.hu.html',
    '{#PackageReadmeHuHash}',
    'README.hu.html');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.bg.html',
    '{#PackageReadmeBgHash}',
    'README.bg.html');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;
#endif

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'README.md',
    '{#PackageAssetReadmeHash}',
    'package README.md');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'CHANGELOG.md',
    '{#PackageChangelogHash}',
    'CHANGELOG.md');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'LICENSE',
    '{#PackageLicenseHash}',
    'LICENSE');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := VerifyInstalledFileHash(
    AddBackslash(WizardDirValue) + 'THIRD_PARTY_NOTICES.md',
    '{#PackageNoticesHash}',
    'THIRD_PARTY_NOTICES.md');
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := ValidateOriginalMapReadmeBundle();
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := ValidateCustomMapBundle();
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  ValidationError := ValidateServerConfigBundle();
  if ValidationError <> '' then
  begin
    Result := ValidationError;
    Exit;
  end;

  Log('Verified installed Muff Mode payload hashes, docs, configs, and version markers.');
end;

procedure WriteInstallReceipt();
var
  ReceiptFile: String;
  ReceiptText: String;
begin
  ReceiptFile := AddBackslash(GetBaseq2Dir(WizardDirValue)) + 'muffmode-install-receipt.txt';
  ReceiptText :=
    'Muff Mode installer receipt' + #13#10 +
    'Version: {#AppVersion}' + #13#10 +
    'Channel: {#Channel}' + #13#10 +
    'InstalledAtLocal: ' + GetDateTimeString('yyyy-mm-dd hh:nn:ss', '-', ':') + #13#10 +
    'InstallRoot: ' + WizardDirValue + #13#10 +
    'GameDllSHA256: {#PackageGameDllHash}' + #13#10 +
    'UpdaterSHA256: {#PackageUpdaterHash}' + #13#10 +
    'ReadmeSHA256: {#PackageReadmeHash}' + #13#10 +
    'PackageReadmeSHA256: {#PackageAssetReadmeHash}' + #13#10 +
    'ChangelogSHA256: {#PackageChangelogHash}' + #13#10 +
    'LicenseSHA256: {#PackageLicenseHash}' + #13#10 +
    'ThirdPartyNoticesSHA256: {#PackageNoticesHash}' + #13#10 +
    'VersionManifestSHA256: {#PackageVersionManifestHash}' + #13#10 +
    'OriginalReadmesIndexSHA256: {#PackageOriginalReadmesIndexHash}' + #13#10 +
    'OriginalReadme2Box4SHA256: {#PackageOriginalReadme2Box4Hash}' + #13#10 +
    'OriginalReadmeAerowalkSHA256: {#PackageOriginalReadmeAerowalkHash}' + #13#10 +
    'OriginalReadmeBroken2SHA256: {#PackageOriginalReadmeBroken2Hash}' + #13#10 +
    'OriginalReadmeFleshrefSHA256: {#PackageOriginalReadmeFleshrefHash}' + #13#10 +
    'OriginalReadmeGrindSHA256: {#PackageOriginalReadmeGrindHash}' + #13#10 +
    'OriginalReadmeZtn2dm1SHA256: {#PackageOriginalReadmeZtn2dm1Hash}' + #13#10 +
    'OriginalReadmeZtn2dm2SHA256: {#PackageOriginalReadmeZtn2dm2Hash}' + #13#10 +
    'OriginalReadmeZtn2dm3SHA256: {#PackageOriginalReadmeZtn2dm3Hash}' + #13#10 +
    'OriginalReadmeZtn2dm5SHA256: {#PackageOriginalReadmeZtn2dm5Hash}' + #13#10 +
    'MapAerowalkSHA256: {#PackageMapAerowalkHash}' + #13#10 +
    'MapColdzeroSHA256: {#PackageMapColdzeroHash}' + #13#10 +
    'MapCrucibleSHA256: {#PackageMapCrucibleHash}' + #13#10 +
    'MapKmachineSHA256: {#PackageMapKmachineHash}' + #13#10 +
    'MapPowertripSHA256: {#PackageMapPowertripHash}' + #13#10 +
    'MapRageSHA256: {#PackageMapRageHash}' + #13#10 +
    'MapRailgun101SHA256: {#PackageMapRailgun101Hash}' + #13#10 +
    'MapReclamationSHA256: {#PackageMapReclamationHash}' + #13#10 +
    'MapRecyclerSHA256: {#PackageMapRecyclerHash}' + #13#10 +
    'Entity2Box4SHA256: {#PackageEntity2Box4Hash}' + #13#10 +
    'EntityAerowalkSHA256: {#PackageEntityAerowalkHash}' + #13#10 +
    'EntityGromDm3SHA256: {#PackageEntityGromDm3Hash}' + #13#10 +
    'EntityKmachineSHA256: {#PackageEntityKmachineHash}' + #13#10 +
    'EntityKoldduel1SHA256: {#PackageEntityKoldduel1Hash}' + #13#10 +
    'EntityParadm4SHA256: {#PackageEntityParadm4Hash}' + #13#10 +
    'EntityTrdm04aSHA256: {#PackageEntityTrdm04aHash}' + #13#10 +
    'EntityVd6dm2SHA256: {#PackageEntityVd6dm2Hash}' + #13#10 +
    'EntityVenDm2SHA256: {#PackageEntityVenDm2Hash}' + #13#10 +
    'EntityZtn2dm5SHA256: {#PackageEntityZtn2dm5Hash}' + #13#10 +
    'ServerConfigGuideSHA256: {#PackageConfigReadmeHash}' + #13#10 +
    'MapCycleExampleSHA256: {#PackageMapCycleExampleHash}' + #13#10 +
    'MapPoolExampleSHA256: {#PackageMapPoolExampleHash}' + #13#10 +
    'ServerBaseConfigSHA256: {#PackageServerBaseHash}' + #13#10 +
    'FactoriesConfigSHA256: {#PackageFactoriesHash}' + #13#10;

  if LastBackupFile <> '' then
    ReceiptText := ReceiptText + 'PreviousGameDllBackup: ' + LastBackupFile + #13#10
  else
    ReceiptText := ReceiptText + 'PreviousGameDllBackup: none created' + #13#10;

  if LastConfigBackupDir <> '' then
    ReceiptText := ReceiptText + 'PreviousServerConfigBackup: ' + LastConfigBackupDir + #13#10
  else
    ReceiptText := ReceiptText + 'PreviousServerConfigBackup: none created' + #13#10;

  if SaveStringToFile(ReceiptFile, ReceiptText, False) then
    Log('Wrote Muff Mode install receipt: ' + ReceiptFile)
  else
    Log('Could not write Muff Mode install receipt: ' + ReceiptFile);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  CanonicalizeSelectedInstallDirForInstall();
  Result := ValidateSafeInstallDir();
  if Result <> '' then
    Exit;

  Result := ValidateInstallTargetForInstall();
  if Result <> '' then
    Exit;

  Result := ValidateInstallTreeForReparsePoints();
  if Result <> '' then
    Exit;

  Result := ValidateQuake2NotRunning();
  if Result <> '' then
    Exit;

  Result := ValidateTargetWritable();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  BackupError: String;
  VerificationError: String;
  BackupMessage: String;
  SafetyError: String;
begin
  if CurStep = ssInstall then
  begin
    SafetyError := ValidateInstallTreeForReparsePoints();
    if SafetyError <> '' then
    begin
      SuppressibleMsgBox(SafetyError, mbError, MB_OK, IDOK);
      Abort;
    end;

    BackupError := BackupExistingServerConfigs();
    if BackupError <> '' then
    begin
      SuppressibleMsgBox(BackupError, mbError, MB_OK, IDOK);
      Abort;
    end;

    BackupError := BackupExistingGameDll();
    if BackupError <> '' then
    begin
      SuppressibleMsgBox(BackupError, mbError, MB_OK, IDOK);
      Abort;
    end;
  end;
  if CurStep = ssPostInstall then
  begin
    VerificationError := ValidateInstalledPayload();
    if VerificationError <> '' then
    begin
      SuppressibleMsgBox(VerificationError, mbError, MB_OK, IDOK);
      Abort;
    end;

    CleanupObsoleteAerowalkMap();
    WriteInstallReceipt();

    if (LastBackupFile <> '') or (LastConfigBackupDir <> '') then
    begin
      if WizardSilent() then
      begin
        if LastConfigBackupDir <> '' then
          Log('Existing server config backup created at: ' + LastConfigBackupDir);
        if LastBackupFile <> '' then
          Log('Existing game_x64.dll backup created at: ' + LastBackupFile);
      end
      else
      begin
        BackupMessage := '';
        if LastConfigBackupDir <> '' then
          BackupMessage :=
            'Existing server-base.cfg and factories.cfg files were backed up before the packaged configs were installed:' +
            #13#10#13#10 + LastConfigBackupDir;

        if LastBackupFile <> '' then
        begin
          if BackupMessage <> '' then
            BackupMessage := BackupMessage + #13#10#13#10;
          BackupMessage :=
            BackupMessage +
            'The existing Quake II game_x64.dll was backed up before Muff Mode was installed:' +
            #13#10#13#10 + LastBackupFile + #13#10#13#10 +
            'If older Muff Mode version marker files were present, matching .version.txt and .version.json files were saved next to that backup.';
        end;

        MsgBox(BackupMessage, mbInformation, MB_OK);
      end;
    end;
  end;
end;
