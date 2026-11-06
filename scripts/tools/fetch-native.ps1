# Downloads the native libraries and the speech models that are deliberately not in git.
#
#   .\scripts\tools\fetch-native.ps1
#   .\scripts\tools\fetch-native.ps1 -Force
#
# sherpa-onnx's Windows DLLs come from the official Python wheel at a pinned version. No prebuilt C++
# archive is published for Windows, and the wheel carries exactly the libraries needed. Note that
# they live in the separate sherpa-onnx-core package rather than sherpa-onnx.
#
# The wheel also carries sherpa-onnx-c-api.lib, an MSVC-format import library, alongside the DLL.
# MinGW links the DLL directly and never touches this file; MSVC needs it to link at all.
#
# Android speaks through the platform's own TextToSpeech engine, so none of this ships there.
# -AndroidAbis controls which ABIs get OpenSSL; the Windows emulator runs x86_64.
#
# The speech model is fetched too: too large for version control, where every clone would carry it
# forever. Only the C API header is committed.
#
# Python does the bz2 extraction because Windows tar has no bzip2 filter. It is already a prerequisite
# for the dictionary importer.

param(
    [switch]$Force,
    [string[]]$AndroidAbis = @("x86_64")
)

$SherpaVersion = "1.13.8"
$OnnxRuntimeVersion = "1.28.2"
$ModelBaseUrl = "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models"

# Qt Creator's own Android setup wizard points at this same repository. Qt ships no OpenSSL for
# Android at all, for licensing reasons, and QSslSocket refuses every connection without it.
$OpenSslCommit = "b71f1470962019bd89534a2919f5925f93bc5779"
$OpenSslBaseUrl = "https://raw.githubusercontent.com/KDAB/android_openssl/$OpenSslCommit/ssl_3"

$Models = @(
    @{ Name = "supertonic-3"; Archive = "sherpa-onnx-supertonic-3-tts-int8-2026-05-11" },
    @{ Name = "piper-fr-siwis-medium"; Archive = "vits-piper-fr_FR-siwis-medium" },
    @{ Name = "piper-es-claude-high"; Archive = "vits-piper-es_MX-claude-high" },
    @{ Name = "piper-en-amy-medium"; Archive = "vits-piper-en_US-amy-medium" }
)

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Vendor = Join-Path $Root "third_party\sherpa-onnx"
$ModelsRoot = Join-Path $Vendor "models"
$Staging = Join-Path $env:TEMP "IkonikaFetch"

New-Item -ItemType Directory -Force -Path $Vendor | Out-Null
New-Item -ItemType Directory -Force -Path $ModelsRoot | Out-Null
New-Item -ItemType Directory -Force -Path $Staging | Out-Null

# ---------------------------------------------------------------- native libraries

$RequiredDlls = @("sherpa-onnx-c-api.dll", "onnxruntime.dll")
$RequiredLibs = @("sherpa-onnx-c-api.lib")
$HaveDlls = $true

foreach ($Name in ($RequiredDlls + $RequiredLibs))
{
    if (-not (Test-Path (Join-Path $Vendor $Name)))
    {
        $HaveDlls = $false
    }
}

