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
$decoderStage = Join-Path $stage 'decoder'
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
New-Item -ItemType Directory -Path $decoderStage, $proxyStage, $sdrSharpStage,
    $sdrppStage, $plutoDriverStage -Force | Out-Null

Copy-Item -Path (Join-Path $DecoderSource '*') -Destination $decoderStage -Recurse -Force
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

# Split the Qt applications, copying only their transitive runtime dependencies.
$objdump = Get-Command 'objdump.exe' -ErrorAction SilentlyContinue
$dumpbin = Get-Command 'dumpbin.exe' -ErrorAction SilentlyContinue
if (-not $objdump -and -not $dumpbin) {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $dumpbinPath = & $vswhere -latest -products '*' -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe' |
            Select-Object -First 1
        if ($dumpbinPath) { $dumpbin = Get-Command $dumpbinPath }
    }
}
if (-not $objdump -and -not $dumpbin) {
    throw 'objdump.exe or Visual Studio dumpbin.exe is required to package module dependencies.'
}

function Get-ModuleDependencies([string]$Binary) {
    if ($objdump) {
        $output = & $objdump.Source -p $Binary 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect dependencies: $Binary" }
        $output | Select-String 'DLL Name:\s*(.+)$' |
            ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
    } else {
        $output = & $dumpbin.Source /DEPENDENTS $Binary 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect dependencies: $Binary" }
        $output | Select-String '^\s+([A-Za-z0-9_.-]+\.dll)\s*$' |
            ForEach-Object { $_.Matches[0].Groups[1].Value }
    }
}

foreach ($module in @(
    @{ Directory = 'launcher'; Executable = 'ASRTU1_Launcher.exe' },
    @{ Directory = 'doppler'; Executable = 'TinyDoppler.exe' },
    @{ Directory = 'satnogs-uploader'; Executable = 'ASRTU_SatnogsUploader.exe' }
)) {
    $moduleStage = Join-Path $stage $module.Directory
    New-Item -ItemType Directory -Path $moduleStage -Force | Out-Null
    $moduleExe = Join-Path $moduleStage $module.Executable
    Copy-Item -LiteralPath (Join-Path $DecoderSource $module.Executable) -Destination $moduleExe
    foreach ($resource in @('platforms', 'imageformats', 'translations', 'qt.conf')) {
        $resourceSource = Join-Path $DecoderSource $resource
        if (Test-Path -LiteralPath $resourceSource) {
            Copy-Item -LiteralPath $resourceSource -Destination $moduleStage -Recurse -Force
        }
    }
    $queue = [Collections.Generic.Queue[string]]::new()
    Get-ChildItem -LiteralPath $moduleStage -File -Recurse |
        Where-Object { $_.Extension -in @('.exe', '.dll') } |
        ForEach-Object { $queue.Enqueue($_.FullName) }
    $processed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while ($queue.Count -gt 0) {
        $binary = $queue.Dequeue()
        if (-not $processed.Add($binary)) { continue }
        foreach ($dependency in (Get-ModuleDependencies $binary)) {
            $source = Join-Path $DecoderSource $dependency
            $destination = Join-Path $moduleStage $dependency
            if (Test-Path -LiteralPath $source) {
                if (-not (Test-Path -LiteralPath $destination)) {
                    Copy-Item -LiteralPath $source -Destination $destination
                }
                $queue.Enqueue($destination)
            } elseif ($dependency -notlike 'api-ms-win-*' -and
                      -not (Test-Path -LiteralPath (Join-Path ([Environment]::SystemDirectory) $dependency))) {
                throw "Missing dependency for $($module.Directory): $dependency"
            }
        }
    }
    Remove-Item -LiteralPath (Join-Path $decoderStage $module.Executable)
}

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

