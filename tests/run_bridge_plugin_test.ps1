param(
    [Parameter(Mandatory=$true)][string]$SdrSharpApiRoot,
    [Parameter(Mandatory=$true)][string]$NativeBridgeTest,
    [string]$OutputDirectory = 'work/bridge-managed-test'
)
$ErrorActionPreference = 'Stop'
$vswhere = 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$csc = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/Current/Bin/Roslyn/csc.exe' | Select-Object -First 1
if (-not $csc) { throw 'Roslyn compiler not found' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$output = Join-Path $OutputDirectory 'BridgePluginTest.exe'
$framework = 'C:/Windows/Microsoft.NET/Framework/v4.0.30319'
$plugin = Join-Path $PSScriptRoot '../external/TinyDoppler/plugin'
& $csc /nologo /target:exe /platform:x86 /unsafe /optimize+ /nostdlib+ "/out:$output" `
    "/reference:$framework/mscorlib.dll" "/reference:$framework/System.dll" "/reference:$framework/System.Core.dll" `
    "/reference:$SdrSharpApiRoot/SDRSharp.Radio.dll" `
    (Join-Path $plugin 'IqBridgeProcessor.cs') (Join-Path $PSScriptRoot 'bridge_plugin_test.cs')
if ($LASTEXITCODE -ne 0) { throw 'Bridge plugin test compilation failed' }
Copy-Item -LiteralPath (Join-Path $SdrSharpApiRoot 'SDRSharp.Radio.dll') -Destination $OutputDirectory -Force
& $output (Resolve-Path -LiteralPath $NativeBridgeTest).Path
if ($LASTEXITCODE -ne 0) { throw "Bridge plugin test exited with $LASTEXITCODE" }
