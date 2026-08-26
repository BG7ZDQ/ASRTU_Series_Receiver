param(
    [string]$ProxySource = "$PSScriptRoot\..\vendor\proxy_mmt_gui",
    [string]$SdrSharpPresetSource = "$PSScriptRoot\..\vendor\SDRSharp_TLM",
    [string]$DecoderProject = "$PSScriptRoot\..\asrtu-qt",
    [string]$SdrSharpApiRoot = "$PSScriptRoot\..\vendor\SDRSharp_TLM",
    [string]$IqBridgeProject = "$PSScriptRoot\..\sdrsharp-iq-bridge"
)

$ErrorActionPreference = 'Stop'
$stage = Join-Path $PSScriptRoot 'stage'
$decoderStage = Join-Path $stage 'decoder'
$proxyStage = Join-Path $stage 'proxy'
$sdrSharpStage = Join-Path $stage 'sdrsharp'
$terminfoSource = 'C:\Program Files\Git\usr\share\terminfo\78\xterm'
$decoderPortable = Join-Path $DecoderProject 'portable\ASRTU1_Demod_CQt'
$localHyacinthRuntime = Join-Path $PSScriptRoot '..\work\gr-hyacinthsat-stage-msvc\bin\gnuradio-hyacinthsat.dll'
$iqBridgeBuild = Join-Path $IqBridgeProject 'build_legacy.ps1'
$iqBridgePlugin = Join-Path $IqBridgeProject 'bin\Release\net46\SDRSharp.AstroSeriesBridge.dll'
$proxyWrapper = Join-Path $DecoderProject 'build-vs-clean\Release\ASRTU_Proxy.exe'
$iscc = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'

$reuseStagedProxy = -not (Test-Path -LiteralPath $ProxySource)
$reuseStagedSdrSharp = -not (Test-Path -LiteralPath (Join-Path $SdrSharpPresetSource 'SDRSharp.exe'))
if ($reuseStagedProxy -and
    -not (Test-Path -LiteralPath (Join-Path $proxyStage 'proxy_mmt_gui.exe'))) {
    throw "Proxy folder not found and no staged proxy is available: $ProxySource"
}
if ($reuseStagedSdrSharp -and
    -not (Test-Path -LiteralPath (Join-Path $sdrSharpStage 'SDRSharp.exe'))) {
    throw "SDRSharp preset host not found and no staged copy is available: $SdrSharpPresetSource"
}
if (-not (Test-Path -LiteralPath (Join-Path $SdrSharpApiRoot 'SDRSharp.exe'))) {
    if (Test-Path -LiteralPath (Join-Path $sdrSharpStage 'SDRSharp.exe')) {
        $SdrSharpApiRoot = $sdrSharpStage
    } else {
        throw "SDRSharp API root not found: $SdrSharpApiRoot"
    }
}
if (-not (Test-Path -LiteralPath $iqBridgeBuild)) { throw "I/Q bridge build script not found: $IqBridgeProject" }
if (-not (Test-Path -LiteralPath $terminfoSource)) { throw "xterm terminfo not found: $terminfoSource" }
if (-not (Test-Path -LiteralPath $iscc)) { throw "Inno Setup compiler not found: $iscc" }

& $iqBridgeBuild -SdrSharpApiRoot $SdrSharpApiRoot -Configuration Release
if ($LASTEXITCODE -ne 0) { throw 'SDRSharp I/Q bridge plugin build failed.' }
if (-not (Test-Path -LiteralPath $iqBridgePlugin)) { throw "I/Q bridge DLL not found: $iqBridgePlugin" }

