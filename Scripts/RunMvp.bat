@echo off
rem UNPAID: OVERTIME - launches the game on Lvl_Office (main menu -> setup -> day).
rem Tips: open the console (~) and type "uo.ShiftSecondsOverride 60" before clicking Create for a short shift.
rem Dev switches (non-Shipping): -UOAutoStart=Acme skips the menus; -UOShots runs the screenshot tour (RunShots.bat).

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%EDITOR%" (
	echo Unreal Editor not found at "%EDITOR%". Edit this script to point at your UE 5.8 install.
	exit /b 1
)

start "" "%EDITOR%" "%UPROJECT%" /Game/UO/Maps/Lvl_Office -game -windowed -ResX=1600 -ResY=900 -log %*
endlocal
