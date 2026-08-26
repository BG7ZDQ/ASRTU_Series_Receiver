$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\packaging\inno\build_installer.ps1" @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
