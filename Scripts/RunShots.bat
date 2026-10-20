@echo off
rem UNPAID: OVERTIME - automated in-game verification: plays a scripted day on Lvl_Office and saves screenshots
rem (main menu, settings, setup, gameplay, interaction, carried item, pause, overtime, evaluation), then quits.
rem Output: Saved\Screenshots\UOAuto\*.png    Log: Saved\Logs\UnpaidOvertime.log (search "UOShot" / "Dev automation" / "UOAudio")

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%EDITOR%" (
	echo Unreal Editor not found at "%EDITOR%". Edit this script to point at your UE 5.8 install.
	exit /b 1
)

"%EDITOR%" "%UPROJECT%" /Game/UO/Maps/Lvl_Office -game -windowed -ResX=1600 -ResY=900 -nosplash -log -UOShots -UOTasks=8 -ExecCmds="uo.ShiftSecondsOverride 70, uo.AudioDebug 1" %*
endlocal & exit /b %ERRORLEVEL%
