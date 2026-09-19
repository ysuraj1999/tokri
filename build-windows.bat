@echo off
setlocal enabledelayedexpansion

rem Paths
set EXE=build\Release\Tokri.exe
set OUTDIR=dist\Windows

rem Deploy Qt
windeployqt --dir "%OUTDIR%" "%EXE%"

rem Copy exe to release (overwrite)
copy /Y "%EXE%" "%OUTDIR%\Tokri.exe"

endlocal
