#ifndef TargetArch
  #define TargetArch "x64"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\CR3WICCodec-" + TargetArch
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
#ifndef AppVersion
  #define AppVersion "1.5.0"
#endif

#if TargetArch == "arm64"
  #define AllowedArch "arm64"
  #define Install64Arch "arm64"
  #define VCRedistRegArch "ARM64"
  #define VCRedistExe "vc_redist.arm64.exe"
  #define VCRedistUrl "https://aka.ms/vs/17/release/vc_redist.arm64.exe"
#else
  #define AllowedArch "x64compatible"
  #define Install64Arch "x64compatible"
  #define VCRedistRegArch "x64"
  #define VCRedistExe "vc_redist.x64.exe"
  #define VCRedistUrl "https://aka.ms/vs/17/release/vc_redist.x64.exe"
#endif

[Setup]
AppId={{D7E4A3B1-5C8F-4A9E-9B2D-6F1A8E3C4D50}
AppName=Canon CR3 WIC RAW Codec
AppVersion={#AppVersion}
AppPublisher=CR3 WIC Codec Contributors
DefaultDirName={autopf}\CR3WICCodec
DefaultGroupName=Canon CR3 WIC RAW Codec
DisableProgramGroupPage=yes
DirExistsWarning=no
ShowLanguageDialog=no
SetupIconFile=..\assets\CR3WICCodec.ico
UninstallDisplayIcon={uninstallexe}
ArchitecturesAllowed={#AllowedArch}
ArchitecturesInstallIn64BitMode={#Install64Arch}
MinVersion=10.0.17763
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#OutputDir}
OutputBaseFilename=CR3WICCodec-Setup-{#TargetArch}-v{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
RestartApplications=no
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
Source: "{#SourceDir}\CR3WICCodec.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#SourceDir}\libraw.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#SourceDir}\vcomp140.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace skipifsourcedoesntexist
Source: "{#SourceDir}\test_decoder.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE.LGPL.LibRaw.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\LICENSE.CDDL.LibRaw.txt"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\scripts\*"; DestDir: "{app}\scripts"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
; Grant Read & Execute permissions to ALL APPLICATION PACKAGES (S-1-15-2-1) when installed system-wide
Filename: "{sys}\icacls.exe"; Parameters: """{app}"" /grant ""*S-1-15-2-1:(OI)(CI)(RX)"" /Q"; Flags: runhidden waituntilterminated; Check: IsAdminInstallMode
; Register COM/WIC codec under HKLM (admin) or HKCU (per-user standard account)
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\CR3WICCodec.dll"""; Flags: runhidden waituntilterminated; Check: IsAdminInstallMode
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /n /i:user ""{app}\CR3WICCodec.dll"""; Flags: runhidden waituntilterminated; Check: not IsAdminInstallMode

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\CR3WICCodec.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "UnregCR3WICAdmin"; Check: IsAdminInstallMode
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s /n /i:user ""{app}\CR3WICCodec.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "UnregCR3WICUser"; Check: not IsAdminInstallMode

[Code]
var
  DownloadPage: TDownloadWizardPage;

function IsVCRedistInstalled(): Boolean;
var
  Installed: Cardinal;
begin
  Result := RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\{#VCRedistRegArch}', 'Installed', Installed) and (Installed = 1);
  if not Result then
    Result := FileExists(ExpandConstant('{sys}\vcruntime140_1.dll'));
end;

procedure InitializeWizard;
begin
  if ActiveLanguage = 'german' then
    DownloadPage := CreateDownloadPage(
      'Microsoft VC++ Runtime herunterladen',
      'LibRaw benoetigt die Visual C++ 2015-2022 Redistributable ({#VCRedistRegArch}). Wird automatisch heruntergeladen...',
      nil)
  else
    DownloadPage := CreateDownloadPage(
      'Downloading Microsoft VC++ Runtime',
      'LibRaw requires the Visual C++ 2015-2022 Redistributable ({#VCRedistRegArch}). Downloading automatically...',
      nil);
  DownloadPage.ShowBaseNameInsteadOfUrl := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ErrMsg: String;
  PromptText: String;
  DummyCode: Integer;
begin
  Result := True;
  if (CurPageID = wpReady) and (not IsVCRedistInstalled()) then
  begin
    DownloadPage.Clear;
    DownloadPage.Add('{#VCRedistUrl}', '{#VCRedistExe}', '');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
      except
        ErrMsg := GetExceptionMessage;
        if ActiveLanguage = 'german' then
          PromptText :=
            'Der automatische Download der Visual C++ Redistributable ({#VCRedistRegArch}) ist fehlgeschlagen:'#13#10 +
            ErrMsg + #13#10#13#10 +
            'Soll die offizielle Microsoft-Downloadseite stattdessen im Browser geoeffnet werden?'
        else
          PromptText :=
            'Failed to automatically download the Visual C++ Redistributable ({#VCRedistRegArch}):'#13#10 +
            ErrMsg + #13#10#13#10 +
            'Open the official Microsoft download page in your browser instead?';

        if not WizardSilent() then
        begin
          if SuppressibleMsgBox(PromptText, mbError, MB_YESNO, IDNO) = IDYES then
            ShellExec('open', '{#VCRedistUrl}', '', '', SW_SHOW, ewNoWait, DummyCode);
        end;
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end;
end;

function RunElevatedOrDirect(const ExePath, Params: String; var ResultCode: Integer): Boolean;
begin
  if IsAdminInstallMode then
    Result := Exec(ExePath, Params, '', SW_SHOW, ewWaitUntilTerminated, ResultCode)
  else
    Result := ShellExec('', ExePath, Params, '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExePath: String;
  Params: String;
  ResultCode: Integer;
begin
  Result := '';
  if IsVCRedistInstalled() then
    Exit;

  ExePath := ExpandConstant('{tmp}\{#VCRedistExe}');
  if not FileExists(ExePath) then
  begin
    if ActiveLanguage = 'german' then
      Result := 'Visual C++ Redistributable ({#VCRedistRegArch}) konnte nicht gefunden werden.'
    else
      Result := 'Visual C++ Redistributable ({#VCRedistRegArch}) installer file is missing.';
    Exit;
  end;

  if WizardSilent() then
    Params := '/install /quiet /norestart'
  else
    Params := '/install /passive /norestart';

  if (not RunElevatedOrDirect(ExePath, Params, ResultCode)) or
     ((ResultCode <> 0) and (ResultCode <> 3010) and (ResultCode <> 1638)) then
  begin
    if ActiveLanguage = 'german' then
      Result := 'Installation der Microsoft VC++ Redistributable ({#VCRedistRegArch}) fehlgeschlagen (Code ' + IntToStr(ResultCode) + ').'
    else
      Result := 'Failed to install Microsoft VC++ Redistributable ({#VCRedistRegArch}) (exit code ' + IntToStr(ResultCode) + ').';
    Exit;
  end;

  if ResultCode = 3010 then
    NeedsRestart := True;
end;
