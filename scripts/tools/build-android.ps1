# Configures and builds the Android target. Defaults to the x86_64 Qt kit, which is what the
# Windows Android emulator runs; -Abi arm64-v8a targets an actual phone instead.
#
#   .\scripts\tools\build-android.ps1                    build Debug for the emulator (configures first time only)
#   .\scripts\tools\build-android.ps1 -Abi arm64-v8a      build Debug for a real device instead
#   .\scripts\tools\build-android.ps1 -Release            build Release instead
#   .\scripts\tools\build-android.ps1 -Configure          force a reconfigure
#   .\scripts\tools\build-android.ps1 -Apk                build then produce the APK
#   .\scripts\tools\build-android.ps1 -Aab                build then produce the Android App Bundle for Play Store
#   .\scripts\tools\build-android.ps1 -Install            build the APK then install it to the connected device/emulator
#
# Nothing here is on PATH by default, which is why every path is spelled out.

param(
    [ValidateSet("x86_64", "arm64-v8a", "armeabi-v7a", "x86")]
    [string]$Abi = "x86_64",
    [switch]$Release,
    [switch]$Configure,
    [switch]$Apk,
    [switch]$Aab,
    [switch]$Install
)

if ($Install -and $Aab)
{
    Write-Output "-Install requires an APK. An AAB is uploaded to Play Store and cannot be installed directly."
    exit 1
}

if ($Install -and -not $Apk)
{
    $Apk = $true
}

$QtRoot     = "C:\Qt"
$QtVersion  = "6.11.1"
$SdkRoot    = "$env:LOCALAPPDATA\Android\Sdk"
$NdkVersion = "30.0.15729638"

# Qt's own kit directory names, one per ABI.
$QtKitByAbi = @{
    "x86_64"      = "android_x86_64"
    "arm64-v8a"   = "android_arm64_v8a"
    "armeabi-v7a" = "android_armv7"
    "x86"         = "android_x86"
}

$CMake   = "$QtRoot\Tools\CMake_64\bin\cmake.exe"
$QtCmake = "$QtRoot\$QtVersion\$($QtKitByAbi[$Abi])\bin\qt-cmake.bat"
$Config  = if ($Release) { "Release" } else { "Debug" }

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

# The x86_64 build keeps its original directory, which existing tooling and docs already refer to.
# Any other ABI gets its own build tree so switching targets never mixes CMake caches.
$Build = if ($Abi -eq "x86_64")
{
    Join-Path $Root "build\android-$($Config.ToLower())"
}
else
{
    Join-Path $Root "build\android-$Abi-$($Config.ToLower())"
}

$env:PATH = "$QtRoot\Tools\Ninja;$env:PATH"

# Play requires a signed AAB, and a sideloaded release APK is signed the same way for consistency.
$NeedsSigning = $Release -and ($Apk -or $Aab)

if ($NeedsSigning)
{
    $CredentialsFile = Join-Path $Root "app\android\release-keystore-credentials.local.txt"

    if (-not (Test-Path $CredentialsFile))
    {
        Write-Output "Missing $CredentialsFile - release signing needs the keystore credentials file."
        exit 1
    }

    $Credentials = Get-Content $CredentialsFile -Raw

    function Read-Credential([string]$Pattern)
    {
        if ($Credentials -match $Pattern)
        {
            return $Matches[1]
        }

        Write-Output "Could not find '$Pattern' in $CredentialsFile"
        exit 1
    }

    $env:QT_ANDROID_KEYSTORE_PATH = Join-Path $Root (Read-Credential "Keystore path:\s*(\S+)")
    $env:QT_ANDROID_KEYSTORE_ALIAS = Read-Credential "Alias:\s*(\S+)"
    $env:QT_ANDROID_KEYSTORE_STORE_PASS = Read-Credential "Store password:\s*(\S+)"
    $env:QT_ANDROID_KEYSTORE_KEY_PASS = Read-Credential "Key password:\s*(\S+)"
}

$CacheFile = Join-Path $Build "CMakeCache.txt"

# An already-configured release build tree predates the signing variables, so it needs a
# reconfigure to pick them up even though its CMakeCache.txt already exists.
if ($NeedsSigning -and (Test-Path $CacheFile) -and -not (Select-String -Path $CacheFile -Pattern "QT_ANDROID_SIGN_AAB" -Quiet))
{
    $Configure = $true
}

$TranslationEnabled = (Test-Path $CacheFile) -and (Select-String -Path $CacheFile -Pattern '^IKONIKA_ENABLE_MACHINE_TRANSLATION:BOOL=ON$' -Quiet)

if ($Configure -or -not $TranslationEnabled)
{
    # Qt's own auto-detection expects a sibling mingw_64 host kit, which isn't installed on this
    # machine - only the msvc2022_64 desktop kit is, so the cross-compiling host path is spelled
    # out explicitly instead.
    $ConfigureArgs = @(
        "-DANDROID_SDK_ROOT=$SdkRoot",
        "-DANDROID_NDK_ROOT=$SdkRoot\ndk\$NdkVersion",
        "-DCMAKE_BUILD_TYPE=$Config",
        "-DIKONIKA_ENABLE_MACHINE_TRANSLATION=ON",
        "-DQT_HOST_PATH=$QtRoot\$QtVersion\msvc2022_64",
        "-DQT_HOST_PATH_CMAKE_DIR=$QtRoot\$QtVersion\msvc2022_64\lib\cmake"
    )

    if ($NeedsSigning)
    {
        $ConfigureArgs += "-DQT_ANDROID_SIGN_APK=ON"
        $ConfigureArgs += "-DQT_ANDROID_SIGN_AAB=ON"
    }

    & $QtCmake -S $Root -B $Build -GNinja @ConfigureArgs

    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

$BuildTarget = if ($Aab) { "aab" } elseif ($Apk) { "apk" } else { "IkonikaApp" }

& $CMake --build $Build --target $BuildTarget

if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}

if ($Install)
{
    $ApkPath = Get-ChildItem -Path $Build -Filter "*.apk" -Recurse |
        Where-Object { $_.FullName -match "apk\\debug|apk\\release" } |
        Select-Object -First 1

    if ($null -eq $ApkPath)
    {
        Write-Output "No APK found under $Build"
        exit 1
    }

    & "$SdkRoot\platform-tools\adb.exe" install -r $ApkPath.FullName
    exit $LASTEXITCODE
}
