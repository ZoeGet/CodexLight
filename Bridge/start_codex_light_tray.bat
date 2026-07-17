@echo off
setlocal

rem User configuration.
set "SERIAL_PORT=auto"
set "SERIAL_BAUD=115200"
set "UDP_PORT=4210"

rem Start modes:
rem   double-click or auto: serial + UDP, with wired preferred by firmware
rem   wired:               serial only
rem   wireless:            UDP states; serial is released after MODE WIRELESS setup
set "CONNECTION_MODE=%~1"
if not defined CONNECTION_MODE set "CONNECTION_MODE=auto"

if /I "%CONNECTION_MODE%"=="auto" (
  set "DISPLAY_MODE=AUTO"
  goto launch
)

if /I "%CONNECTION_MODE%"=="wired" (
  set "DISPLAY_MODE=WIRED"
  goto launch
)

if /I "%CONNECTION_MODE%"=="wireless" (
  set "DISPLAY_MODE=WIRELESS"
  goto launch
)

echo Invalid connection mode: %CONNECTION_MODE%
echo Usage: %~nx0 [auto^|wired^|wireless]
exit /b 2

:launch
start "CodexLight Bridge" powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0CodexLightTray.ps1" -ConnectionMode "%DISPLAY_MODE%" -SerialPort "%SERIAL_PORT%" -SerialBaud %SERIAL_BAUD% -UdpPort %UDP_PORT%