# Keep one physical copy of DLLs shared by the Qt modules. The launcher keeps
# its own startup copy; child modules receive common\ through PATH.
$runtimeModules = @($decoderStage, $proxyStage, (Join-Path $stage 'doppler'),
    (Join-Path $stage 'satnogs-uploader'))
$filesByHash = @{}
$staleFileEntries = [Collections.Generic.List[string]]::new()
$staleFileEntries.Add('; Generated by build_installer.ps1. Do not edit.')
foreach ($moduleRoot in $runtimeModules) {
    foreach ($file in (Get-ChildItem -LiteralPath $moduleRoot -File -Recurse)) {
        if ($file.DirectoryName -ne $moduleRoot -or $file.Extension -ne '.dll') { continue }
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        if (-not $filesByHash.ContainsKey($hash)) { $filesByHash[$hash] = @() }
        $filesByHash[$hash] += $file
    }
}
foreach ($sameFiles in $filesByHash.Values) {
    if ($sameFiles.Count -lt 2) { continue }
    $canonical = $sameFiles[0]
$commonFile = Join-Path (Join-Path $stage 'launcher') $canonical.Name
    Copy-Item -LiteralPath $canonical.FullName -Destination $commonFile -Force
    foreach ($file in $sameFiles) {
        $relativePath = Get-RelativePath $stage $file.FullName
        $staleFileEntries.Add(
            "Type: files; Name: `"{app}\$relativePath`"")
        Remove-Item -LiteralPath $file.FullName -Force
    }
}
[IO.File]::WriteAllLines((Join-Path $stage 'core-stale.iss'), $staleFileEntries,
    [Text.UTF8Encoding]::new($false))

# Inno Setup merges entries with the same source path, not identical content
# at different paths. Reuse one source per hash while preserving destinations.
$coreFileEntries = [Collections.Generic.List[string]]::new()
$coreFileEntries.Add('; Generated by build_installer.ps1. Do not edit.')
$hardLinkEntries = [Collections.Generic.List[string]]::new()
$sourceByHash = @{}
$duplicateBytes = 0L
$coreModuleNames = @('decoder', 'launcher', 'doppler', 'satnogs-uploader', 'proxy')
foreach ($moduleName in @('decoder', 'launcher', 'doppler', 'satnogs-uploader', 'proxy')) {
    $moduleRoot = Join-Path $stage $moduleName
    foreach ($file in (Get-ChildItem -LiteralPath $moduleRoot -File -Recurse | Sort-Object FullName)) {
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $relativePath = Get-RelativePath $stage $file.FullName
        if ($sourceByHash.ContainsKey($hash)) {
            $sourcePath = $sourceByHash[$hash]
            $duplicateBytes += $file.Length
            $sourceRelativePath = $sourcePath.Substring(6)
            $hardLinkEntries.Add(
                "  CollapseHardLink(ExpandConstant('{app}\$relativePath'), ExpandConstant('{app}\$sourceRelativePath'));" )
        } else {
            $sourcePath = "stage\$relativePath"
            $sourceByHash[$hash] = $sourcePath
        }
        if ($coreModuleNames -contains $moduleName) {
            $destinationDirectory = [IO.Path]::GetDirectoryName($relativePath)
            $coreFileEntries.Add(
                "Source: `"$sourcePath`"; DestDir: `"{app}\$destinationDirectory`"; DestName: `"$($file.Name)`"; Components: core; Flags: ignoreversion")
        }
    }
}
[IO.File]::WriteAllLines((Join-Path $stage 'core-files.iss'), $coreFileEntries,
    [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllLines((Join-Path $stage 'core-hardlinks.iss'), $hardLinkEntries,
    [Text.UTF8Encoding]::new($false))
Write-Host ("Core payload: {0} unique files; {1:N1} MiB of duplicate data reused." -f
    $sourceByHash.Count, ($duplicateBytes / 1MB))

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
    "dist\ASRTU_Series_Receiver-${appVersion}-WinInstaller(Recommend)${BuildSuffix}.exe"
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
