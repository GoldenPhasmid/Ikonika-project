; Restart Manager does not restart a process that never registered for it. See docs/releases.md.
[Run]
Filename: "{app}\IkonikaApp.exe"; Flags: nowait; Check: WizardSilent

[Code]
function IkonikaPreviousInstallDir(): String;
var
  InstallDir: String;
begin
  if RegQueryStringValue(HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Uninstall\{89146247-DDFE-4522-BB79-05596AD96D7C}_is1',
      'InstallLocation', InstallDir) then
    Result := InstallDir
  else
    Result := '';
end;

procedure InitializeWizard();
var
  PreviousDir: String;
begin
  if WizardSilent() then
  begin
    PreviousDir := IkonikaPreviousInstallDir();

    if PreviousDir <> '' then
      WizardForm.DirEdit.Text := PreviousDir;
  end;
end;

function IkonikaPreviousUninstallString(): String;
var
  UninstallString: String;
begin
  if RegQueryStringValue(HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Uninstall\{89146247-DDFE-4522-BB79-05596AD96D7C}_is1',
      'UninstallString', UninstallString) then
    Result := UninstallString
  else
    Result := '';
end;

function IkonikaStripTrailingSlash(Path: String): String;
begin
  Result := Path;

  if (Length(Result) > 0) and (Result[Length(Result)] = '\') then
    Delete(Result, Length(Result), 1);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  PreviousDir: String;
  ChosenDir: String;
  UninstallString: String;
  ResultCode: Integer;
begin
  Result := True;

  if WizardSilent() or (CurPageID <> wpSelectDir) then
    exit;

  PreviousDir := IkonikaStripTrailingSlash(IkonikaPreviousInstallDir());
  ChosenDir := IkonikaStripTrailingSlash(WizardForm.DirEdit.Text);

  if (PreviousDir = '') or (CompareText(PreviousDir, ChosenDir) = 0) then
    exit;

  if MsgBox('Ikonika is already installed at ' + PreviousDir + '.' + #13#10
      + 'Remove that copy before installing here?', mbConfirmation, MB_YESNO) = IDYES then
  begin
    UninstallString := IkonikaPreviousUninstallString();

    if UninstallString <> '' then
      Exec(RemoveQuotes(UninstallString), '/VERYSILENT /NORESTART', '', SW_SHOW,
          ewWaitUntilTerminated, ResultCode);
  end;
end;
