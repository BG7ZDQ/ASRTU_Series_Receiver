$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$runtimePrefix = Join-Path $env:CONDA_PREFIX 'Library'
$ootPrefix = Join-Path $env:RUNNER_TEMP 'asrtu-oot-windows'
$ootSource = Join-Path $env:RUNNER_TEMP 'asrtu-oot-windows-src'
$dependencyConfig = Join-Path $PSScriptRoot 'oot-dependencies.env'
$dependencies = @{}
Get-Content $dependencyConfig | ForEach-Object {
    if ($_ -match '^([A-Z_]+)=(.+)$') {
        $dependencies[$Matches[1]] = $Matches[2]
    }
}

function Invoke-Cmake([string[]]$Arguments) {
    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CMake failed with exit code $LASTEXITCODE"
    }
}

function Build-Oot([string]$Name, [string]$Repository, [string]$Revision,
                   [string[]]$Options) {
    $sourceDir = Join-Path $ootSource $Name
    $buildDir = Join-Path $sourceDir 'build'
    Remove-Item -LiteralPath $sourceDir -Recurse -Force -ErrorAction SilentlyContinue
    & git clone --no-checkout $Repository $sourceDir
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to clone $Repository"
    }
    & git -C $sourceDir checkout --detach $Revision
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to check out $Revision"
    }
    if ($Name -eq 'gr-lilacsat') {
        $libraryCmake = Join-Path $sourceDir 'lib\CMakeLists.txt'
        $contents = Get-Content -LiteralPath $libraryCmake -Raw
        $old = 'add_library(gnuradio-lilacsat SHARED ${lilacsat_sources})'
        $new = @'
if(WIN32)
    set(lilacsat_sources
        ccsds/tab.c
        ccsds/viterbi27.c
        vitfilt27_fb_impl.cc)
endif()

add_library(gnuradio-lilacsat SHARED ${lilacsat_sources})
'@
        if (-not $contents.Contains($old)) {
            throw "Unable to prepare the Windows gr-lilacsat source set"
        }
        Set-Content -LiteralPath $libraryCmake -NoNewline `
            -Value $contents.Replace($old, $new)
    }
    $configureArgs = @('-S', $sourceDir, '-B', $buildDir, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
        "-DCMAKE_INSTALL_PREFIX=$ootPrefix",
        "-DCMAKE_PREFIX_PATH=$runtimePrefix") + $Options
    Invoke-Cmake $configureArgs
    Invoke-Cmake @('--build', $buildDir, '--parallel')
    Invoke-Cmake @('--install', $buildDir)
}

New-Item -ItemType Directory -Path $ootPrefix, $ootSource -Force | Out-Null
Build-Oot 'gr-lilacsat' $dependencies.LILACSAT_REPOSITORY `
    $dependencies.LILACSAT_REVISION @(
        '-DENABLE_PYTHON=OFF', '-DENABLE_DOXYGEN=OFF')
Build-Oot 'gr-hyacinth' $dependencies.HYACINTH_REPOSITORY `
    $dependencies.HYACINTH_REVISION @(
        '-DHYACINTHSAT_ENABLE_PYTHON=OFF', '-DENABLE_DOXYGEN=OFF')

$env:PATH = "$ootPrefix\bin;$ootPrefix\lib;$runtimePrefix\bin;$env:PATH"
$buildDir = Join-Path $repoRoot 'build-windows'
Invoke-Cmake @('-S', $repoRoot, '-B', $buildDir, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
    "-DCMAKE_PREFIX_PATH=$ootPrefix;$runtimePrefix",
    "-DCMAKE_INCLUDE_PATH=$ootPrefix\include;$runtimePrefix\include",
    "-DCMAKE_LIBRARY_PATH=$ootPrefix\lib;$runtimePrefix\lib",
    '-DASRTU_BUILD_BENCHMARK=OFF',
    '-DASRTU_STRICT_WARNINGS=ON',
    '-DBUILD_TESTING=ON')
Invoke-Cmake @('--build', $buildDir, '--parallel')
& ctest --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "CTest failed with exit code $LASTEXITCODE"
}
