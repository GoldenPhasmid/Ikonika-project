param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Destination
)

# Windows Defender's real-time scanner opens newly-written .dll/.onnx files for a moment right
# after they land on disk, which occasionally collides with this copy and fails it outright.
# Retrying a few times with a short backoff rides out that window instead of failing the build.
$attempts = 5
$delaySeconds = 2

for ($i = 1; $i -le $attempts; $i++)
{
    try
    {
        if (Test-Path $Destination) { Remove-Item -Recurse -Force $Destination -ErrorAction Stop }

        Copy-Item -Path $Source -Destination $Destination -Recurse -Force -ErrorAction Stop
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
