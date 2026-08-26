param(
    [string]$DecoderSource = "$PSScriptRoot\..\payload\decoder",
    [string]$ProxySource = "$PSScriptRoot\..\payload\proxy",
    [string]$SdrSharpSource = "$PSScriptRoot\..\payload\sdrsharp",
    [string]$SdrSharpApiRoot = "$PSScriptRoot\..\payload\sdrsharp-api",
    [string]$IqBridgeProject = "$PSScriptRoot\..\..\plugins\sdrsharp-bridge",
    [string]$InnoSetup = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    [switch]$RebuildDsp,
    [string]$RuntimeRoot = 'C:\ProgramData\radioconda'
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$stage = Join-Path $PSScriptRoot 'stage'
$decoderStage = Join-Path $stage 'decoder'
$proxyStage = Join-Path $stage 'proxy'
$sdrSharpStage = Join-Path $stage 'sdrsharp'
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
New-Item -ItemType Directory -Path $decoderStage, $proxyStage, $sdrSharpStage -Force | Out-Null

Copy-Item -Path (Join-Path $DecoderSource '*') -Destination $decoderStage -Recurse -Force
Copy-Item -Path (Join-Path $ProxySource '*') -Destination $proxyStage -Recurse -Force
Copy-Item -Path (Join-Path $SdrSharpSource '*') -Destination $sdrSharpStage -Recurse -Force

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

& $InnoSetup (Join-Path $PSScriptRoot 'ASRTU1_Receiver_Setup.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }

$installer = Join-Path $PSScriptRoot 'dist\ASRTU_Series_Receiver_Setup.exe'
Write-Host "Installer: $installer"
Get-Item -LiteralPath $installer | Select-Object FullName, Length, LastWriteTime
