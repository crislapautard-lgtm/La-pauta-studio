
#define MyAppName "La Pauta Studio"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "La Pauta"

[Setup]
AppId={{8D6F3A21-9B47-4C82-A651-2F9B7A31D840}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}

DefaultDirName={autopf}\La Pauta Studio
DefaultGroupName=La Pauta Studio

ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin

OutputDir=InstallerOutput
OutputBaseFilename=LaPautaStudio_Installer

Compression=lzma
SolidCompression=yes

WizardStyle=modern
Uninstallable=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked

[Files]
Source: "OutputFiles\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\La Pauta Studio"; Filename: "{app}\La Pauta Studio.exe"
Name: "{autodesktop}\La Pauta Studio"; Filename: "{app}\La Pauta Studio.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\La Pauta Studio.exe"; Description: "Abrir La Pauta Studio"; Flags: postinstall nowait skipifsilent
