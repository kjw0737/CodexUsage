#ifndef AppVersion
  #define AppVersion "1.0.2026.0929"
#endif

#define AppName "CodexUsage"
#define AppPublisher "Citopia"
#define RuntimeKey "Software\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"
#define RunKey "Software\Microsoft\Windows\CurrentVersion\Run"
#define RedistPath "redist\VC_redist.x64.exe"
#define AppExePath "..\x64\Release\CodexUsage.exe"

#ifndef RedistPath
  #error RedistPath must point to the official vc_redist.x64.exe. Run tools\BuildInstaller.ps1.
#endif

#ifndef AppExePath
  #error AppExePath must point to a Release x64 CodexUsage.exe. Run tools\BuildInstaller.ps1.
#endif




[Setup]
AppId={{6D17CB86-C7D7-4E44-AF3D-7B5DD620841E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/kjw0737/CodexUsage
AppSupportURL=https://github.com/kjw0737/CodexUsage/issues
AppUpdatesURL=https://github.com/kjw0737/CodexUsage/releases
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\CodexUsage\res\CodexUsage.ico
UninstallDisplayIcon={app}\CodexUsage.exe
OutputDir=..\dist
OutputBaseFilename=CodexUsage-Setup-{#AppVersion}-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#AppExePath}"; DestDir: "{app}"; DestName: "CodexUsage.exe"; Flags: ignoreversion
Source: "{#RedistPath}"; DestDir: "{tmp}"; DestName: "vc_redist.x64.exe"; Flags: deleteafterinstall

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\CodexUsage.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\CodexUsage.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Visual C++ 런타임을 설치하는 중..."; Flags: runhidden waituntilterminated; Check: NeedsVCRuntime
Filename: "{app}\CodexUsage.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
function NeedsVCRuntime: Boolean;
var
  Installed, MajorVersion, MinorVersion: Cardinal;
begin
  Result := True;
  if RegQueryDWordValue(HKLM64, '{#RuntimeKey}', 'Installed', Installed) and
     RegQueryDWordValue(HKLM64, '{#RuntimeKey}', 'Major', MajorVersion) and
     RegQueryDWordValue(HKLM64, '{#RuntimeKey}', 'Minor', MinorVersion) then
    Result := (Installed <> 1) or (MajorVersion < 14) or
      ((MajorVersion = 14) and (MinorVersion < 44));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command, InstalledCommand: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;

  { Only remove this installation's own auto-start entry. Keep user settings. }
  if RegQueryStringValue(HKCU, '{#RunKey}', '{#AppName}', Command) then
  begin
    InstalledCommand := '"' + ExpandConstant('{app}\CodexUsage.exe') + '"';
    if CompareText(Command, InstalledCommand) = 0 then
      RegDeleteValue(HKCU, '{#RunKey}', '{#AppName}');
  end;
end;
