@echo off

mkdir tmp

echo Downloading RTLSDR Driver
httpget http://github.com/rtlsdrblog/rtl-sdr-blog/releases/latest/download/Release.zip tmp\Release.zip

echo Downloading Zadig
set zadig_url=b721/zadig-2.4.exe
ver | findstr /l "5.1." > NUL
if %errorlevel% equ 0 set zadig_url=v1.2.5/zadig_xp-2.2.exe
httpget http://github.com/pbatard/libwdi/releases/download/%zadig_url% zadig.exe

unzip -o tmp\Release.zip -d tmp
move tmp\x86\rtlsdr.dll .

rmdir tmp /S /Q

pause