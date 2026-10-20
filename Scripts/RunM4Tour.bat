@echo off
rem UNPAID: OVERTIME - automated verification of the killers, the investigation and the meeting (M4.1) on Lvl_Office.
rem   employee   : AI killer sabotages; the scripted human clears a jam, reads every terminal, pins, presents the pins
rem                at the meeting and votes (first for someone else, then reassigns)
rem   killer     : the scripted human is a saboteur: all 5 sabotages, real work, private fake rating ("-UOFakeStars=3")
rem   kill       : AI killer with dismissals on (never the human): emergency meeting, body lying flat + letter
rem   fakeone    : an AI killer shows 1 star; its wrong defense answer is accepted in public
rem   forgery    : an AI killer shows 3 stars (forged shield) and is voted out: "The shield was a forgery."
rem   duelrigged : the human and an AI killer tie: rigged rock-paper-scissors, saving throw, probation
rem   duelfair   : two AI employees tie: a fair duel, then the loser's saving throw
rem   takeover   : the human is a saboteur; the meetings vote the employees out until only saboteurs are left
rem Usage: Scripts\RunM4Tour.bat [tour] [days] ["-UOEmployees=6" "-UOKillers=2" "-UOFakeStars=3" "-UOMeetingPolicy=..."]
rem        (defaults: employee 1; quote extra switches, cmd splits at "=")
rem Output: Saved\Screenshots\UOAuto\M4\*.png    Log: Saved\Logs\UnpaidOvertime.log
rem   (grep "Roles:", "Sabotage:", "Fake rating", "ALWAYS ACCEPTED", "AI evidence", "AI statement", "AI vote", "Meeting",
rem    "Duel", "Saving throw", "Kill:", "Dismissal:", "Match over")

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "EDITOR=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "TOUR=%~1"
set "DAYS=%~2"
if "%TOUR%"=="" set "TOUR=employee"
if "%DAYS%"=="" set "DAYS=1"

if not exist "%EDITOR%" (
	echo Unreal Editor not found at "%EDITOR%". Edit this script to point at your UE 5.8 install.
	exit /b 1
)

"%EDITOR%" "%UPROJECT%" /Game/UO/Maps/Lvl_Office -game -windowed -ResX=1600 -ResY=900 -nosplash -log -UOM4=%TOUR% -UOAutoDays=%DAYS% -UOTasks=8 %~3 %~4 %~5 %~6 -ExecCmds="uo.ShiftSecondsOverride 200"
endlocal & exit /b %ERRORLEVEL%
