@echo off
setlocal enabledelayedexpansion

rem Run from the repository root so all paths below stay project-relative.
cd /d "%~dp0..\.."

rem Paths
set EXE=build\Release\Tokri.exe
set OUTDIR=dist\Windows

rem Deploy Qt
windeployqt --dir "%OUTDIR%" "%EXE%"

rem Copy exe to release (overwrite)
copy /Y "%EXE%" "%OUTDIR%\Tokri.exe"

endlocal
