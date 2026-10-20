@echo off
rem UNPAID: OVERTIME - regenerates the environment art and audio (textures, materials, meshes, audio, Lvl_Office) headless.
rem Safe to run with the editor open as long as /Game/UO/Maps/Lvl_Office is NOT open in it
rem (only packages under /Game/UO/ are written). Re-running overwrites everything cleanly.
rem
rem Usage:
rem   Scripts\BuildArt.bat                 full rebuild (textures, materials, meshes, audio, level)
rem   Scripts\BuildArt.bat meshes,level    only some stages (textures,materials,meshes,audio,level)
rem   Scripts\BuildArt.bat audio           synthesize + import every sound and place the level audio (see Docs\Audio.md)
rem   Scripts\BuildArt.bat shots           render verification screenshots to Saved\Screenshots\ArtPass
rem Log: Saved\Logs\UOArt.log

setlocal
set "PROJECT_DIR=%~dp0.."
set "UPROJECT=%PROJECT_DIR%\UnpaidOvertime.uproject"
set "UE_BIN=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64"

if not exist "%UE_BIN%\UnrealEditor-Cmd.exe" (
	echo Unreal Engine 5.8 not found at "%UE_BIN%". Edit this script to point at your install.
	exit /b 1
)

if /I "%~1"=="shots" (
	"%UE_BIN%\UnrealEditor.exe" "%UPROJECT%" /Game/UO/Maps/Lvl_Office -ExecutePythonScript="%PROJECT_DIR%\Scripts\Editor\uo_screenshots.py" -unattended -nosplash -windowed -ResX=1700 -ResY=1000
	exit /b %ERRORLEVEL%
)

rem %* keeps "meshes,level" whole (cmd splits %1 at commas); build_all.py splits on commas and spaces
set "UO_ART_STAGES=%*"
"%UE_BIN%\UnrealEditor-Cmd.exe" "%UPROJECT%" -run=pythonscript -script="%PROJECT_DIR%\Scripts\Editor\build_all.py" -unattended -nosplash -nullrhi -log
set "RC=%ERRORLEVEL%"
echo.
echo Art build finished (exit %RC%). See Saved\Logs\UOArt.log
endlocal & exit /b %RC%
