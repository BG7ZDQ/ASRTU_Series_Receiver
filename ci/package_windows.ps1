# ci/package_windows.ps1
# Packages the Windows CI build into a self-contained portable folder and a
# zip artifact that runs on a stock Windows machine without conda, GNU Radio
# or radioconda installed. The folder mirrors what the local radioconda
# packaging script (packaging/package_portable.ps1) produces: every
# transitive DLL dependency is copied next to the executables, together with
# the Qt platform/image-format plugins and the built-in translations.
#
# Run inside the 'asrtu' micromamba environment:
#   micromamba run -n asrtu powershell -NoProfile -ExecutionPolicy Bypass \
#       -File ci/package_windows.ps1

[CmdletBinding()]
param(
    [string]$RepoRoot = '',
    [string]$BuildDir = '',
    [string]$OutputDir = '',
    [string]$RuntimeRoot = $env:CONDA_PREFIX,
    [string]$OotRoot = ''
)

$ErrorActionPreference = 'Stop'

# Resolve the repository root from the script location. Parameters default
# to '' instead of using $PSScriptRoot so the script also works when it is
# dot-sourced or invoked through a -Command wrapper.
$scriptDir = $PSScriptRoot
if (-not $scriptDir -and $MyInvocation.MyCommand.Path) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
if (-not $RepoRoot) {
    $RepoRoot = Split-Path -Parent $scriptDir
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot 'build-windows'
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot 'dist\windows\portable'
}

if (-not $RuntimeRoot) {
    throw 'CONDA_PREFIX is empty; run this script inside the "asrtu" micromamba environment.'
}
if (-not $OotRoot -and $env:RUNNER_TEMP) {
    $OotRoot = Join-Path $env:RUNNER_TEMP 'asrtu-oot-windows'
}

$runtimeBin = Join-Path $RuntimeRoot 'Library\bin'
$pluginsRoot = Join-Path $RuntimeRoot 'Library\plugins'
if (-not (Test-Path -LiteralPath $runtimeBin)) {
    throw "Runtime bin directory not found: $runtimeBin"
}

$ootDirs = @()
if ($OotRoot) {
    $ootDirs += Join-Path $OotRoot 'bin'
    $ootDirs += Join-Path $OotRoot 'lib'
}
$searchDirs = [System.Collections.Generic.List[string]]::new()
$searchDirs.Add($runtimeBin)
foreach ($ootDir in $ootDirs) {
    if (Test-Path -LiteralPath $ootDir) {
        $searchDirs.Add($ootDir)
    }
}

# Pick a PE dependency dumper. objdump (m2w64-binutils in the conda env)
# matches the format used by the local radioconda packaging script; dumpbin
# is the MSVC fallback that ilammy/msvc-dev-cmd places on PATH.
$objdump = Get-Command 'objdump.exe' -ErrorAction SilentlyContinue
$dumpbin = Get-Command 'dumpbin.exe' -ErrorAction SilentlyContinue
if (-not $objdump -and -not $dumpbin) {
    throw 'Neither objdump.exe nor dumpbin.exe was found on PATH; install m2w64-binutils in the conda environment.'
}

function Get-Dependencies([string]$binary) {
    if ($objdump) {
        & $objdump.Source -p $binary 2>$null |
            Select-String 'DLL Name:\s*(.+)$' |
            ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
    } else {
        & $dumpbin.Source /DEPENDENTS $binary |
            Select-String -Pattern '^\s+([A-Za-z0-9_.-]+\.dll)\s*$' |
            ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
    }
}