& (Join-Path $DecoderProject 'build_release.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Decoder/launcher build failed.' }
if (Test-Path -LiteralPath $localHyacinthRuntime) {
    Copy-Item -LiteralPath $localHyacinthRuntime `
        -Destination (Join-Path $decoderPortable 'gnuradio-hyacinthsat.dll') -Force
    Write-Host 'Using locally built hyacinthsat runtime with stoppable audio input.'
}

$expectedRoot = [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\') + '\'
$resolvedStage = [IO.Path]::GetFullPath($stage)
if (-not $resolvedStage.StartsWith($expectedRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe stage path: $resolvedStage"
}
if ($reuseStagedProxy) {
    Write-Warning "Proxy source folder is missing; reusing the verified staged proxy bundle."
}
if ($reuseStagedSdrSharp) {
    Write-Warning "SDRSharp source folder is missing; reusing the verified staged SDRSharp bundle."
}
$replaceableStages = @($decoderStage)
if (-not $reuseStagedSdrSharp) { $replaceableStages += $sdrSharpStage }
foreach ($replaceableStage in $replaceableStages) {
    if (Test-Path -LiteralPath $replaceableStage) {
        Remove-Item -LiteralPath $replaceableStage -Recurse -Force
    }
}
if (-not $reuseStagedProxy -and (Test-Path -LiteralPath $proxyStage)) {
    Remove-Item -LiteralPath $proxyStage -Recurse -Force
}
New-Item -ItemType Directory -Path $decoderStage, $proxyStage, $sdrSharpStage -Force | Out-Null
Copy-Item -Path (Join-Path $decoderPortable '*') -Destination $decoderStage -Recurse -Force
if (-not $reuseStagedProxy) {
    Copy-Item -Path (Join-Path $ProxySource '*') -Destination $proxyStage -Recurse -Force
}
if (-not (Test-Path -LiteralPath $proxyWrapper)) {
    throw "Proxy launcher with application icon not found: $proxyWrapper"
}
Copy-Item -LiteralPath $proxyWrapper -Destination (Join-Path $proxyStage 'ASRTU_Proxy.exe') -Force
if (-not $reuseStagedSdrSharp) {
    Copy-Item -Path (Join-Path $SdrSharpPresetSource '*') -Destination $sdrSharpStage -Recurse -Force
}
$sdrSharpPlugins = Join-Path $sdrSharpStage 'Plugins'
New-Item -ItemType Directory -Path $sdrSharpPlugins -Force | Out-Null
foreach ($legacyDdePlugin in @('NDde.dll', 'SDRSharp.DDETracker.dll')) {
    $legacyDdePath = Join-Path $sdrSharpPlugins $legacyDdePlugin
    if (Test-Path -LiteralPath $legacyDdePath) {
        Remove-Item -LiteralPath $legacyDdePath -Force
    }
}
Copy-Item -LiteralPath $iqBridgePlugin -Destination (Join-Path $sdrSharpPlugins 'SDRSharp.AstroSeriesBridge.dll') -Force
Copy-Item -LiteralPath (Join-Path $IqBridgeProject 'README.md') -Destination (Join-Path $sdrSharpStage 'ASRTU_IQ_BRIDGE_README.md') -Force
$sdrSharpLayout = Join-Path $sdrSharpStage 'SDRSharp.Layout.xml'
if (Test-Path -LiteralPath $sdrSharpLayout) {
    $layoutText = [IO.File]::ReadAllText($sdrSharpLayout)
    $layoutText = $layoutText.Replace(
        'SDRSharp.DDETracker.DdeTrackingPlugin,Plugins\SDRSharp.DDETracker.dll',
        'SDRSharp.AstroSeriesBridge.AstroSeriesBridgePlugin,Plugins\SDRSharp.AstroSeriesBridge.dll')
    [IO.File]::WriteAllText($sdrSharpLayout, $layoutText, [Text.Encoding]::Unicode)
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
$terminfoStage = Join-Path $proxyStage 'terminfo\78'
New-Item -ItemType Directory -Path $terminfoStage -Force | Out-Null
Copy-Item -LiteralPath $terminfoSource -Destination (Join-Path $terminfoStage 'xterm') -Force
$privateConfig = Join-Path $proxyStage 'config.cfg'
if (Test-Path -LiteralPath $privateConfig) { Remove-Item -LiteralPath $privateConfig -Force }

& $iscc (Join-Path $PSScriptRoot 'ASRTU1_Receiver_Setup.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }

$installer = Join-Path $PSScriptRoot 'dist\ASRTU_Series_Receiver_Setup.exe'
Write-Host "Installer: $installer"
Get-Item -LiteralPath $installer | Select-Object FullName, Length, LastWriteTime
