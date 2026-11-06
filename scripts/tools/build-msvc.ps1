# Configures and builds with the MSVC 2022 toolchain, which is what the application half is built
# and tested against.
#
#   .\scripts\tools\build-msvc.ps1              build Debug (configures first time only)
#   .\scripts\tools\build-msvc.ps1 -Release     build Release instead
#   .\scripts\tools\build-msvc.ps1 -Configure   force a reconfigure
#   .\scripts\tools\build-msvc.ps1 -Test        build then run the tests, speech and slow excluded
#   .\scripts\tools\build-msvc.ps1 -Slow        build then run only the slow tests
#   .\scripts\tools\build-msvc.ps1 -Speech      build then run only the speech tests
#   .\scripts\tools\build-msvc.ps1 -Walk        build then run the screen and navigation walks
#   .\scripts\tools\build-msvc.ps1 -Run         build then launch the app
#   .\scripts\tools\build-msvc.ps1 -Package     build then produce the installer (Release only)
#
# Nothing here is on PATH by default, which is why every path is spelled out.

param(
    [switch]$Release,
    [switch]$Configure,
    [switch]$Test,
    [switch]$Speech,
    [switch]$Slow,
    [switch]$Walk,
    [switch]$Run,
    [switch]$Package,
    [switch]$Msix
)

if ($Package -and -not $Release)
{
    Write-Output "-Package needs -Release"
    exit 1
}

if ($Msix -and -not $Release)
{
    Write-Output "-Msix needs -Release"
    exit 1
}

# Deliberately not setting $ErrorActionPreference to Stop. CMake and MSBuild write ordinary
# warnings to stderr, and Stop would treat those as fatal. Exit codes are checked explicitly instead.

$QtRoot    = "C:\Qt"
$QtVersion = "6.11.1"

$CMake  = "$QtRoot\Tools\CMake_64\bin\cmake.exe"
$QtKit  = "$QtRoot\$QtVersion\msvc2022_64"
$Config = if ($Release) { "Release" } else { "Debug" }

$Root  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Build = Join-Path $Root "build\msvc-$($Config.ToLower())"

# Qt's bin has to be on PATH, or the built executables cannot find Qt6Cored.dll and friends when
# run from here rather than from Qt Creator, which sets this up itself.
$env:PATH = "$QtKit\bin;$env:PATH"

# CMake wants forward slashes in compiler paths.
$QtKitForward = $QtKit.Replace('\', '/')

$CacheFile = Join-Path $Build "CMakeCache.txt"
$TranslationEnabled = (Test-Path $CacheFile) -and (Select-String -Path $CacheFile -Pattern '^IKONIKA_ENABLE_MACHINE_TRANSLATION:BOOL=ON$' -Quiet)

if ($Configure -or -not $TranslationEnabled)
{
    # Debug builds skip qmlcachegen entirely: every QML file's compile step depends on the
    # module's shared .qmltypes/qmldir/.qrc files, which get touched whenever any QML_ELEMENT
    # class's metadata changes - so nearly every controller edit reruns qmlcachegen for every
    # screen. QML then loads from source text and is parsed at runtime instead of precompiled,
    # which is the tradeoff Qt's own docs recommend for a faster develop-debug loop. Release
    # keeps cachegen on for the smaller, AOT-compiled binary that actually ships.
    $QmlNoCacheGen = if ($Release) { "OFF" } else { "ON" }

    & $CMake -S $Root -B $Build -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_PREFIX_PATH="$QtKitForward" `
        -DQT_QML_NO_CACHEGEN="$QmlNoCacheGen" `
        -DIKONIKA_ENABLE_MACHINE_TRANSLATION=ON

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

Get-Process IkonikaApp -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq [System.IO.Path]::GetFullPath("$PSScriptRoot\..\..\build\msvc-debug\app\Debug\IkonikaApp.exe") } | Stop-Process -Force

& $CMake --build $Build --config $Config

if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}

$CoreTests = Join-Path $Build "tests\$Config\CoreTests.exe"
$AppTests  = Join-Path $Build "app\$Config\AppTests.exe"

# Speech is excluded from the default run because those 20 tests load the models and synthesise real
# audio: 17.9 seconds against 0.1 for the other 190, and they need the fetched models present. Run
# -Speech after touching anything under Speech. AppTests has no [speech]-tagged tests of its own -
# the filter is harmless there, kept only so both binaries run the same way.
if ($Test)
{
    & $CoreTests "~[speech]~[slow]"

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }

    & $AppTests "~[speech]~[slow]"

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

if ($Slow)
{
    & $CoreTests "[slow]~[speech]"

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }

    & $AppTests "[slow]~[speech]"

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

if ($Speech)
{
    & $CoreTests "[speech]"

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

if ($Walk)
{
    $Exe = Join-Path $Build "app\$Config\IkonikaApp.exe"
    $Log = Join-Path $Root "local\logs\Ikonika.log"
    $DesktopScreens = "today,vocabulary,words,reader,settings"
    $MobileScreens = "today,vocabulary,dictionary,settings,profile,reader"

    function Invoke-Walk
    {
        param($EnvVars, $ExtraArgs, $Label)

        foreach ($Key in $EnvVars.Keys)
        {
            Set-Item -Path "Env:$Key" -Value $EnvVars[$Key]
        }

        $Process = Start-Process -FilePath $Exe -ArgumentList (@("--minimised") + $ExtraArgs) -PassThru

        try
        {
            Wait-Process -Id $Process.Id -Timeout 30 -ErrorAction Stop
        }
        catch
        {
            Write-Host "$Label timed out, killing"
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        }

        foreach ($Key in $EnvVars.Keys)
        {
            Remove-Item -Path "Env:$Key" -ErrorAction SilentlyContinue
        }

        $Bad = @(Select-String -Path $Log -Pattern "NAV FAIL") +
               @(Select-String -Path $Log -Pattern "\| warnings [1-9]")

        if ($Bad.Count -gt 0)
        {
            Write-Host "$Label FAILED"
            $Bad | ForEach-Object { Write-Host "  $($_.Line.Trim())" }
        }
        else
        {
            Write-Host "$Label passed"
        }

        return $Bad.Count
    }

    $FailCount = 0
    $FailCount += Invoke-Walk @{ IKONIKA_SCREEN_WALK = $DesktopScreens } @() "Screen walk (desktop)"
    $FailCount += Invoke-Walk @{ IKONIKA_SCREEN_WALK = $MobileScreens } @("-mobile") "Screen walk (mobile)"
    $FailCount += Invoke-Walk @{ IKONIKA_NAV_WALK = "1" } @() "Navigation walk (desktop)"
    $FailCount += Invoke-Walk @{ IKONIKA_NAV_WALK = "1" } @("-mobile") "Navigation walk (mobile)"

    if ($FailCount -gt 0)
    {
        exit 1
    }
}

if ($Run)
{
    & (Join-Path $Build "app\$Config\IkonikaApp.exe")
    exit $LASTEXITCODE
}

if ($Package)
{
    & $CMake --build $Build --config $Config --target package

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

if ($Msix)
{
    & python (Join-Path $PSScriptRoot "package-msix.py") --config $Config
    exit $LASTEXITCODE
}
