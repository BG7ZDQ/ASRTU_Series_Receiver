param(
    [string]$SdrSharpApiRoot = "$PSScriptRoot\..\vendor\SDRSharp_TLM",
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$csc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe'
$framework = 'C:\Windows\Microsoft.NET\Framework\v4.0.30319'
$outputDir = Join-Path $PSScriptRoot "bin\$Configuration\net46"
$output = Join-Path $outputDir 'SDRSharp.AstroSeriesBridge.dll'

foreach ($required in @(
    $csc,
    (Join-Path $framework 'mscorlib.dll'),
    (Join-Path $SdrSharpApiRoot 'SDRSharp.Common.dll'),
    (Join-Path $SdrSharpApiRoot 'SDRSharp.Radio.dll'),
    (Join-Path $SdrSharpApiRoot 'SDRSharp.PanView.dll')
)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Build dependency not found: $required" }
}

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
& $csc /nologo /target:library /platform:x86 /unsafe /optimize+ /nostdlib+ `
    "/out:$output" `
    "/reference:$framework\mscorlib.dll" `
    "/reference:$framework\System.dll" `
    "/reference:$framework\System.Core.dll" `
    "/reference:$framework\System.Drawing.dll" `
    "/reference:$framework\System.Windows.Forms.dll" `
    "/reference:$SdrSharpApiRoot\SDRSharp.Common.dll" `
    "/reference:$SdrSharpApiRoot\SDRSharp.Radio.dll" `
    "/reference:$SdrSharpApiRoot\SDRSharp.PanView.dll" `
    (Join-Path $PSScriptRoot 'ASRTUIQBridgePlugin.cs') `
    (Join-Path $PSScriptRoot 'IqBridgeProcessor.cs') `
    (Join-Path $PSScriptRoot 'BridgeControlPanel.cs') `
    (Join-Path $PSScriptRoot 'DopplerControlReader.cs')
if ($LASTEXITCODE -ne 0) { throw 'Legacy SDRSharp bridge compilation failed.' }
Get-Item -LiteralPath $output
