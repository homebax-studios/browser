
#define MyAppName "HB Browser"
#define MyAppVersion "2.1.2"
#define MyAppPublisher "Homebax Studios"
#define MyAppExeName "HB Browser.exe"

[Setup]
AppId={{B8C4D7E1-92A1-4F63-A7B2-1D8E6F5C3A10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Homebax Studios\HomebaxBrowser
DefaultGroupName=HB Browser
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=resources\icons\app.ico
OutputDir=output
OutputBaseFilename=HB-Browser-Setup
ArchitecturesInstallIn64BitMode=x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Vytvořit zástupce na ploše"; Flags: checkedonce

[Files]
Source: "HB Browser.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "resources\*"; DestDir: "{app}\resources"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\HB Browser"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\HB Browser"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\resources\icons\app.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Spustit HB Browser"; Flags: postinstall nowait skipifsilent
