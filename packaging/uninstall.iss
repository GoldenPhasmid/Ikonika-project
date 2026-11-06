[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if UninstallSilent() or (CurUninstallStep <> usPostUninstall) then
    exit;

  DelTree(ExpandConstant('{localappdata}\Ikonika'), True, True, True);

  if MsgBox('Also delete your words, progress and settings from this computer?' + #13#10 + #13#10
      + 'If you sync with a Google account, they stay there. If you do not, this is the only copy.',
      mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
    DelTree(ExpandConstant('{userappdata}\Ikonika'), True, True, True);
end;
