; Bangla Keyboard (Windows) installer — Inno Setup. Produces a branded
; BanglaKeyboard-Setup-<ver>.exe that installs the tray app to Program Files.
; Build (after windows\build-all.bat has produced dist\bangla-tray.exe):
;   iscc banglakeyboard.iss     -> installer\dist\BanglaKeyboard-Setup-<ver>.exe
; Unsigned for now — users click "More info -> Run anyway" on SmartScreen.
;
; Keep the version in sync across THREE places: windows\VERSION, windows\tray\tray.rc,
; and MyAppVersion below.

#define MyAppName "Bangla Keyboard"
#define MyAppVersion "1.1.1"
#define MyAppPublisher "BiswasHost"
#define MyAppExe "bangla-tray.exe"
#define MyAppURL "https://github.com/wpexpertinbd/bangla-keyboard"

[Setup]
AppId={{5BD4FB21-7946-4912-98C0-C178E33747BD}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
; Per-user install: no admin/UAC, lands in %LocalAppData%\Programs, and the
; "start at sign-in" shortcut + Start Menu entry are correctly per-user. (A tray
; utility doesn't need a system-wide install.)
PrivilegesRequired=lowest
DefaultDirName={autopf}\Bangla Keyboard
DefaultGroupName=Bangla Keyboard
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputDir=dist
OutputBaseFilename=BanglaKeyboard-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupIconFile=..\tray\banglakeyboard.ico
UninstallDisplayIcon={app}\{#MyAppExe}
; The tray app force-closes via taskkill in [Code] before file copy, so don't
; involve the Restart Manager (it would stall on the hidden tray window).
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel1=Welcome to Bangla Keyboard
WelcomeLabel2=A free Bangla keyboard for Windows — type Bangla in any app and switch right from the system tray.%n%n      -   Bangla Unicode  (Ctrl+Alt+V)%n      -   Bangla Classic  (Ctrl+Alt+B)%n      -   Voice typing — Bangla (Ctrl+Alt+S) / English (Ctrl+Alt+D), needs internet%n      -   Works in every app; all English shortcuts keep working%n%nMIT licensed, free & open-source — by BiswasHost.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "startup"; Description: "Start Bangla Keyboard automatically when I sign in"; GroupDescription: "Startup:"
; Default ON. Unchecking fully omits the voice files + disables the feature (no
; menu items, nothing launched). Uses a free online speech service, so it needs internet.
Name: "voice"; Description: "Voice typing — speak Bangla/English (requires an internet connection)"; GroupDescription: "Voice typing:"
Name: "fonts"; Description: "Install 14 free Bangla Unicode fonts (SolaimanLipi, Kalpurush, Siyam Rupali, …)"; GroupDescription: "Fonts:"

[Files]
Source: "..\dist\bangla-tray.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\tray\banglakeyboard.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "USAGE.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
; Voice typing (only when the "voice" task is selected): the WebView2 host, the
; redistributable loader, and the speech page. The tray launches bangla-voice.exe
; when VoiceEnabled (set in [Registry] below).
Source: "..\dist\bangla-voice.exe";      DestDir: "{app}";       Flags: ignoreversion; Tasks: voice
Source: "..\dist\WebView2Loader.dll";    DestDir: "{app}";       Flags: ignoreversion; Tasks: voice
Source: "..\dist\voice\voice.html";      DestDir: "{app}\voice"; Flags: ignoreversion; Tasks: voice
; Free/libre Bangla Unicode fonts (same set as the macOS build). Installed per-user
; into {autofonts}; won't overwrite a font the user already has; left in place on
; uninstall. Sourced from the shared macos/ font files.
Source: "..\..\macos\src\fonts\SolaimanLipi.ttf";            DestDir: "{autofonts}"; FontInstall: "SolaimanLipi";   Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\kalpurush.ttf";               DestDir: "{autofonts}"; FontInstall: "Kalpurush";      Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\Siyamrupali.ttf";             DestDir: "{autofonts}"; FontInstall: "Siyam Rupali";   Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\AdorshoLipi_20-07-2007.ttf";  DestDir: "{autofonts}"; FontInstall: "AdorshoLipi";     Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\Lohit_14-04-2007.ttf";        DestDir: "{autofonts}"; FontInstall: "Ekushey Lohit";  Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\Mukti_1.99_PR.ttf";           DestDir: "{autofonts}"; FontInstall: "Mukti";          Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\muktinarrow.ttf";             DestDir: "{autofonts}"; FontInstall: "Mukti Narrow";   Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\akaashnormal.ttf";            DestDir: "{autofonts}"; FontInstall: "Akaash";         Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\AponaLohit.ttf";              DestDir: "{autofonts}"; FontInstall: "AponaLohit";     Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\sagarnormal.ttf";             DestDir: "{autofonts}"; FontInstall: "Sagar";          Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\mitra.ttf";                   DestDir: "{autofonts}"; FontInstall: "Mitra Mono";     Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\Bangla.ttf";                  DestDir: "{autofonts}"; FontInstall: "Bangla";         Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\BenSen.ttf";                  DestDir: "{autofonts}"; FontInstall: "BenSen";         Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts
Source: "..\..\macos\src\fonts\BenSenHandwriting.ttf";       DestDir: "{autofonts}"; FontInstall: "BenSenHandwriting"; Flags: onlyifdoesntexist uninsneveruninstall; Tasks: fonts

[Registry]
; The tray reads this to decide whether to run the voice companion + show its menu.
Root: HKCU; Subkey: "Software\BanglaKeyboard"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\BanglaKeyboard"; ValueType: dword; ValueName: "VoiceEnabled"; ValueData: "1"; Check: VoiceChecked; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\BanglaKeyboard"; ValueType: dword; ValueName: "VoiceEnabled"; ValueData: "0"; Check: not VoiceChecked; Flags: uninsdeletevalue

[Icons]
Name: "{group}\Bangla Keyboard"; Filename: "{app}\{#MyAppExe}"; IconFilename: "{app}\banglakeyboard.ico"
Name: "{group}\Uninstall Bangla Keyboard"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Bangla Keyboard"; Filename: "{app}\{#MyAppExe}"; IconFilename: "{app}\banglakeyboard.ico"; Tasks: desktopicon
Name: "{userstartup}\Bangla Keyboard"; Filename: "{app}\{#MyAppExe}"; Tasks: startup

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "Launch Bangla Keyboard now"; Flags: nowait postinstall skipifsilent

[Code]
// True when the user kept the "voice" task ticked — used to write VoiceEnabled.
function VoiceChecked: Boolean;
begin
  Result := WizardIsTaskSelected('voice');
end;

// Force-close any running tray + voice instance before install / uninstall so the
// EXEs aren't locked (the apps hide to the tray, so a window-based close won't do).
procedure KillTray;
var ResultCode: Integer;
begin
  Exec(ExpandConstant('{cmd}'), '/c taskkill /f /im {#MyAppExe}', '',
       SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec(ExpandConstant('{cmd}'), '/c taskkill /f /im bangla-voice.exe', '',
       SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  KillTray;
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  KillTray;
  Result := True;
end;
