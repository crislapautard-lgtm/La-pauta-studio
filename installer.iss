[Setup]
AppName=La Pauta Studio
AppVersion=1.0
DefaultDirName={pf}\La Pauta Studio
DefaultGroupName=La Pauta Studio
OutputDir=.
OutputBaseFilename=LaPautaStudio_Installer
Compression=lzma
SolidCompression=yes

[Files]
Source: "Source\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
