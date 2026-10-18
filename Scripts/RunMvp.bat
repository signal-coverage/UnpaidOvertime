@echo off
rem UNPAID: OVERTIME - launches the playable MVP on the engine's empty Entry map with UOGameMode.
rem No binary assets are required: the game mode builds the office, lights and sky at runtime.
rem Tip: open the console (~) and type "uo.ShiftSecondsOverride 30" before clicking Create for a short shift.

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%EDITOR%" (
	echo Unreal Editor not found at "%EDITOR%". Edit this script to point at your UE 5.8 install.
	exit /b 1
)

start "" "%EDITOR%" "%UPROJECT%" /Engine/Maps/Entry?game=/Script/UnpaidOvertime.UOGameMode -game -windowed -ResX=1600 -ResY=900 -log
endlocal
