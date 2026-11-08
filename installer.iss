[Setup]
AppName=La Pauta Studio
AppVersion=1.0.0
AppPublisher=La Pauta Studio
DefaultDirName={autopf}\La Pauta Studio
DefaultGroupName=La Pauta Studio
OutputDir=installer
OutputBaseFilename=La-Pauta-Studio-Setup-1.0.0
SetupIconFile=Assets\icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
[Files]
Source: "build\Release\La-Pauta-Studio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Assets\*"; DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\La Pauta Studio"; Filename: "{app}\La-Pauta-Studio.exe"; IconFilename: "{app}\Assets\icon.ico"
Name: "{autodesktop}\La Pauta Studio"; Filename: "{app}\La-Pauta-Studio.exe"; IconFilename: "{app}\Assets\icon.ico"; Tasks: desktopicon
[Run]
Filename: "{app}\La-Pauta-Studio.exe"; Description: "{cm:LaunchProgram,La Pauta Studio}"; Flags: nowait postinstall skipifsilent
