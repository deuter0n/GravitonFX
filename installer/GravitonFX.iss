; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 deuter0n
;
; Inno Setup 7 script for the per-user installer (also builds with 6.3 or newer). Built by tools\package.ps1:
;   ISCC.exe /DAppVersion=<x.y.z> /O<dist> installer\GravitonFX.iss
;
; - Installs for the current user only, without admin rights, to
;   %LOCALAPPDATA%\Programs\Graviton FX.
; - A newer installer upgrades in place (same AppId, closes a running copy,
;   removes files the new version no longer has); settings, presets and the
;   Start with Windows entry are kept.
; - Uninstalling removes everything the installer and the app created, and
;   asks whether to delete the settings and presets too.

#ifndef AppVersion
  #error Pass the version: ISCC /DAppVersion=x.y.z GravitonFX.iss
#endif

[Setup]
AppId={{5B0E6D2C-8F4A-4C7E-9B21-6A3D7F1E2C90}
AppName=Graviton FX
AppVersion={#AppVersion}
AppVerName=Graviton FX {#AppVersion}
AppPublisher=deuter0n
AppPublisherURL=https://github.com/deuter0n/GravitonFX
AppSupportURL=https://github.com/deuter0n/GravitonFX/issues
AppUpdatesURL=https://github.com/deuter0n/GravitonFX/releases
DefaultDirName={localappdata}\Programs\Graviton FX
DisableDirPage=yes
DefaultGroupName=Graviton FX
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=GravitonFX-Setup-{#AppVersion}
SetupIconFile=..\assets\icon\app.ico
UninstallDisplayIcon={app}\GravitonFX.exe
UninstallDisplayName=Graviton FX
LicenseFile=..\LICENSE
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
; A running copy is closed by the [Code] below (it hides to the tray on a
; normal close request, so Windows' own way of closing apps doesn't work).
CloseApplications=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[InstallDelete]
; Upgrades: start from an empty program folder, so files a new version no
; longer uses don't stay behind.
Type: filesandordirs; Name: "{app}\*"

[Files]
Source: "..\build\release\GravitonFX.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
; Microsoft's WebView2 bootstrapper, downloaded by tools\package.ps1; used only
; when the runtime is missing, then deleted.
Source: "redist\MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: WebView2Missing

[Icons]
Name: "{autoprograms}\Graviton FX"; Filename: "{app}\GravitonFX.exe"
Name: "{autodesktop}\Graviton FX"; Filename: "{app}\GravitonFX.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "Installing the Microsoft Edge WebView2 runtime..."; Flags: waituntilterminated; Check: WebView2Missing
Filename: "{app}\GravitonFX.exe"; Description: "Start Graviton FX"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Everything in the program folder, and the app's cache (WebView2 data).
Type: filesandordirs; Name: "{app}"
Type: filesandordirs; Name: "{localappdata}\GravitonFX"

[Code]
const
  AppWindowClass = 'GravitonFXMainWindow';
  WM_ENDSESSION = $0016;
  SYNCHRONIZE = $00100000;
  WAIT_OBJECT_0 = 0;
  WebView2Key = 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';

{ The runtime is installed if EdgeUpdate lists a version, per machine or per user. }
function WebView2Missing: Boolean;
var
  Version: String;
begin
  Result := not (
    (RegQueryStringValue(HKLM64, WebView2Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0')) or
    (RegQueryStringValue(HKLM32, WebView2Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0')) or
    (RegQueryStringValue(HKCU, WebView2Key, 'pv', Version) and (Version <> '') and (Version <> '0.0.0.0')));
end;

{ Closes a running Graviton FX before installing or uninstalling: says so
  (unless silent), then sends it the "session is ending" message, which makes
  the app save its settings and quit. }
function GetWindowThreadProcessId(Wnd: HWND; var ProcessId: DWORD): DWORD;
  external 'GetWindowThreadProcessId@user32.dll stdcall';
function OpenProcess(Access: DWORD; Inherit: LongBool; ProcessId: DWORD): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(Handle: THandle; Milliseconds: DWORD): DWORD;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): LongBool;
  external 'CloseHandle@kernel32.dll stdcall';

function CloseRunningApp(Silent: Boolean): Boolean;
var
  Wnd: HWND;
  Pid: DWORD;
  Process: THandle;
begin
  Result := True;
  Wnd := FindWindowByClassName(AppWindowClass);
  if Wnd = 0 then
    Exit;
  if not Silent and (MsgBox('Graviton FX is running. It will be closed to continue.', mbInformation, MB_OKCANCEL) <> IDOK) then begin
    Result := False;
    Exit;
  end;
  { Wait for the process itself, not just its window: after the window
    closes, the app still saves, stops the engine and shuts down WebView2. }
  Pid := 0;
  GetWindowThreadProcessId(Wnd, Pid);
  Process := OpenProcess(SYNCHRONIZE, False, Pid);
  PostMessage(Wnd, WM_ENDSESSION, 1, 0);
  if Process <> 0 then begin
    Result := WaitForSingleObject(Process, 10000) = WAIT_OBJECT_0;
    CloseHandle(Process);
    if Result then
      Exit;
  end;
  Result := False;
  if not Silent then
    MsgBox('Graviton FX could not be closed. Quit it from its tray icon, then try again.', mbError, MB_OK);
end;

function InitializeSetup: Boolean;
begin
  Result := CloseRunningApp(WizardSilent);
end;

function InitializeUninstall: Boolean;
begin
  Result := CloseRunningApp(UninstallSilent);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    { The app writes this value itself (Start with Windows). }
    RegDeleteValue(HKCU, RunKey, 'GravitonFX');
  if CurUninstallStep = usPostUninstall then
    { A silent uninstall keeps them; otherwise ask (No is the default). }
    if DirExists(ExpandConstant('{userappdata}\GravitonFX')) and not UninstallSilent and
       (MsgBox('Also delete your Graviton FX settings and presets?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES) then
      DelTree(ExpandConstant('{userappdata}\GravitonFX'), True, True, True);
end;
