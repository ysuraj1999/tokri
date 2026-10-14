@echo off
setlocal enabledelayedexpansion

echo === Step 1: Running build-windows.bat to deploy Qt dependencies ===
call "%~dp0build-windows.bat"
if %errorlevel% neq 0 (
    echo Error: build-windows.bat failed.
    exit /b %errorlevel%
)

echo.
echo === Step 2: Resolving Inno Setup Compiler (ISCC.exe) ===
set "ISCC="
where iscc >nul 2>nul
if %errorlevel% equ 0 (
    set "ISCC=iscc"
) else if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" (
    set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
) else if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
)

if not defined ISCC (
    echo Error: Inno Setup Compiler ^(ISCC.exe^) not found.
    echo Please ensure JRSoftware.InnoSetup is installed.
    exit /b 1
)

echo Using ISCC: "!ISCC!"
echo.
echo === Step 3: Generating TokriSetup.exe Installer ===
"!ISCC!" "%~dp0windows-installer-script.iss"
if %errorlevel% neq 0 (
    echo Error: Installer compilation failed.
    exit /b %errorlevel%
)

echo.
echo === Success: Installer generated at dist\installer\TokriSetup.exe ===
endlocal
