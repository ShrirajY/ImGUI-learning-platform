@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
title ImGui Lab - Setup

echo.
echo  ==========================================================
echo    ImGui Lab - one-time setup
echo  ==========================================================
echo.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

rem ----------------------------------------------------------------
rem  1. Node.js (runs the local website)
rem ----------------------------------------------------------------
echo [1/4] Checking Node.js ...
where node >nul 2>nul
if errorlevel 1 (
    if exist "%ProgramFiles%\nodejs\node.exe" set "PATH=%ProgramFiles%\nodejs;!PATH!"
)
where node >nul 2>nul
if errorlevel 1 (
    echo       Node.js not found. Installing Node.js LTS with winget ...
    where winget >nul 2>nul
    if errorlevel 1 goto :no_winget_node
    winget install -e --id OpenJS.NodeJS.LTS --accept-package-agreements --accept-source-agreements
    set "PATH=%ProgramFiles%\nodejs;!PATH!"
    where node >nul 2>nul
    if errorlevel 1 goto :no_winget_node
)
for /f "tokens=*" %%v in ('node -v') do echo       OK  Node.js %%v

rem ----------------------------------------------------------------
rem  2. Visual Studio C++ compiler (builds the lesson code)
rem ----------------------------------------------------------------
echo.
echo [2/4] Checking Visual Studio C++ build tools ...
set "VSPATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH (
    echo       MSVC C++ tools not found.
    echo       They are needed to compile lessons into real OpenGL windows - about 3-5 GB.
    choice /c YN /m "      Install Visual Studio 2022 Build Tools now with winget"
    if errorlevel 2 (
        echo       Skipped. The website will still open, but Build ^& Run will not work.
        goto :imgui
    )
    where winget >nul 2>nul
    if errorlevel 1 goto :no_winget_vs
    winget install -e --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if exist "%VSWHERE%" (
        for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
    )
)
if defined VSPATH (echo       OK  !VSPATH!) else (echo       WARNING: C++ tools still not detected.)

rem ----------------------------------------------------------------
rem  3. Dear ImGui sources (sandbox\external\imgui)
rem ----------------------------------------------------------------
:imgui
echo.
echo [3/4] Checking Dear ImGui sources ...
set "IMGUI=sandbox\external\imgui"
if exist "%IMGUI%\imgui_impl_opengl2.cpp" goto :imgui_ok

if exist "..\setup\ImGui_OpenGL_Template\external\imgui\imgui.cpp" (
    echo       Copying from ..\setup\ImGui_OpenGL_Template ...
    xcopy /e /i /y /q "..\setup\ImGui_OpenGL_Template\external\imgui" "%IMGUI%" >nul
    goto :imgui_ok
)
echo       Downloading Dear ImGui from GitHub ...
if not exist sandbox\cache mkdir sandbox\cache
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol='Tls12';" ^
  "Invoke-WebRequest 'https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.zip' -OutFile 'sandbox\cache\imgui.zip';" ^
  "Expand-Archive 'sandbox\cache\imgui.zip' 'sandbox\cache\imgui_src' -Force;" ^
  "$s=(Get-ChildItem 'sandbox\cache\imgui_src' -Directory)[0].FullName;" ^
  "New-Item -ItemType Directory -Force '%IMGUI%' | Out-Null;" ^
  "Copy-Item \"$s\*.h\",\"$s\*.cpp\" '%IMGUI%';" ^
  "Copy-Item \"$s\backends\imgui_impl_win32.*\",\"$s\backends\imgui_impl_opengl2.*\" '%IMGUI%'"
if errorlevel 1 goto :imgui_fail

:imgui_ok
echo       OK  %IMGUI%

rem Keep a copy of the starter zip + guide available on the website's Setup page
if not exist public\downloads mkdir public\downloads
if exist "..\setup\ImGui_OpenGL_Template.zip" copy /y "..\setup\ImGui_OpenGL_Template.zip" public\downloads\ >nul
if exist "..\setup\ImGui_OpenGL_Setup_Guide.md" copy /y "..\setup\ImGui_OpenGL_Setup_Guide.md" public\downloads\ >nul

rem ----------------------------------------------------------------
rem  4. Test build (also pre-compiles the ImGui library once)
rem ----------------------------------------------------------------
echo.
echo [4/4] Test build of the lesson engine ...
if not defined VSPATH (
    echo       Skipped - no C++ compiler.
    goto :done
)
node server\build-cli.js public\lessons\03-first-window.cpp
if errorlevel 1 (
    echo       Test build FAILED - see messages above.
    goto :done
)
echo       OK  engine builds

:done
rem First setup on this machine: create a unique install id.
rem The website resets saved progress to 0 when it sees a new id.
rem (Delete .install_id and run setup.bat again to start over.)
if not exist .install_id (
    powershell -NoProfile -Command "[guid]::NewGuid().ToString()" > .install_id
    echo       New install - lesson progress starts from 0.
)
echo ok> .setup_done
echo.
echo  ==========================================================
echo    Setup finished.  Start the website with:  run.bat
echo  ==========================================================
echo.
if /i not "%~1"=="/quiet" pause
exit /b 0

:no_winget_node
echo.
echo  ERROR: Could not install Node.js automatically.
echo  Install the LTS version from https://nodejs.org and run setup.bat again.
pause
exit /b 1

:no_winget_vs
echo.
echo  ERROR: winget is not available.
echo  Install "Visual Studio 2022 Community" from https://visualstudio.microsoft.com
echo  with the workload "Desktop development with C++", then run setup.bat again.
pause
exit /b 1

:imgui_fail
echo.
echo  ERROR: Could not get Dear ImGui sources. Copy the 'external\imgui' folder
echo  from ImGui_OpenGL_Template.zip into  sandbox\external\imgui  and run setup.bat again.
pause
exit /b 1
