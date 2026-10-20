@echo off
rem UNPAID: OVERTIME - automated multi-day verification of the end-of-day cycle on Lvl_Office.
rem A scripted human plays several days (works the shift, answers the boss, votes) and screenshots every
rem evaluation screen: review, firings, promotion/defense offers, boss questions, AI answers, cut vote,
rem outcome, the next day's HUD and the game over / victory screen. Then it quits.
rem
rem Usage: Scripts\RunEvalTour.bat [policy] [days]      policy = random (default) | win | lose, days default 4
rem Extra arguments after those are passed to the game. Quote each one (cmd splits at "="), e.g.
rem   Scripts\RunEvalTour.bat win 8 "-UOAIDifficulty=Easy" "-UOTasks=4" "-UOAutoSeed=7"
rem (the first -UOTasks on the command line wins, so an extra one overrides the default 6).
rem Output: Saved\Screenshots\UOAuto\Eval\*.png    Log: Saved\Logs\UnpaidOvertime.log
rem   (grep "Starting Day", "Difficulty day", "Boss question", "Boss answer", "Direct firing", "Cut vote", "Match over")

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "POLICY=%~1"
set "DAYS=%~2"
if "%POLICY%"=="" set "POLICY=random"
if "%DAYS%"=="" set "DAYS=4"

if not exist "%EDITOR%" (
	echo Unreal Editor not found at "%EDITOR%". Edit this script to point at your UE 5.8 install.
	exit /b 1
)

"%EDITOR%" "%UPROJECT%" /Game/UO/Maps/Lvl_Office -game -windowed -ResX=1600 -ResY=900 -nosplash -log -UOEvalTour -UOAutoPolicy=%POLICY% -UOAutoDays=%DAYS% %~3 %~4 %~5 %~6 -UOTasks=6 -ExecCmds="uo.ShiftSecondsOverride 45"
endlocal & exit /b %ERRORLEVEL%
