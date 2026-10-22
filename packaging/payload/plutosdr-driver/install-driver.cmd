@echo off
setlocal
if /I "%~1"=="--elevated" goto run

set "PLUTO_DRIVER_SCRIPT=%~f0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath $env:PLUTO_DRIVER_SCRIPT -ArgumentList '--elevated' -Verb RunAs"
exit /b

:run
pushd "%~dp0"
"%~dp0dpinst.exe" /S /F /PATH "%~dp0"
set "RESULT=%ERRORLEVEL%"
popd

if "%RESULT%"=="0" (
    echo PlutoSDR driver installation completed.
    echo PlutoSDR 驱动安装完成。请拔插设备后重新启动 SDR++。
) else (
    echo PlutoSDR driver installation returned code %RESULT%.
    echo PlutoSDR 驱动安装返回代码 %RESULT%。请重新连接设备后再试。
)
echo.
pause
exit /b %RESULT%
