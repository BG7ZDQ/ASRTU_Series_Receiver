param(
    [string]$SdrSharpApiRoot = "$PSScriptRoot\..\..\packaging\payload\sdrsharp-api",
    [string]$Configuration = 'Release',
    [string]$CscPath = ''
)

$ErrorActionPreference = 'Stop'
$csc = $CscPath
if (-not $csc) {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $csc = (& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\Current\Bin\Roslyn\csc.exe' | Select-Object -First 1)
    }
}
if (-not $csc) {
    foreach ($edition in @('BuildTools', 'Community', 'Professional', 'Enterprise')) {
        $candidate = "C:\Program Files (x86)\Microsoft Visual Studio\2022\$edition\MSBuild\Current\Bin\Roslyn\csc.exe"
        if (Test-Path -LiteralPath $candidate) {
            $csc = $candidate
            break
        }
    }
}
if (-not $csc) {
    throw 'Roslyn csc.exe was not found in Visual Studio 2022. Install the MSBuild component or pass -CscPath.'
}
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
