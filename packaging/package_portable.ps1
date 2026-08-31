param(
    [string]$BuildDir = "$PSScriptRoot\..\build-vs-clean\Release",
    [string]$RuntimeRoot = "C:\ProgramData\radioconda",
    [string]$OutputDir = "$PSScriptRoot\..\portable\ASRTU1_Demod_CQt"
)

$ErrorActionPreference = 'Stop'
$exe = Join-Path $BuildDir 'ASRTU1_Demod_CQt.exe'
$launcher = Join-Path $BuildDir 'ASRTU1_Launcher.exe'
$doppler = Join-Path $BuildDir 'ASRTU_Doppler.exe'
$satnogsUploader = Join-Path $BuildDir 'ASRTU_SatnogsUploader.exe'
$runtimeBin = Join-Path $RuntimeRoot 'Library\bin'
$plugins = Join-Path $RuntimeRoot 'Library\plugins'
$objdump = (Get-Command 'objdump.exe' -ErrorAction Stop).Source

if (-not (Test-Path -LiteralPath $exe)) { throw "EXE not found: $exe" }
if (-not (Test-Path -LiteralPath $runtimeBin)) { throw "Runtime bin not found: $runtimeBin" }

$resolvedOutput = [IO.Path]::GetFullPath($OutputDir)
$resolvedRoot = [IO.Path]::GetPathRoot($resolvedOutput).TrimEnd('\')
if ($resolvedOutput.TrimEnd('\') -eq $resolvedRoot) {
    throw "Unsafe portable output directory: $resolvedOutput"
}
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$portableRoot = [IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'portable')).TrimEnd('\') + '\'
$payloadRoot = [IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot 'payload')).TrimEnd('\') + '\'
$safeRoots = @($portableRoot, $payloadRoot)
$insideManagedRoot = $false
foreach ($safeRoot in $safeRoots) {
    if ($resolvedOutput.StartsWith(
            $safeRoot, [StringComparison]::OrdinalIgnoreCase)) {
        $insideManagedRoot = $true
        break
    }
}
$externalMarker = "$resolvedOutput.asrtu-portable-output"
if (Test-Path -LiteralPath $resolvedOutput) {
    if (-not $insideManagedRoot -and
        -not (Test-Path -LiteralPath $externalMarker -PathType Leaf)) {
        throw "Refusing to clean unmarked output directory outside managed roots: $resolvedOutput"
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
if (-not $insideManagedRoot) {
    Set-Content -LiteralPath $externalMarker `
        -Value 'Managed by ASRTU package_portable.ps1' -Encoding Ascii
}
Copy-Item -LiteralPath $exe -Destination (Join-Path $OutputDir 'ASRTU1_Demod_CQt.exe') -Force
if (Test-Path -LiteralPath $launcher) {
    Copy-Item -LiteralPath $launcher -Destination (Join-Path $OutputDir 'ASRTU1_Launcher.exe') -Force
}
if (Test-Path -LiteralPath $doppler) {
    Copy-Item -LiteralPath $doppler -Destination (Join-Path $OutputDir 'ASRTU_Doppler.exe') -Force
}
if (-not (Test-Path -LiteralPath $satnogsUploader)) {
    throw "SatNOGS uploader not found: $satnogsUploader"
}
Copy-Item -LiteralPath $satnogsUploader `
    -Destination (Join-Path $OutputDir 'ASRTU_SatnogsUploader.exe') -Force
$converter = Join-Path $runtimeBin 'sndfile-convert.exe'
if (-not (Test-Path -LiteralPath $converter)) { throw "Audio converter not found: $converter" }
Copy-Item -LiteralPath $converter -Destination (Join-Path $OutputDir 'sndfile-convert.exe') -Force
$translationOutput = Join-Path $OutputDir 'translations'
New-Item -ItemType Directory -Force -Path $translationOutput | Out-Null
foreach ($translationName in @('asrtu_en.qm', 'asrtu_ja.qm')) {
    $translationSource = Join-Path $PSScriptRoot "..\assets\translations\$translationName"
    if (-not (Test-Path -LiteralPath $translationSource)) {
        throw "Translation package not found: $translationSource"
    }
    Copy-Item -LiteralPath $translationSource `
        -Destination (Join-Path $translationOutput $translationName) -Force
}

$knownSystem = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
@(
    'KERNEL32.dll','USER32.dll','GDI32.dll','ADVAPI32.dll','SHELL32.dll',
    'OLE32.dll','OLEAUT32.dll','COMDLG32.dll','COMCTL32.dll','WS2_32.dll',
    'IPHLPAPI.dll','WINMM.dll','IMM32.dll','DWMAPI.dll','UXTHEME.dll',
    'SETUPAPI.dll','VERSION.dll','CRYPT32.dll','RPCRT4.dll','SHLWAPI.dll',
    'NETAPI32.dll','USERENV.dll','WTSAPI32.dll','POWRPROF.dll','AVRT.dll',
    'MFPLAT.dll','MF.dll','MFREADWRITE.dll','PROPSYS.dll','DNSAPI.dll',
    'MSVCRT.dll','NTDLL.dll'
) | ForEach-Object { [void]$knownSystem.Add($_) }

$processed = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$queue = [System.Collections.Generic.Queue[string]]::new()
$queue.Enqueue((Join-Path $OutputDir 'ASRTU1_Demod_CQt.exe'))
$queue.Enqueue((Join-Path $OutputDir 'ASRTU1_Launcher.exe'))
$queue.Enqueue((Join-Path $OutputDir 'ASRTU_Doppler.exe'))
$queue.Enqueue((Join-Path $OutputDir 'ASRTU_SatnogsUploader.exe'))
$queue.Enqueue((Join-Path $OutputDir 'sndfile-convert.exe'))

function Get-Dependencies([string]$binary) {
    & $objdump -p $binary 2>$null |
        Select-String 'DLL Name:\s*(.+)$' |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Find-RuntimeDll([string]$name) {
    $candidate = Join-Path $runtimeBin $name
    if (Test-Path -LiteralPath $candidate) { return $candidate }
    return $null
}

while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $processed.Add($binary)) { continue }
    foreach ($dependency in (Get-Dependencies $binary)) {
        if ($knownSystem.Contains($dependency) -or $dependency -like 'api-ms-win-*') { continue }
        $destination = Join-Path $OutputDir $dependency
        $source = Find-RuntimeDll $dependency
        if ($source) {
            # Always refresh runtime DLLs. Keeping an existing file made
            # incremental packages silently retain stale runtime modules.
            Copy-Item -LiteralPath $source -Destination $destination -Force
        } elseif (-not (Test-Path -LiteralPath $destination)) {
                Write-Warning "Dependency not found in radioconda (assumed system): $dependency"
                continue
        }
        $queue.Enqueue($destination)
    }
}

$platformDir = Join-Path $OutputDir 'platforms'
New-Item -ItemType Directory -Force -Path $platformDir | Out-Null
$qwindows = Join-Path $plugins 'platforms\qwindows.dll'
Copy-Item -LiteralPath $qwindows -Destination (Join-Path $platformDir 'qwindows.dll') -Force
$queue.Enqueue((Join-Path $platformDir 'qwindows.dll'))
$imageFormatsDir = Join-Path $OutputDir 'imageformats'
New-Item -ItemType Directory -Force -Path $imageFormatsDir | Out-Null
$qjpeg = Join-Path $plugins 'imageformats\qjpeg.dll'
if (-not (Test-Path -LiteralPath $qjpeg)) { throw "Qt JPEG plugin not found: $qjpeg" }
Copy-Item -LiteralPath $qjpeg -Destination (Join-Path $imageFormatsDir 'qjpeg.dll') -Force
$queue.Enqueue((Join-Path $imageFormatsDir 'qjpeg.dll'))
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $processed.Add($binary)) { continue }
    foreach ($dependency in (Get-Dependencies $binary)) {
        if ($knownSystem.Contains($dependency) -or $dependency -like 'api-ms-win-*') { continue }
        $destination = Join-Path $OutputDir $dependency
        $source = Find-RuntimeDll $dependency
        if ($source) { Copy-Item -LiteralPath $source -Destination $destination -Force }
        if (Test-Path -LiteralPath $destination) { $queue.Enqueue($destination) }
    }
}

Set-Content -LiteralPath (Join-Path $OutputDir 'qt.conf') -Encoding ASCII -Value @(
    '[Paths]',
    'Plugins=.'
)

$sgp4License = Join-Path $PSScriptRoot '..\third_party\sgp4\SGP4_LICENSE.txt'
if (Test-Path -LiteralPath $sgp4License) {
    Copy-Item -LiteralPath $sgp4License `
        -Destination (Join-Path $OutputDir 'SGP4_LICENSE.txt') -Force
}
$ssdvLicense = Join-Path $PSScriptRoot '..\third_party\ssdv_dslwp\COPYING'
if (Test-Path -LiteralPath $ssdvLicense) {
    Copy-Item -LiteralPath $ssdvLicense `
        -Destination (Join-Path $OutputDir 'SSDV_GPL-3.0.txt') -Force
}

Set-Content -LiteralPath (Join-Path $OutputDir 'README.txt') -Encoding UTF8 -Value @(
    'Astro-series Satellite Demodulator C++/Qt portable build',
    '',
    'Run ASRTU1_Demod_CQt.exe directly. No Python, radioconda or GNU Radio installation is required.',
    'The application has no console window. Runtime and FEC messages are written to ASRTU1_Demod.log next to the EXE.',
    'TCP PDU output: 127.0.0.1:9985',
    'ZeroMQ PUB output: tcp://127.0.0.1:5555',
    'ASRTU_SatnogsUploader.exe provides the Windows/Linux SatNOGS frame, station and server-status window.',
    '',
    'This package contains GNU Radio, GPL-licensed OOT modules, the OpenHoshimi ASRTU soundmodem core and the DSLWP-compatible SSDV decoder.',
    'OpenHoshimi decoder credits: BG6HNY / Hyacinth Satellite Team and the upstream HIT LilacSat soundmodem authors.',
    'Distribute these components under their applicable licenses.'
)

$files = Get-ChildItem -LiteralPath $OutputDir -File -Recurse
$bytes = ($files | Measure-Object Length -Sum).Sum
Write-Host ("Portable package: {0}" -f $OutputDir)
Write-Host ("Files: {0}; Size: {1:N1} MiB" -f $files.Count, ($bytes / 1MB))
