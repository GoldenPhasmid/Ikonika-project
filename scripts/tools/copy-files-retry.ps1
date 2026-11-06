param(
    [Parameter(Mandatory = $true)][string]$Destination,
    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)][string[]]$Files
)

# Same rationale as copy-directory-retry.ps1: a freshly-scanned file occasionally can't be read
# for a moment, so retry instead of failing the build outright.
$attempts = 5
$delaySeconds = 2

if (-not (Test-Path $Destination)) { New-Item -ItemType Directory -Force -Path $Destination | Out-Null }

for ($i = 1; $i -le $attempts; $i++)
{
    try
    {
        foreach ($file in $Files)
        {
            Copy-Item -Path $file -Destination $Destination -Force -ErrorAction Stop
        }

        exit 0
    }
    catch
    {
        if ($i -eq $attempts)
        {
            Write-Error "Copy failed after $attempts attempts: $($_.Exception.Message)"
            exit 1
        }

        Start-Sleep -Seconds $delaySeconds
    }
}
