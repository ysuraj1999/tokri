@echo off
setlocal enabledelayedexpansion

rem Check standard build directories
if exist "build\Tokri.exe" (
    set "EXE=build\Tokri.exe"
) else if exist "build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release\Tokri.exe" (
    set "EXE=build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release\Tokri.exe"
) else if exist "build\Release\Tokri.exe" (
    set "EXE=build\Release\Tokri.exe"
) else (
    echo Error: Tokri.exe not found. Please build the project first.
    exit /b 1
)

set OUTDIR=dist\Windows
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

rem Stop any running Tokri instance so files in dist\Windows are not locked
taskkill /F /IM Tokri.exe >nul 2>nul

rem Resolve windeployqt from PATH or standard C:\Qt installation
set "WINDEPLOYQT="
where windeployqt >nul 2>nul
if %errorlevel% equ 0 (
    set "WINDEPLOYQT=windeployqt"
) else if exist "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" (
    set "WINDEPLOYQT=C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe"
    set "PATH=C:\Qt\6.12.0\msvc2022_64\bin;%PATH%"
) else (
    for /d %%D in ("C:\Qt\6*") do (
        if exist "%%D\msvc2022_64\bin\windeployqt.exe" (
            set "WINDEPLOYQT=%%D\msvc2022_64\bin\windeployqt.exe"
            set "PATH=%%D\msvc2022_64\bin;%PATH%"
        )
    )
)

if defined WINDEPLOYQT (
    echo Using windeployqt: "!WINDEPLOYQT!"
    "!WINDEPLOYQT!" --dir "%OUTDIR%" "%EXE%"
) else (
    echo Error: windeployqt.exe not found in PATH or C:\Qt.
    exit /b 1
)

rem Copy exe to release (overwrite)
copy /Y "%EXE%" "%OUTDIR%\Tokri.exe"

rem Deploy WebP image format plugin
if not exist "%OUTDIR%\imageformats" mkdir "%OUTDIR%\imageformats"
if exist "C:\Qt\Tools\QtCreator\bin\plugins\imageformats\qwebp.dll" (
    copy /Y "C:\Qt\Tools\QtCreator\bin\plugins\imageformats\qwebp.dll" "%OUTDIR%\imageformats\qwebp.dll"
)

endlocal
