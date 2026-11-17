; Instalador de La Pauta Studio (Inno Setup 6). Se compila solo en GitHub Actions.
#define MyAppName "La Pauta Studio"
#define MyAppVersion "0.1.0"
#define MyAppExe "La Pauta Studio.exe"

[Setup]
AppId={{B7A3C1D2-5E4F-4A6B-9C8D-2F1E0A3B4C5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=La Pauta
DefaultDirName={autopf}\La Pauta Studio
DefaultGroupName=La Pauta Studio
UninstallDisplayIcon={app}\{#MyAppExe}
OutputDir=installer_output
OutputBaseFilename=LaPautaStudio-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"

[Files]
Source: "build\LaPautaStudio_artefacts\Release\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\La Pauta Studio"; Filename: "{app}\{#MyAppExe}"
Name: "{group}\Desinstalar La Pauta Studio"; Filename: "{uninstallexe}"
Name: "{autodesktop}\La Pauta Studio"; Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "Abrir La Pauta Studio"; Flags: nowait postinstall skipifsilent
