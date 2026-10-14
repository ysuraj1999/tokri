#define AppName "Tokri"
#define AppExe  "Tokri.exe"
#define AppVer  "2026.01.01"
#define AppId "5B1B5853-971C-453B-A9C2-21B566CA3A5B"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVer}
DefaultDirName={autopf}\Tokri
DisableDirPage=no
DisableProgramGroupPage=no
DefaultGroupName={#AppName}
OutputBaseFilename=TokriSetup
OutputDir=dist\installer
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=lowest
SetupIconFile=resources\com.ysuraj.Tokri.ico
UninstallDisplayIcon={app}\{#AppExe}
WizardStyle=modern

[Files]
Source: "dist\Windows\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "Tokri.exe"
Source: "dist\Windows\Tokri.exe"; DestDir: "{app}"; Flags: ignoreversion

[Tasks]
Name: desktopicon; Description: "Create a &desktop shortcut"; Flags: unchecked
Name: autostart; Description: "Automatically start {#AppName} when Windows starts"; GroupDescription: "Startup:"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{userdesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExe}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
