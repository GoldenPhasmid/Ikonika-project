param([Parameter(ValueFromRemainingArguments = $true)][string[]]$PipelineArguments)
$ErrorActionPreference = 'Stop'
$BlenderExecutable = $env:LUFI_BLENDER_EXE
if (-not $BlenderExecutable)
{
    $BlenderCommand = Get-Command blender -ErrorAction SilentlyContinue
    if ($BlenderCommand) { $BlenderExecutable = $BlenderCommand.Source }
}
if (-not $BlenderExecutable)
{
    $AssociationKey = Get-Item 'Registry::HKEY_CLASSES_ROOT\.blend' -ErrorAction SilentlyContinue
    if ($AssociationKey)
    {
        $Association = $AssociationKey.GetValue('')
        $CommandKey = Get-Item ('Registry::HKEY_CLASSES_ROOT\' + $Association + '\shell\open\command') -ErrorAction SilentlyContinue
        if ($CommandKey -and $CommandKey.GetValue('') -match '^"([^"]+)"') { $BlenderExecutable = $matches[1] }
    }
}
if (-not $BlenderExecutable)
{
    $Candidates = Get-ChildItem ($env:ProgramFiles + '\Blender Foundation\Blender*\blender.exe') -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
    if ($Candidates) { $BlenderExecutable = $Candidates[0].FullName }
}
if (-not $BlenderExecutable) { throw 'Blender not found. Export from the Blender extension, or set LUFI_BLENDER_EXE.' }
& $BlenderExecutable --background --disable-autoexec --python-exit-code 1 --python (Join-Path $PSScriptRoot 'pipeline.py') -- @PipelineArguments
exit $LASTEXITCODE