if ($Force -or -not $HaveDlls)
{
    Write-Output "Fetching sherpa-onnx $SherpaVersion libraries..."

    $WheelDirectory = Join-Path $Staging "wheel"

    # Emptied first. Leaving an older run's wheel behind means the wrong one gets picked below.
    if (Test-Path $WheelDirectory)
    {
        [System.IO.Directory]::Delete($WheelDirectory, $true)
    }

    New-Item -ItemType Directory -Force -Path $WheelDirectory | Out-Null

    python -m pip download "sherpa-onnx-core==$SherpaVersion" `
        --no-deps --dest $WheelDirectory --quiet --disable-pip-version-check

    if ($LASTEXITCODE -ne 0)
    {
        Write-Output "pip download failed"
        exit 1
    }

    $Wheel = Get-ChildItem $WheelDirectory -Filter "sherpa_onnx_core-*.whl" | Select-Object -First 1

    if ($null -eq $Wheel)
    {
        Write-Output "no core wheel downloaded"
        exit 1
    }

    # A wheel is a zip. Expand-Archive needs the extension to say so.
    $AsZip = Join-Path $Staging "sherpa.zip"
    Copy-Item $Wheel.FullName $AsZip -Force

    $Unpacked = Join-Path $Staging "unpacked"

    if (Test-Path $Unpacked)
    {
        [System.IO.Directory]::Delete($Unpacked, $true)
    }

    Expand-Archive -Path $AsZip -DestinationPath $Unpacked -Force

    foreach ($Name in ($RequiredDlls + $RequiredLibs))
    {
        $Found = Get-ChildItem $Unpacked -Recurse -File -Filter $Name | Select-Object -First 1

        if ($null -eq $Found)
        {
            Write-Output "  missing $Name in the wheel"
            exit 1
        }

        Copy-Item $Found.FullName (Join-Path $Vendor $Name) -Force
        Write-Output ("  {0}  {1:N1} MB" -f $Name, ($Found.Length / 1MB))
    }
}
else
{
    Write-Output "sherpa-onnx libraries already present"
}

# -------------------------------------------------------------- OpenSSL, Android

$OpenSslDir = Join-Path $Root "third_party\android-openssl"

foreach ($Abi in $AndroidAbis)
{
    $OpenSslDestination = Join-Path $OpenSslDir $Abi
    $HaveOpenSsl = (Test-Path (Join-Path $OpenSslDestination "libcrypto_3.so")) -and
                   (Test-Path (Join-Path $OpenSslDestination "libssl_3.so"))

    if (-not $Force -and $HaveOpenSsl)
    {
        Write-Output "OpenSSL Android libraries already present for $Abi"
        continue
    }

    Write-Output "Fetching OpenSSL Android libraries for $Abi..."

    New-Item -ItemType Directory -Force -Path $OpenSslDestination | Out-Null

    foreach ($Name in @("libcrypto_3.so", "libssl_3.so"))
    {
        Invoke-WebRequest -Uri "$OpenSslBaseUrl/$Abi/$Name" `
            -OutFile (Join-Path $OpenSslDestination $Name) -UseBasicParsing

        $Size = (Get-Item (Join-Path $OpenSslDestination $Name)).Length
        Write-Output ("  {0}  {1:N1} MB" -f $Name, ($Size / 1MB))
    }
}

# ------------------------------------------------------------------------ models

# Python rather than tar, which on Windows has no bzip2 filter. Flattens the archive's own top-level
# folder into the destination so the layout does not depend on how the archive was built.
$ExtractScript = @"
import os, shutil, sys, tarfile
archive, destination = sys.argv[1], sys.argv[2]
staging = archive + '-extract'
if os.path.isdir(staging):
    shutil.rmtree(staging)
with tarfile.open(archive, 'r:bz2') as handle:
    handle.extractall(staging)
entries = [os.path.join(staging, e) for e in os.listdir(staging)]
source = entries[0] if len(entries) == 1 and os.path.isdir(entries[0]) else staging
os.makedirs(destination, exist_ok=True)
for entry in os.listdir(source):
    full = os.path.join(source, entry)
    target = os.path.join(destination, entry)
    if os.path.isdir(full):
        if os.path.isdir(target):
            shutil.rmtree(target)
        shutil.copytree(full, target)
    else:
        shutil.copy2(full, target)
shutil.rmtree(staging)
"@

$ExtractPath = Join-Path $Staging "extract.py"
Set-Content -Path $ExtractPath -Value $ExtractScript -Encoding utf8

foreach ($Model in $Models)
{
    $Destination = Join-Path $ModelsRoot $Model.Name

    # Both families put at least one .onnx at the top level, so that is the presence check.
    $Present = (Test-Path $Destination) -and
               ((Get-ChildItem $Destination -Filter "*.onnx" -File -ErrorAction SilentlyContinue).Count -gt 0)

    if (-not $Force -and $Present)
    {
        Write-Output "$($Model.Name) already present"
        continue
    }

    Write-Output "Fetching $($Model.Name)..."

    $Archive = Join-Path $Staging "$($Model.Archive).tar.bz2"

    if (-not (Test-Path $Archive))
    {
        Invoke-WebRequest -Uri "$ModelBaseUrl/$($Model.Archive).tar.bz2" -OutFile $Archive -UseBasicParsing
    }

    python $ExtractPath $Archive $Destination

    if ($LASTEXITCODE -ne 0)
    {
        Write-Output "  extraction failed"
        exit 1
    }

    $Size = (Get-ChildItem $Destination -Recurse -File | Measure-Object -Sum Length).Sum
    Write-Output ("  {0:N1} MB" -f ($Size / 1MB))
}

# ----------------------------------------------------------------- onnxruntime C API header
#
# The in-context translator drives onnxruntime directly through its C API. The header must match the
# onnxruntime.dll version that ships in the sherpa-onnx wheel above, because the C API is versioned
# by ORT_API_VERSION. Fetching the wrong header fails at run time, not at build time.
#
# The header is large but plain text; only the one file is needed, no import library, because the
# C API is reached through function pointers returned by OrtGetApiBase. The DLL itself already
# landed in third_party/sherpa-onnx from the wheel.

