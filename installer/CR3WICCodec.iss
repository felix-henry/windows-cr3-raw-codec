#ifndef SourceDir
  #define SourceDir "..\dist\CR3WICCodec-x64"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}
AppName=Canon CR3 WIC RAW Codec
AppVersion={#AppVersion}
AppPublisher=CR3 WIC Codec Contributors
DefaultDirName={autopf}\CR3WICCodec
DefaultGroupName=Canon CR3 WIC RAW Codec
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=CR3WICCodec-Setup-x64-v{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
RestartApplications=no
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
Source: "{#SourceDir}\CR3WICCodec.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace regserver
Source: "{#SourceDir}\libraw.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#SourceDir}\vcomp140.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace skipifsourcedoesntexist
Source: "{#SourceDir}\test_decoder.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE.LGPL.LibRaw.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\LICENSE.CDDL.LibRaw.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Run]
; Grant Read & Execute permissions to ALL APPLICATION PACKAGES (S-1-15-2-1)
Filename: "{sys}\icacls.exe"; Parameters: """{app}"" /grant ""*S-1-15-2-1:(OI)(CI)(RX)"" /Q"; Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\CR3WICCodec.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "UnregCR3WIC"

[Code]
function IsVCRedistInstalled(): Boolean;
var
  Installed: Cardinal;
begin
  Result := RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1);
  if not Result then
    Result := FileExists(ExpandConstant('{sys}\vcruntime140_1.dll'));
end;

function InitializeSetup(): Boolean;
var
  ErrorCode: Integer;
begin
  Result := True;
  if not IsVCRedistInstalled() then
  begin
    if MsgBox(
      'Microsoft Visual C++ 2015-2022 Redistributable (x64) was not detected.'#13#10 +
      'LibRaw requires it to run. Open the official Microsoft download page now?',
      mbConfirmation, MB_YESNO) = IDYES then
    begin
      ShellExec('open', 'https://aka.ms/vs/17/release/vc_redist.x64.exe', '', '', SW_SHOW, ewNoWait, ErrorCode);
    end;
  end;
end;
