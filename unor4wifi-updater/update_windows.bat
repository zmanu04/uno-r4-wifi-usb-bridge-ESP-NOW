@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
setlocal

REM go to the folder where this bat script is located
cd /d %~dp0

CALL bin\unor4wifi-reboot

IF %ERRORLEVEL% NEQ 0 (
    GOTO ESPMODEERROR
)

echo Start flashing firmware
timeout /t 5 /nobreak > NUL
CALL bin\espflash write-bin -b 115200 0x0 ..\UNOR4USBBridge\build\esp32-patched.esp32.arduino_unor4wifi_usb_bridge\S3-ALL.bin

@pause
exit /b 0

:ESPMODEERROR
echo Cannot put the board in ESP mode. (via 'unor4wifi-reboot')
@pause
exit /b 1