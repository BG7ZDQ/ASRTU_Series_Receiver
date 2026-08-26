param(
    [string]$BuildDir = "$PSScriptRoot\..\build-vs-clean\Release",
    [string]$RuntimeRoot = "C:\ProgramData\radioconda",
    [string]$OutputDir = "$PSScriptRoot\..\portable\ASRTU1_Demod_CQt"
)

$ErrorActionPreference = 'Stop'
$exe = Join-Path $BuildDir 'ASRTU1_Demod_CQt.exe'
$launcher = Join-Path $BuildDir 'ASRTU1_Launcher.exe'
$doppler = Join-Path $BuildDir 'ASRTU_Doppler.exe'
$runtimeBin = Join-Path $RuntimeRoot 'Library\bin'
$plugins = Join-Path $RuntimeRoot 'Library\plugins'
$objdump = (Get-Command 'objdump.exe' -ErrorAction Stop).Source

if (-not (Test-Path -LiteralPath $exe)) { throw "EXE not found: $exe" }
if (-not (Test-Path -LiteralPath $runtimeBin)) { throw "Runtime bin not found: $runtimeBin" }

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$oldLog = Join-Path $OutputDir 'ASRTU1_Demod.log'
if (Test-Path -LiteralPath $oldLog) { Remove-Item -LiteralPath $oldLog -Force }
Copy-Item -LiteralPath $exe -Destination (Join-Path $OutputDir 'ASRTU1_Demod_CQt.exe') -Force
if (Test-Path -LiteralPath $launcher) {
    Copy-Item -LiteralPath $launcher -Destination (Join-Path $OutputDir 'ASRTU1_Launcher.exe') -Force
}
if (Test-Path -LiteralPath $doppler) {
    Copy-Item -LiteralPath $doppler -Destination (Join-Path $OutputDir 'ASRTU_Doppler.exe') -Force
}
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
        if (-not (Test-Path -LiteralPath $destination)) {
            $source = Find-RuntimeDll $dependency
            if (-not $source) {
                Write-Warning "Dependency not found in radioconda (assumed system): $dependency"
                continue
            }
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
        $queue.Enqueue($destination)
    }
}

$platformDir = Join-Path $OutputDir 'platforms'
New-Item -ItemType Directory -Force -Path $platformDir | Out-Null
$qwindows = Join-Path $plugins 'platforms\qwindows.dll'
Copy-Item -LiteralPath $qwindows -Destination (Join-Path $platformDir 'qwindows.dll') -Force
$queue.Enqueue((Join-Path $platformDir 'qwindows.dll'))
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $processed.Add($binary)) { continue }
    foreach ($dependency in (Get-Dependencies $binary)) {
        if ($knownSystem.Contains($dependency) -or $dependency -like 'api-ms-win-*') { continue }
        $destination = Join-Path $OutputDir $dependency
        if (-not (Test-Path -LiteralPath $destination)) {
            $source = Find-RuntimeDll $dependency
            if ($source) { Copy-Item -LiteralPath $source -Destination $destination -Force }
        }
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

Set-Content -LiteralPath (Join-Path $OutputDir 'README.txt') -Encoding UTF8 -Value @(
    'Astro-series Satellite Demodulator C++/Qt portable build',
    '',
    'Run ASRTU1_Demod_CQt.exe directly. No Python, radioconda or GNU Radio installation is required.',
    'The application has no console window. Runtime and FEC messages are written to ASRTU1_Demod.log next to the EXE.',
    'TCP PDU output: 127.0.0.1:9985',
    'ZeroMQ PUB output: tcp://127.0.0.1:5555',
    '',
    'This package contains GNU Radio and GPL-licensed OOT modules. Distribute it under the applicable licenses.'
)

$files = Get-ChildItem -LiteralPath $OutputDir -File -Recurse
$bytes = ($files | Measure-Object Length -Sum).Sum
Write-Host ("Portable package: {0}" -f $OutputDir)
Write-Host ("Files: {0}; Size: {1:N1} MiB" -f $files.Count, ($bytes / 1MB))
