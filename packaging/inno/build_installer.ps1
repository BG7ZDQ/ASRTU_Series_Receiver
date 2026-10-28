param(
    [string]$DecoderSource = "$PSScriptRoot\..\payload\decoder",
    [string]$ProxySource = "$PSScriptRoot\..\payload\proxy",
    [string]$SdrSharpSource = "$PSScriptRoot\..\payload\sdrsharp",
    [string]$SdrSharpApiRoot = "$PSScriptRoot\..\payload\sdrsharp-api",
    [string]$SdrppSource = "$PSScriptRoot\..\payload\sdrpp",
    [string]$PlutoDriverSource = "$PSScriptRoot\..\payload\plutosdr-driver",
    [string]$IqBridgeProject = "$PSScriptRoot\..\..\external\TinyDoppler\plugin",
    [string]$InnoSetup = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    [ValidatePattern('^[A-Za-z0-9_-]*$')][string]$BuildSuffix = '',
    [switch]$RebuildDsp,
    [string]$RuntimeRoot = 'C:\ProgramData\radioconda'
)

$ErrorActionPreference = 'Stop'

function Get-RelativePath([string]$BasePath, [string]$Path) {
    # Path.GetRelativePath is .NET Core only; Windows PowerShell 5.1 needs
    # the Uri-based fallback.
    if ([IO.Path].GetMethods() | Where-Object { $_.Name -eq 'GetRelativePath' }) {
        return [IO.Path]::GetRelativePath($BasePath, $Path)
    }
    $baseUri = [Uri](([IO.Path]::GetFullPath($BasePath).TrimEnd('\')) + '\')
    $pathUri = [Uri]([IO.Path]::GetFullPath($Path))
    return [Uri]::UnescapeDataString(
        $baseUri.MakeRelativeUri($pathUri).ToString()).Replace('/', '\')
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$stage = Join-Path $PSScriptRoot 'stage'
$binStage = Join-Path $stage 'bin'
$proxyStage = Join-Path $stage 'proxy'
$sdrSharpStage = Join-Path $stage 'sdrsharp'
$sdrppStage = Join-Path $stage 'sdrpp'
$plutoDriverStage = Join-Path $stage 'plutosdr-driver'
$dspProject = $repoRoot
$iqBridgeBuild = Join-Path $IqBridgeProject 'build_legacy.ps1'
$iqBridgePlugin = Join-Path $IqBridgeProject 'bin\Release\net46\SDRSharp.AstroSeriesBridge.dll'

foreach ($required in @(
    $DecoderSource,
    (Join-Path $ProxySource 'proxy_mmt_gui.exe'),
    (Join-Path $ProxySource 'ASRTU_Proxy.exe'),
    (Join-Path $SdrSharpSource 'SDRSharp.exe'),
    (Join-Path $SdrSharpApiRoot 'SDRSharp.Common.dll'),
    (Join-Path $SdrSharpApiRoot 'SDRSharp.Radio.dll'),
    (Join-Path $SdrSharpApiRoot 'SDRSharp.PanView.dll'),
    $iqBridgeBuild,
    $InnoSetup
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required build input not found: $required"
    }
}

if ($RebuildDsp) {
    & (Join-Path $dspProject 'build_release.ps1') -RuntimeRoot $RuntimeRoot
    if ($LASTEXITCODE -ne 0) { throw 'DSP/launcher build failed.' }
    $DecoderSource = Join-Path $dspProject 'portable\ASRTU1_Demod_CQt'
}
if (-not (Test-Path -LiteralPath (
        Join-Path $DecoderSource 'ASRTU_SatnogsUploader.exe'))) {
    throw "SatNOGS uploader not found in decoder payload: $DecoderSource"
}
if (-not (Test-Path -LiteralPath (
        Join-Path $DecoderSource 'TinyDoppler.exe'))) {
    throw "New TinyDoppler.exe not found in decoder payload: $DecoderSource"
}

# Refuse a new installer label around stale receiver executables.
$versionLine = Select-String -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') `
    -Pattern 'project\(ASRTU1Qt VERSION ([0-9.]+)' | Select-Object -First 1
$appVersion = $versionLine.Matches[0].Groups[1].Value
foreach ($name in @('ASRTU1_Demod_CQt.exe', 'ASRTU1_Launcher.exe', 'ASRTU_SatnogsUploader.exe')) {
    $binary = Join-Path $DecoderSource $name
    $actual = (Get-Item -LiteralPath $binary).VersionInfo.ProductVersion
    if ($actual -ne $appVersion) {
        throw "Stale installer input $name ($actual, expected $appVersion). Run with -RebuildDsp."
    }
}

# Rebuild the legacy SDR# bridge using Roslyn from Visual Studio Build Tools.
& $iqBridgeBuild -SdrSharpApiRoot $SdrSharpApiRoot -Configuration Release
if ($LASTEXITCODE -ne 0) { throw 'SDR# I/Q bridge plugin build failed.' }
if (-not (Test-Path -LiteralPath $iqBridgePlugin)) {
    throw "I/Q bridge DLL not found: $iqBridgePlugin"
}

$expectedStageRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\') + '\'
$resolvedStage = [IO.Path]::GetFullPath($stage)
if (-not $resolvedStage.StartsWith($expectedStageRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe stage path: $resolvedStage"
}
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path $binStage, $proxyStage, $sdrSharpStage,
    $sdrppStage, $plutoDriverStage -Force | Out-Null

# Copy runtime assets only, never cache/settings left by a local smoke test.
Get-ChildItem -LiteralPath $DecoderSource -File |
    Where-Object { $_.Extension -in @('.exe', '.dll') -or
        $_.Name -in @('qt.conf', 'README.txt', 'SGP4_LICENSE.txt', 'SSDV_GPL-3.0.txt') } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $binStage }
foreach ($resource in @('platforms', 'imageformats', 'translations')) {
    $resourceSource = Join-Path $DecoderSource $resource
    if (Test-Path -LiteralPath $resourceSource) {
        $resourceStage = Join-Path $binStage $resource
        New-Item -ItemType Directory -Path $resourceStage -Force | Out-Null
        Get-ChildItem -LiteralPath $resourceSource -File |
            Where-Object { $_.Extension -in @('.dll', '.qm') } |
            ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $resourceStage }
    }
}
Copy-Item -Path (Join-Path $ProxySource '*') -Destination $proxyStage -Recurse -Force
Copy-Item -Path (Join-Path $SdrSharpSource '*') -Destination $sdrSharpStage -Recurse -Force
if (Test-Path -LiteralPath (Join-Path $SdrppSource 'sdrpp.exe')) {
    Copy-Item -Path (Join-Path $SdrppSource '*') -Destination $sdrppStage -Recurse -Force
    # Configure the bundled SDR++ bridge; fail instead of shipping a preset
    # that no longer matches the Windows launcher.
    & (Join-Path $PSScriptRoot '../../integrations/sdrpp/configure_windows.ps1') -SdrppRoot $sdrppStage
} else {
    Write-Warning "SDR++ payload not found at $SdrppSource; the installer will omit the SDR++ component."
}
if (Test-Path -LiteralPath (Join-Path $PlutoDriverSource 'dpinst.exe')) {
    Copy-Item -Path (Join-Path $PlutoDriverSource '*') -Destination $plutoDriverStage -Recurse -Force
} else {
    Write-Warning "PlutoSDR driver payload not found at $PlutoDriverSource; the installer will omit it."
}

# All suite executables use one application-local runtime in bin/. This is
# also the Windows loader's first normal DLL directory, before System32.
# Third-party SDR applications and the legacy proxy keep their own runtimes.

$sdrSharpPlugins = Join-Path $sdrSharpStage 'Plugins'
New-Item -ItemType Directory -Path $sdrSharpPlugins -Force | Out-Null
Copy-Item -LiteralPath $iqBridgePlugin `
    -Destination (Join-Path $sdrSharpPlugins 'SDRSharp.AstroSeriesBridge.dll') -Force
Copy-Item -LiteralPath (Join-Path $IqBridgeProject 'README.md') `
    -Destination (Join-Path $sdrSharpStage 'ASRTU_IQ_BRIDGE_README.md') -Force

# Never publish local operator state.
$privateConfig = Join-Path $proxyStage 'config.cfg'
if (Test-Path -LiteralPath $privateConfig) {
    Remove-Item -LiteralPath $privateConfig -Force
}
$sdrSharpConfig = Join-Path $sdrSharpStage 'SDRSharp.config'
if (Test-Path -LiteralPath $sdrSharpConfig) {
    $configText = [IO.File]::ReadAllText($sdrSharpConfig)
    $configText = [Text.RegularExpressions.Regex]::Replace(
        $configText,
        '(<add key="core\.filePlayer\.lastFileName" value=")[^"]*("\s*/>)',
        '$1$2')
    [IO.File]::WriteAllText($sdrSharpConfig, $configText,
        [Text.UTF8Encoding]::new($false))
}

# Remove only known package files from the old split layout on upgrade.
# Never recursively remove module directories: they may contain user settings,
# Rigctl configuration, logs or other files that must remain untouched.
$staleFileEntries = [Collections.Generic.List[string]]::new()
$staleFileEntries.Add('; Generated by build_installer.ps1. Known legacy package files only.')
$legacyModules = @('decoder', 'launcher', 'doppler', 'satnogs-uploader')
$runtimeFiles = @(Get-ChildItem -LiteralPath $binStage -File -Recurse | Sort-Object FullName)
foreach ($file in $runtimeFiles) {
    $relativePath = Get-RelativePath $binStage $file.FullName
    foreach ($module in $legacyModules) {
        $staleFileEntries.Add("Type: files; Name: `"{app}\$module\$relativePath`"")
    }
}
$staleFileEntries.Add('Type: files; Name: "{app}\launcher\sdrsharp.ico"')
[IO.File]::WriteAllLines((Join-Path $stage 'core-stale.iss'), $staleFileEntries,
    [Text.UTF8Encoding]::new($false))

# One destination per file: no hard links, junctions or duplicated Qt/CRT trees.
# Inno may still share compressed sources without changing installed files.
$coreFileEntries = [Collections.Generic.List[string]]::new()
$coreFileEntries.Add('; Generated by build_installer.ps1. Do not edit.')
$sourceByHash = @{}
foreach ($moduleName in @('bin', 'proxy')) {
    $moduleRoot = Join-Path $stage $moduleName
    foreach ($file in (Get-ChildItem -LiteralPath $moduleRoot -File -Recurse | Sort-Object FullName)) {
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $relativePath = Get-RelativePath $stage $file.FullName
        if ($sourceByHash.ContainsKey($hash)) {
            $sourcePath = $sourceByHash[$hash]
        } else {
            $sourcePath = "stage\$relativePath"
            $sourceByHash[$hash] = $sourcePath
        }
        $destinationDirectory = [IO.Path]::GetDirectoryName($relativePath)
        $coreFileEntries.Add(
            "Source: `"$sourcePath`"; DestDir: `"{app}\$destinationDirectory`"; DestName: `"$($file.Name)`"; Components: core; Flags: ignoreversion")
    }
}
[IO.File]::WriteAllLines((Join-Path $stage 'core-files.iss'), $coreFileEntries,
    [Text.UTF8Encoding]::new($false))
Write-Host ("Core payload: {0} unique compressed sources; shared bin runtime, no hard links." -f
    $sourceByHash.Count)

# Resolve the application version from CMakeLists.txt so the installer never
# drifts from the single PROJECT_VERSION source.
$versionLine = Select-String -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') `
    -Pattern 'project\(ASRTU1Qt VERSION ([0-9.]+)' | Select-Object -First 1
if (-not $versionLine) {
    throw 'Unable to read the application version from CMakeLists.txt'
}
$appVersion = $versionLine.Matches[0].Groups[1].Value
$sdrppAvailable = Test-Path -LiteralPath (Join-Path $sdrppStage 'sdrpp.exe')
$plutoAvailable = Test-Path -LiteralPath (Join-Path $plutoDriverStage 'dpinst.exe')
$innoArgs = @("/DAppVersion=$appVersion", "/DAppBuildSuffix=$BuildSuffix")
if ($sdrppAvailable) { $innoArgs += '/DSdrppAvailable' }
if ($plutoAvailable) { $innoArgs += '/DPlutoAvailable' }
$innoArgs += (Join-Path $PSScriptRoot 'ASRTU1_Receiver_Setup.iss')

& $InnoSetup @innoArgs
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }

$installer = Join-Path $PSScriptRoot `
    "dist\ASRTU_Series_Receiver-${appVersion}-x86_64-WinInstaller${BuildSuffix}.exe"
# Builds created inside a DSH workspace inherit its Low mandatory label. A
# Low-labelled Setup cannot create its protected directory under the normal
# user %TEMP% or write the per-user install directory. Relabel the finished
# executable to Medium; this label is deliberately not part of the file bytes.
$icacls = Join-Path $env:SystemRoot 'System32\icacls.exe'
if ((Test-Path -LiteralPath $installer) -and (Test-Path -LiteralPath $icacls)) {
    & $icacls $installer /setintegritylevel M | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Could not set the installer integrity level to Medium: $installer"
    }
}
Write-Host "Installer: $installer"
Get-Item -LiteralPath $installer | Select-Object FullName, Length, LastWriteTime
