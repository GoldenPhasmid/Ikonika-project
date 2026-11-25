param([Parameter(ValueFromRemainingArguments = $true)][string[]]$DemoArguments)
$ErrorActionPreference = 'Stop'
$Archive = Join-Path $PSScriptRoot 'ikonika-demo-windows.zip'
if (-not (Test-Path -LiteralPath $Archive)) { throw 'Portable demo missing. A developer must run build.cmd and commit the runtime ZIP.' }
$Stream = [IO.File]::OpenRead($Archive)
try { $Digest = [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($Stream)).Replace('-', '').ToLowerInvariant() }
finally { $Stream.Dispose() }
$AnimationFolder = Split-Path $PSScriptRoot -Parent
$RuntimeFolder = Join-Path $AnimationFolder ('local\demo-runtime\' + $Digest.Substring(0, 16))
if (-not (Test-Path -LiteralPath (Join-Path $RuntimeFolder '.complete')))
{
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Directory]::CreateDirectory($RuntimeFolder) | Out-Null
    $Package = [IO.Compression.ZipFile]::OpenRead($Archive)
    try
    {
        foreach ($Entry in $Package.Entries)
        {
            $Target = [IO.Path]::GetFullPath((Join-Path $RuntimeFolder $Entry.FullName))
            if (-not $Target.StartsWith($RuntimeFolder + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid runtime archive entry' }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Target)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($Entry, $Target, $true)
        }
    }
    finally { $Package.Dispose() }
    [IO.File]::WriteAllText((Join-Path $RuntimeFolder '.complete'), $Digest)
}
& (Join-Path $RuntimeFolder 'LufiLottieTrial.exe') @DemoArguments
exit $LASTEXITCODE
