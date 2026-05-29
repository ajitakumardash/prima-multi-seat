; ============================================================
; Prima Multi Seat - Inno Setup Script
; Generates PrimaMultiSeatSetup.exe
; Requires: Inno Setup 6.x + iscc.exe
; ============================================================

#define AppName      "Prima Multi Seat"
#define AppVersion   "1.0.0"
#define AppPublisher "Prima Multi Seat"
#define AppURL       "https://github.com/yourusername/prima-multi-seat"
#define AppExe       "PrimaMultiSeat.exe"
#define ServiceExe   "PrimaService.exe"

[Setup]
AppId                   = {{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}}
AppName                 = {#AppName}
AppVersion              = {#AppVersion}
AppPublisher            = {#AppPublisher}
AppPublisherURL         = {#AppURL}
AppSupportURL           = {#AppURL}
AppUpdatesURL           = {#AppURL}
DefaultDirName          = {autopf}\{#AppName}
DefaultGroupName        = {#AppName}
AllowNoIcons            = yes
LicenseFile             = .\LICENSE.txt
OutputDir               = .\Output
OutputBaseFilename      = PrimaMultiSeatSetup
SetupIconFile           = .\prima.ico
Compression             = lzma2/ultra64
SolidCompression        = yes
WizardStyle             = modern
WizardImageFile         = .\WizardLarge.bmp
WizardSmallImageFile    = .\WizardSmall.bmp
PrivilegesRequired      = admin
PrivilegesRequiredOverridesAllowed = commandline
ArchitecturesInstallIn64BitMode = x64
MinVersion              = 10.0.17763
UninstallDisplayIcon    = {app}\{#AppExe}
UninstallDisplayName    = {#AppName}
VersionInfoVersion      = {#AppVersion}.0
VersionInfoDescription  = {#AppName} Installer
VersionInfoCopyright    = 2024 {#AppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";    Description: "{cm:CreateDesktopIcon}";    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon";    Description: "Start with Windows";         GroupDescription: "Startup Options"
Name: "serviceinstall"; Description: "Install as Windows Service"; GroupDescription: "Service Options"; Flags: checkedonce

[Files]
; ── Core Engine ──────────────────────────────────────────────
Source: ".\Build\{#AppExe}";          DestDir: "{app}"; Flags: ignoreversion
Source: ".\Build\{#ServiceExe}";      DestDir: "{app}"; Flags: ignoreversion
Source: ".\Build\PrimaUI.exe";        DestDir: "{app}"; Flags: ignoreversion

; ── .NET Runtime (self-contained) ────────────────────────────
Source: ".\Build\*.dll";              DestDir: "{app}"; Flags: ignoreversion recursesubdirs

; ── Configuration & Resources ────────────────────────────────
Source: ".\config.json";              DestDir: "{app}"; Flags: ignoreversion onlyifdoesntexist
Source: ".\README.txt";               DestDir: "{app}"; Flags: ignoreversion
Source: ".\prima.ico";                DestDir: "{app}"; Flags: ignoreversion

; ── Scripts ──────────────────────────────────────────────────
Source: ".\Scripts\start-prima.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: ".\Scripts\stop-prima.bat";  DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{app}\logs";    Permissions: users-full
Name: "{app}\config";  Permissions: users-full

[Icons]
; Start Menu
Name: "{group}\{#AppName}";          Filename: "{app}\PrimaUI.exe";    IconFilename: "{app}\prima.ico"
Name: "{group}\{#AppName} (Engine)"; Filename: "{app}\{#AppExe}";      IconFilename: "{app}\prima.ico"
Name: "{group}\Uninstall {#AppName}";Filename: "{uninstallexe}"

; Desktop shortcut (optional)
Name: "{autodesktop}\{#AppName}";    Filename: "{app}\PrimaUI.exe";    IconFilename: "{app}\prima.ico"; Tasks: desktopicon

[Registry]
; ── Run at startup ────────────────────────────────────────────
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
  ValueType: string; ValueName: "{#AppName}";
  ValueData: """{app}\{#AppExe}""";
  Flags: uninsdeletevalue;
  Tasks: startupicon

; ── Uninstall info ────────────────────────────────────────────
Root: HKLM; Subkey: "SOFTWARE\{#AppName}";
  ValueType: string; ValueName: "InstallPath";
  ValueData: "{app}"; Flags: uninsdeletekey

[Run]
; Install Windows Service
Filename: "{app}\{#ServiceExe}"; Parameters: "install";
  StatusMsg: "Installing Windows Service...";
  Flags: runhidden waituntilterminated;
  Tasks: serviceinstall

; Start the service
Filename: "net"; Parameters: "start PrimaMultiSeatService";
  StatusMsg: "Starting Prima Service...";
  Flags: runhidden waituntilterminated;
  Tasks: serviceinstall

; Launch dashboard after install
Filename: "{app}\PrimaUI.exe";
  Description: "Launch Prima Multi Seat Dashboard";
  Flags: nowait postinstall skipifsilent

[UninstallRun]
; Stop and remove Windows Service
Filename: "net";              Parameters: "stop PrimaMultiSeatService"; Flags: runhidden
Filename: "{app}\{#ServiceExe}"; Parameters: "uninstall";                Flags: runhidden waituntilterminated

[Code]
// ── Version detection ────────────────────────────────────
function GetWindowsBuild(): Integer;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  Result := Version.Build;
end;

function InitializeSetup(): Boolean;
begin
  // Require Windows 10 build 17763 (October 2018 Update) or later
  if GetWindowsBuild() < 17763 then begin
    MsgBox('Prima Multi Seat requires Windows 10 (version 1809) or later.' + #13#10 +
           'Your Windows version is not supported.',
           mbError, MB_OK);
    Result := False;
    Exit;
  end;
  Result := True;
end;

// ── Admin check ─────────────────────────────────────────
function IsAdminInstall(): Boolean;
begin
  Result := IsAdminLoggedOn();
end;