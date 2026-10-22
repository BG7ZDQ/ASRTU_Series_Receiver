param(
    [Parameter(Mandatory=$true)][string]$SdrppRoot,
    [string]$PluginPath = '',
    [string]$PlutoUri = ''
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath (Join-Path $SdrppRoot 'sdrpp.exe'))) {
    throw "SDR++ installation not found: $SdrppRoot"
}
$pluginDestination = Join-Path $SdrppRoot 'modules/asrtu_sink.dll'
if ($PluginPath) {
    Copy-Item -LiteralPath $PluginPath -Destination $pluginDestination -Force
}
if (-not (Test-Path -LiteralPath $pluginDestination)) {
    throw 'Build and install the matching asrtu_sink.dll before configuring the shared-memory preset.'
}
function Set-Field($Object, [string]$Name, $Value) {
    $Object | Add-Member -MemberType NoteProperty -Name $Name -Value $Value -Force
}
function Ensure-Object($Object, [string]$Name) {
    if ($null -eq $Object.$Name) { Set-Field $Object $Name ([pscustomobject]@{}) }
    return $Object.$Name
}
function Read-Config([string]$Name) {
    $path = Join-Path $SdrppRoot $Name
    if (Test-Path -LiteralPath $path) { return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json }
    return [pscustomobject]@{}
}
function Write-Config([string]$Name, $Config) {
    $json = $Config | ConvertTo-Json -Depth 40
    [IO.File]::WriteAllText((Join-Path $SdrppRoot $Name), $json, [Text.UTF8Encoding]::new($false))
}
$config = Read-Config 'config.json'
$instances = Ensure-Object $config 'moduleInstances'
if ($null -eq $instances.Radio) {
    Set-Field $instances 'Radio' ([pscustomobject]@{ enabled = $true; module = 'radio' })
}
Set-Field $instances 'ASRTU Bridge' ([pscustomobject]@{ enabled = $true; module = 'asrtu_sink' })
Set-Field $instances 'BladeRF Source' ([pscustomobject]@{ enabled = $true; module = 'bladerf_source' })
Set-Field $instances 'SDRplay Source' ([pscustomobject]@{ enabled = $true; module = 'sdrplay_source' })
Set-Field $instances 'SoapySDR Source' ([pscustomobject]@{ enabled = $true; module = 'soapy_source' })
if ($null -ne $instances.'Rigctl Server') { Set-Field $instances.'Rigctl Server' 'enabled' $false }
if ($PlutoUri) {
    Set-Field $instances 'PlutoSDR Source' ([pscustomobject]@{ enabled = $true; module = 'plutosdr_source' })
}
$stream = Ensure-Object (Ensure-Object $config 'streams') 'Radio'
# SDR++ reads these two keys unconditionally; a missing JSON key becomes null
# and aborts sink initialization during startup.
if ($null -eq $stream.volume) { Set-Field $stream 'volume' 1.0 }
if ($null -eq $stream.muted) { Set-Field $stream 'muted' $false }
Set-Field $stream 'sink' 'ASRTU Receiver'
# Receiver preset: keep the waterfall useful for weak telemetry signals.
Set-Field $config 'min' ([double]-150.0)
Set-Field $config 'max' ([double]-65.0)
Write-Config 'config.json' $config
if ($PlutoUri) {
    $plutoConfig = Read-Config 'plutosdr_source_config.json'
    Set-Field $plutoConfig 'device' $PlutoUri
    Set-Field $plutoConfig 'manualUri' $PlutoUri
    Ensure-Object $plutoConfig 'devices' | Out-Null
    Write-Config 'plutosdr_source_config.json' $plutoConfig
}
$radioConfig = Read-Config 'radio_config.json'
$radio = Ensure-Object $radioConfig 'Radio'
Set-Field $radio 'selectedDemodId' 7 # RAW in SDR++ 1.3.0
Set-Field (Ensure-Object $radio 'RAW') 'bandwidth' 48000.0
Write-Config 'radio_config.json' $radioConfig
$bridgeConfig = Read-Config 'asrtu_sink_config.json'
$bridge = Ensure-Object $bridgeConfig 'Radio'
if ($null -eq $bridge.autoDoppler) { Set-Field $bridge 'autoDoppler' $true }
Write-Config 'asrtu_sink_config.json' $bridgeConfig
$rigctl = Read-Config 'rigctl_server_config.json'
if ($null -ne $rigctl.'Rigctl Server') {
    Set-Field $rigctl.'Rigctl Server' 'autoStart' $false
    Write-Config 'rigctl_server_config.json' $rigctl
}
[IO.File]::WriteAllText((Join-Path $SdrppRoot 'asrtu-preset.version'), '3',
    [Text.ASCIIEncoding]::new())
Write-Host "Shared-memory preset configured: $SdrppRoot"
