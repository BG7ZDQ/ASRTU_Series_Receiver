param(
    [string]$RuntimeRoot = 'C:\ProgramData\radioconda',
    [string]$BuildDir = "$PSScriptRoot\build-vs-clean"
)

$ErrorActionPreference = 'Stop'
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw "CMake not found: $cmake" }

function Invoke-CMake([string[]]$Arguments) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $cmake
    $psi.WorkingDirectory = $PSScriptRoot
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $psi.ArgumentList.Add($argument) }

    # Some launchers inject both PATH and Path. MSBuild treats these as a
    # duplicate key; normalize only the child build environment.
    $sourceEnvironment = [Environment]::GetEnvironmentVariables()
    $pathValue = $sourceEnvironment['PATH']
    if (-not $pathValue) { $pathValue = $sourceEnvironment['Path'] }
    $psi.Environment.Clear()
    foreach ($environmentKey in $sourceEnvironment.Keys) {
        if ($environmentKey -ieq 'PATH') { continue }
        $psi.Environment[[string]$environmentKey] = [string]$sourceEnvironment[$environmentKey]
    }
    $psi.Environment['PATH'] = "$RuntimeRoot\Library\bin;$RuntimeRoot\Scripts;$pathValue"

    $process = [System.Diagnostics.Process]::Start($psi)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    Write-Host $stdout
    if ($stderr) { Write-Host $stderr }
    if ($process.ExitCode -ne 0) { throw "CMake failed with exit code $($process.ExitCode)" }
}

Invoke-CMake @(
    '-S', $PSScriptRoot,
    '-B', $BuildDir,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    "-DCMAKE_PREFIX_PATH=$RuntimeRoot\Library",
    "-DCMAKE_INCLUDE_PATH=$RuntimeRoot\Library\include",
    "-DCMAKE_LIBRARY_PATH=$RuntimeRoot\Library\lib"
)
Invoke-CMake @('--build', $BuildDir, '--config', 'Release', '--parallel', '1')

& "$PSScriptRoot\package_portable.ps1" -BuildDir "$BuildDir\Release" -RuntimeRoot $RuntimeRoot
