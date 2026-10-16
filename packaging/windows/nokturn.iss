; Instalator Nokturn dla Windows (Inno Setup 6.7+).
; Budowanie: .\build-installer.ps1 (kompiluje aplikację, generuje grafiki i uruchamia ISCC).

#define AppName "Nokturn"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define AppExe "Nokturn.exe"
#define Root ".." + "\" + ".."

[Setup]
AppId={{6B0E2F4A-3C1D-4E8B-9A57-0D2C8F1B7E3A}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=zr3mb
AppPublisherURL=https://github.com/zr3mb/apkadomuzy
AppSupportURL=https://github.com/zr3mb/apkadomuzy/issues
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Root}\installer
OutputBaseFilename=Nokturn-{#AppVersion}-setup
SetupIconFile=assets\nokturn.ico
UninstallDisplayIcon={app}\nokturn.ico
UninstallDisplayName={#AppName}
WizardStyle=modern dynamic windows11
WizardImageFile=assets\wizard-100.png,assets\wizard-200.png
WizardSmallImageFile=assets\small-100.png,assets\small-200.png
WizardImageStretch=yes
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "pl"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Root}\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "assets\nokturn.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\nokturn.ico"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\nokturn.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
; Aktualizacja z poziomu aplikacji (instalator w trybie /SILENT) — od razu uruchamiamy nową wersję.
Filename: "{app}\{#AppExe}"; Flags: nowait runasoriginaluser; Check: WizardSilent
