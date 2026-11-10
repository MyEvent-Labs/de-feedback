; MyEvent Labs De-Feedback - Windows installer (ALPHA, unsigned)
;
; Build (from the repository root):
;   ISCC /DSrcRoot="build-configure\DeFeedback_artefacts\configure" installer\de-feedback.iss
;
; Smoke test build (no admin, install into a temp folder):
;   ISCC /DSMOKE_TEST /DSrcRoot="..." /DOutDir="..." installer\de-feedback.iss

#ifndef SrcRoot
  #define SrcRoot "..\build-configure\DeFeedback_artefacts\configure"
#endif
#ifndef RepoRoot
  #define RepoRoot ".."
#endif
#ifndef OutDir
  #define OutDir "..\dist"
#endif
#ifndef AppVersion
  #define AppVersion "0.0.1"
#endif

[Setup]
AppId={{7E3B7B3E-2C7A-4C4E-9E7B-0D2B2D9F1A01}
AppName=MyEvent Labs De-Feedback (ALPHA)
AppVersion={#AppVersion}
AppVerName=MyEvent Labs De-Feedback {#AppVersion} ALPHA
AppPublisher=MyEvent Labs
AppPublisherURL=https://github.com/MyEvent-Labs/de-feedback
AppSupportURL=https://github.com/MyEvent-Labs/de-feedback/issues
AppUpdatesURL=https://github.com/MyEvent-Labs/de-feedback/releases

DefaultDirName={autopf}\MyEvent Labs\De-Feedback
DefaultGroupName=MyEvent Labs\De-Feedback
DisableProgramGroupPage=yes
AllowNoIcons=yes

; The shipped installer writes the VST3 into the shared VST3 folder, so it
; needs elevation. The smoke-test build does not.
#ifdef SMOKE_TEST
PrivilegesRequired=lowest
#else
PrivilegesRequired=admin
#endif

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

OutputDir={#OutDir}
#ifdef SMOKE_TEST
OutputBaseFilename=DeFeedback-{#AppVersion}-alpha-smoketest-setup
#else
OutputBaseFilename=DeFeedback-{#AppVersion}-alpha-win64-unsigned-setup
#endif

Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile={#RepoRoot}\LICENSE
InfoBeforeFile={#RepoRoot}\installer\ALPHA_WARNING.txt
UninstallDisplayName=MyEvent Labs De-Feedback (ALPHA)
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; VST3 bundle (whole folder).
Source: "{#SrcRoot}\VST3\MyEvent Labs De-Feedback.vst3\*"; DestDir: "{code:GetVst3Dir}"; Flags: recursesubdirs createallsubdirs ignoreversion

; Standalone application.
Source: "{#SrcRoot}\Standalone\MyEvent Labs De-Feedback.exe"; DestDir: "{app}"; Flags: ignoreversion

; Documentation installed alongside the standalone.
Source: "{#RepoRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\docs\ALPHA_LIMITATIONS.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#RepoRoot}\installer\ALPHA_WARNING.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\MyEvent Labs De-Feedback (ALPHA)"; Filename: "{app}\MyEvent Labs De-Feedback.exe"
Name: "{group}\Alpha limitations and warning"; Filename: "{app}\ALPHA_WARNING.txt"
Name: "{group}\Uninstall MyEvent Labs De-Feedback"; Filename: "{uninstallexe}"
Name: "{autodesktop}\MyEvent Labs De-Feedback (ALPHA)"; Filename: "{app}\MyEvent Labs De-Feedback.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MyEvent Labs De-Feedback.exe"; Description: "Launch MyEvent Labs De-Feedback (standalone, ALPHA)"; Flags: nowait postinstall skipifsilent

[Code]
var
  Vst3Root: String;

function GetVst3Dir(Param: String): String;
begin
  Result := Vst3Root;
end;

function InitializeSetup(): Boolean;
var
  override_: String;
begin
  override_ := ExpandConstant('{param:VST3DIR|}');
  if override_ <> '' then
    Vst3Root := override_
  else
    Vst3Root := ExpandConstant('{commonpf}\Common Files\VST3');

  Vst3Root := Vst3Root + '\MyEvent Labs De-Feedback.vst3';
  Result := True;
end;