function Find-RuntimeDll([string]$name) {
    foreach ($dir in $searchDirs) {
        $candidate = Join-Path $dir $name
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

function Add-DependencyCopy([string]$name) {
    if ($knownSystem.Contains($name) -or $name -like 'api-ms-win-*') { return }
    $destination = Join-Path $OutputDir $name
    if (Test-Path -LiteralPath $destination) {
        $queue.Enqueue($destination)
        return
    }
    $source = Find-RuntimeDll $name
    if ($source) {
        Copy-Item -LiteralPath $source -Destination $destination -Force
        $queue.Enqueue($destination)
    }
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

# Reset only a managed portable output. CI normally writes below
# dist/windows; manual callers must opt an external directory in once by
# retaining the marker created on the first successful package operation.
$resolvedOutput = [IO.Path]::GetFullPath($OutputDir)
$resolvedRoot = [IO.Path]::GetPathRoot($resolvedOutput).TrimEnd('\')
if ($resolvedOutput.TrimEnd('\') -eq $resolvedRoot) {
    throw "Unsafe Windows package output directory: $resolvedOutput"
}
$managedRoot = [IO.Path]::GetFullPath(
    (Join-Path $RepoRoot 'dist\windows')).TrimEnd('\') + '\'
$insideManagedRoot = $resolvedOutput.StartsWith(
    $managedRoot, [StringComparison]::OrdinalIgnoreCase)
$externalMarker = "$resolvedOutput.asrtu-windows-package-output"
if (Test-Path -LiteralPath $resolvedOutput) {
    if (-not $insideManagedRoot -and
        -not (Test-Path -LiteralPath $externalMarker -PathType Leaf)) {
        throw "Refusing to clean unmarked output directory outside ${managedRoot}: $resolvedOutput"
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedOutput | Out-Null
if (-not $insideManagedRoot) {
    Set-Content -LiteralPath $externalMarker -Encoding Ascii `
        -Value 'Managed by ASRTU ci/package_windows.ps1'
}
$OutputDir = $resolvedOutput

$processed = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$queue = [System.Collections.Generic.Queue[string]]::new()

foreach ($exe in @('ASRTU1_Demod_CQt.exe', 'ASRTU1_Launcher.exe',
                    'ASRTU_Doppler.exe', 'ASRTU_SatnogsUploader.exe')) {
    $source = Join-Path $BuildDir $exe
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Application executable not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $OutputDir $exe) -Force
    $queue.Enqueue((Join-Path $OutputDir $exe))
}

# Audio converter used by the launcher when converting playback input.
$converter = Join-Path $runtimeBin 'sndfile-convert.exe'
if (Test-Path -LiteralPath $converter) {
    Copy-Item -LiteralPath $converter `
        -Destination (Join-Path $OutputDir 'sndfile-convert.exe') -Force
    $queue.Enqueue((Join-Path $OutputDir 'sndfile-convert.exe'))
} else {
    Write-Warning 'sndfile-convert.exe not found; playback conversion will not be available'
}

# Qt Windows platform plugin.
$platformDir = Join-Path $OutputDir 'platforms'
New-Item -ItemType Directory -Force -Path $platformDir | Out-Null
$qwindows = Join-Path $pluginsRoot 'platforms\qwindows.dll'
if (-not (Test-Path -LiteralPath $qwindows)) {
    throw "Qt Windows platform plugin not found: $qwindows"
}
Copy-Item -LiteralPath $qwindows -Destination (Join-Path $platformDir 'qwindows.dll') -Force
$queue.Enqueue((Join-Path $platformDir 'qwindows.dll'))

# Qt image format plugins.
$imageFormatsDir = Join-Path $OutputDir 'imageformats'
New-Item -ItemType Directory -Force -Path $imageFormatsDir | Out-Null
foreach ($plugin in @('qjpeg.dll')) {
    $pluginSource = Join-Path $pluginsRoot "imageformats\$plugin"
    if (Test-Path -LiteralPath $pluginSource) {
        Copy-Item -LiteralPath $pluginSource `
            -Destination (Join-Path $imageFormatsDir $plugin) -Force
        $queue.Enqueue((Join-Path $imageFormatsDir $plugin))
    }
}

# Resolve transitive DLL dependencies breadth-first.
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $processed.Add($binary)) { continue }
    foreach ($dependency in (Get-Dependencies $binary)) {
        Add-DependencyCopy $dependency
    }
}

# Translations live next to the executables.
$translationsDir = Join-Path $OutputDir 'translations'
New-Item -ItemType Directory -Force -Path $translationsDir | Out-Null
foreach ($name in @('asrtu_en.qm', 'asrtu_ja.qm')) {
    $source = Join-Path $RepoRoot "assets\translations\$name"
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Translation package not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $translationsDir $name) -Force
}

# Licenses shipped with the local portable package.
foreach ($license in @(
        @{ Source = 'third_party\sgp4\SGP4_LICENSE.txt'; Destination = 'SGP4_LICENSE.txt' },
        @{ Source = 'third_party\ssdv_dslwp\COPYING'; Destination = 'SSDV_GPL-3.0.txt' })) {
    $licenseSource = Join-Path $RepoRoot $license.Source
    if (Test-Path -LiteralPath $licenseSource) {
        Copy-Item -LiteralPath $licenseSource `
            -Destination (Join-Path $OutputDir $license.Destination) -Force
    }
}

Set-Content -LiteralPath (Join-Path $OutputDir 'qt.conf') -Encoding ASCII -Value @(
    '[Paths]',
    'Plugins=.'
)

# The GNU Radio qtgui module links a Qt 5 build of Qwt. Reject a Qt 6 qwt.dll
# here because its QwtColorMap::colorTable256 export (QList<unsigned int>)
# cannot satisfy the Qt 5 symbol (QVector<unsigned int>) that the qtgui module
# imports, which breaks program startup with "entry point not found".
$qwtPath = Join-Path $OutputDir 'qwt.dll'
if (Test-Path -LiteralPath $qwtPath) {
    $qwtDeps = Get-Dependencies $qwtPath
    if ($qwtDeps -contains 'Qt6Core.dll') {
        throw ('qwt.dll is a Qt 6 build; GNU Radio qtgui expects the Qt 5 ' +
            'variant. Pin qwt to a qt-main build, for example ' +
            'qwt=6.3.0=h9417a65_0.')
    }
}


# Zip the whole folder for the CI artifact.
$zipDir = Split-Path -Parent $OutputDir
New-Item -ItemType Directory -Force -Path $zipDir | Out-Null
$zipPath = Join-Path $zipDir 'asrtu-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $OutputDir '*') -DestinationPath $zipPath `
    -CompressionLevel Optimal

$files = Get-ChildItem -LiteralPath $OutputDir -File -Recurse
$bytes = ($files | Measure-Object Length -Sum).Sum
Write-Host ("Windows portable package: {0}" -f $OutputDir)
Write-Host ("Archive: {0}" -f $zipPath)
Write-Host ("Files: {0}; Size: {1:N1} MiB" -f $files.Count, ($bytes / 1MB))
