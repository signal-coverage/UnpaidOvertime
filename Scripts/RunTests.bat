@echo off
rem UNPAID: OVERTIME - runs every UnpaidOvertime.* automation test headless. Close the editor first.

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "EDITOR_CMD=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%EDITOR_CMD%" (
	echo UnrealEditor-Cmd not found at "%EDITOR_CMD%". Edit this script to point at your UE 5.8 install.
	exit /b 1
)

"%EDITOR_CMD%" "%UPROJECT%" -ExecCmds="Automation RunTests UnpaidOvertime; Quit" -unattended -nullrhi -nosplash -log -testexit="Automation Test Queue Empty"
set "RESULT=%ERRORLEVEL%"
echo Automation finished with exit code %RESULT%. See Saved\Logs\UnpaidOvertime.log for details.
endlocal & exit /b %RESULT%
