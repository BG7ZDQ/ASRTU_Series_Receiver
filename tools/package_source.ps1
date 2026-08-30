param(
    [string]$Version = '',
    [string]$OutputRoot = "$PSScriptRoot\..\outputs"
)

$ErrorActionPreference = 'Stop'

# Derive the version from CMakeLists.txt (the single source of truth) unless
# the caller overrides it explicitly.
if (-not $Version) {
    $versionLine = Select-String `
        -LiteralPath (Join-Path $PSScriptRoot '..\CMakeLists.txt') `
        -Pattern 'project\(ASRTU1Qt VERSION ([0-9.]+)' | Select-Object -First 1
    if (-not $versionLine) {
        throw 'Unable to read the application version from CMakeLists.txt'
    }
    $Version = $versionLine.Matches[0].Groups[1].Value
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$releaseName = "ASRTU_Series_Receiver_Source_v$Version"
$stagingRoot = Join-Path $OutputRoot "source_stage_$stamp"
$releaseRoot = Join-Path $stagingRoot $releaseName
$archive = Join-Path $OutputRoot "$releaseName.zip"

New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null

foreach ($file in @('README.md', 'README_EN.md', 'README_JA.md', 'LICENSE',
                    'THIRD_PARTY.md', '.gitignore', 'CMakeLists.txt',
                    'build_release.ps1')) {
    Copy-Item -LiteralPath (Join-Path $repositoryRoot $file) -Destination $releaseRoot
}
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs') -Destination $releaseRoot -Recurse

Copy-Item -LiteralPath (Join-Path $repositoryRoot 'build_installer.ps1') `
    -Destination $releaseRoot
foreach ($directory in @('apps', 'libs', 'plugins', 'assets', 'packaging',
                          'third_party', 'tests', 'tools')) {
    $source = Join-Path $repositoryRoot $directory
    $destination = Join-Path $releaseRoot $directory
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Get-ChildItem -LiteralPath $source -Force | Where-Object {
        $_.Name -notin @('build', 'build-vs-clean', 'portable', 'stage', 'dist',
                         'bin', 'obj')
    } | Copy-Item -Destination $destination -Recurse
}

foreach ($generatedPath in @(
    'packaging\inno\stage',
    'packaging\inno\dist',
    'plugins\sdrsharp-bridge\bin',
    'plugins\sdrsharp-bridge\obj'
)) {
    $target = [IO.Path]::GetFullPath((Join-Path $releaseRoot $generatedPath))
    $safeRoot = [IO.Path]::GetFullPath($releaseRoot).TrimEnd('\') + '\'
    if (-not $target.StartsWith($safeRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe generated-output path: $target"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

if (Test-Path -LiteralPath $archive) {
    $archive = Join-Path $OutputRoot "$releaseName-$stamp.zip"
}
Compress-Archive -LiteralPath $releaseRoot -DestinationPath $archive -CompressionLevel Optimal
Write-Host "Source archive: $archive"
