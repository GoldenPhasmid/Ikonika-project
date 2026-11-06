# Wipes every place Ikonika keeps per-user state, release and debug alike, so a fresh run behaves
# like a first install. Closes a running copy first, since it holds its own database and log open,
# and retries the removal a couple of times in case something (the installer's own silent relaunch,
# antivirus scanning a just-touched file) still has a brief hold on it.
#
#   .\scripts\tools\clear-local-state.ps1

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$Targets = @(
    (Join-Path $env:APPDATA "Ikonika"),
    (Join-Path $env:LOCALAPPDATA "Ikonika"),
    (Join-Path $Root "local")
)

$attempts = 3
$delaySeconds = 2

for ($i = 1; $i -le $attempts; $i++)
{
    Stop-Process -Name IkonikaApp -Force -ErrorAction SilentlyContinue

    foreach ($Target in $Targets)
    {
        if (Test-Path $Target)
        {
            try
            {
                Remove-Item -Recurse -Force $Target -ErrorAction Stop
            }
            catch
            {
                # Left for the next attempt to retry.
            }
        }
    }

    if (-not ($Targets | Where-Object { Test-Path $_ }))
    {
        break
    }

    Start-Sleep -Seconds $delaySeconds
}

foreach ($Target in $Targets)
{
    if (Test-Path $Target)
    {
        Write-Output "STILL PRESENT: $Target"
    }
    else
    {
        Write-Output "Cleared: $Target"
    }
}