$OnnxIncludeDir = Join-Path $Root "third_party\onnxruntime\include"
$OnnxHeaders = @("onnxruntime_c_api.h", "onnxruntime_ep_c_api.h", "onnxruntime_error_code.h")
$OnnxHeaderBaseUrl = "https://raw.githubusercontent.com/microsoft/onnxruntime/v$OnnxRuntimeVersion/include/onnxruntime/core/session"
New-Item -ItemType Directory -Force -Path $OnnxIncludeDir | Out-Null

$HaveOnnxHeaders = $true

foreach ($Name in $OnnxHeaders)
{
    if (-not (Test-Path (Join-Path $OnnxIncludeDir $Name)))
    {
        $HaveOnnxHeaders = $false
    }
}

if (-not $Force -and $HaveOnnxHeaders)
{
    Write-Output "onnxruntime C API header already present"
}
else
{
    Write-Output "Fetching onnxruntime $OnnxRuntimeVersion C API headers..."

    foreach ($Name in $OnnxHeaders)
    {
        $Destination = Join-Path $OnnxIncludeDir $Name

        try
        {
            Invoke-WebRequest -Uri "$OnnxHeaderBaseUrl/$Name" -OutFile $Destination -UseBasicParsing
        }
        catch
        {
            Write-Output "  download failed: $_"
            exit 1
        }

        Write-Output ("  {0}  {1:N0} KB" -f $Name, ((Get-Item $Destination).Length / 1KB))
    }
}

# -------------------------------------------------------------------- MT models
#
# OPUS-MT, MarianMT encoder-decoder, exported to ONNX. One model per language pair: the app picks the
# directory from the pair being learned, so adding a language here is what makes the reader able to
# translate a sentence in it.
#
# int8 weights are used, saved under the fp32 file names the app loads. On the 50-sentence French
# benchmark in TranslationTests.cpp they matched fp32 on 45 sentences and were no worse on the other
# 5, at about half the size. The decoder file is the "merged" build that switches internally between
# the first step and the cached-past steps, so one session handles the whole decode loop.
#
# The tokenizer is the original Marian setup: source.spm and target.spm (SentencePiece unigram models,
# 32000 pieces each, real scores) plus the merged vocab.json. The tokenizer.json that
# onnx-community also ships cannot be used: its unigram scores were rescored and segment several words
# differently than the model was trained on, which flips the translation. The app reads the .spm files
# with a small built-in SentencePiece reader, no external dependency.
#
# Around 230 MB per pair, which is why they are fetched rather than committed.

$MtPairs = @(
    @{ Pair = "fr-en"; Repository = "onnx-community/opus-mt-fr-en" },
    @{ Pair = "es-en"; Repository = "onnx-community/opus-mt-es-en" },
    @{ Pair = "pt-en"; Repository = "Xenova/opus-mt-ROMANCE-en" },
    @{ Pair = "it-en"; Repository = "Xenova/opus-mt-it-en" },
    @{ Pair = "en-uk"; Repository = "Xenova/opus-mt-en-uk"; Settings = '{ "excludedCharacters": "\u044b\u044d\u044a\u0451\u042b\u042d\u042a\u0401" }' }
)

foreach ($MtPair in $MtPairs)
{
    $Pair = $MtPair.Pair
    $MtBaseUrl = "https://huggingface.co/$($MtPair.Repository)/resolve/main"
    $MtModelRoot = Join-Path $Root "third_party\models\opus-mt-$Pair"
    New-Item -ItemType Directory -Force -Path $MtModelRoot | Out-Null

    Write-Output "opus-mt-$Pair"

    if ($MtPair.Settings)
    {
        [System.IO.File]::WriteAllText((Join-Path $MtModelRoot "translation.json"), $MtPair.Settings, [System.Text.UTF8Encoding]::new($false))
    }

    $MtFiles = @(
        @{ Name = "source.spm"; Url = "$MtBaseUrl/source.spm" },
        @{ Name = "target.spm"; Url = "$MtBaseUrl/target.spm" },
        @{ Name = "vocab.json"; Url = "$MtBaseUrl/vocab.json" },
        @{ Name = "encoder_model.onnx"; Url = "$MtBaseUrl/onnx/encoder_model_int8.onnx" },
        @{ Name = "decoder_model_merged.onnx"; Url = "$MtBaseUrl/onnx/decoder_model_merged_int8.onnx" }
    )

    foreach ($File in $MtFiles)
    {
        $Target = Join-Path $MtModelRoot $File.Name

        if (-not $Force -and (Test-Path $Target))
        {
            Write-Output "  $($File.Name) already present"
            continue
        }

        Write-Output "  Fetching $($File.Name)..."
        try
        {
            Invoke-WebRequest -Uri $File.Url -OutFile $Target -UseBasicParsing
        }
        catch
        {
            Write-Output "    download failed: $_"
            exit 1
        }

        Write-Output ("    {0:N1} MB" -f ((Get-Item $Target).Length / 1MB))
    }
}

Write-Output "Done."
