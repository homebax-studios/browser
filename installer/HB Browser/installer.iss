
#define MyAppName "HB Browser"
#define MyAppVersion "2.5.2"
#define MyAppPublisher "Homebax Studios"
#define MyAppURL "https://homebax.vercel.app"
#define MyAppExeName "HB Browser.exe"
#define MyAppFolder "HomebaxBrowser"
#define MyAppURLTerms "https://homebax.vercel.app/terms"
#define MyAppURLPrivacy "https://homebax.vercel.app/privacy"
#define MyAppURLSupport "https://homebax.vercel.app/support"

[Setup]
AppId={{B8C4D7E1-92A1-4F63-A7B2-1D8E6F5C3A10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURLSupport}
AppUpdatesURL={#MyAppURL}

DefaultDirName={autopf}\Homebax Studios\{#MyAppFolder}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}

SetupIconFile=resources\icons\app.ico
LicenseFile=license.txt

OutputDir=output
OutputBaseFilename=HB-Browser-Setup-v{#MyAppVersion}
SetupLogging=yes

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

CloseApplications=yes
RestartApplications=no

Uninstallable=yes
CreateUninstallRegKey=yes

VersionInfoVersion=2.5.2.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=HB Browser Installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (C) 2026 Homebax Studios

[Tasks]
Name: "desktopicon"; Description: "Vytvořit zástupce na ploše"; GroupDescription: "Další možnosti:"; Flags: checkedonce

[Files]
Source: "HB Browser.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "resources\*"; DestDir: "{app}\resources"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\HB Browser"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\HB Browser"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\resources\icons\app.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Spustit HB Browser"; Flags: postinstall nowait skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\cache"
