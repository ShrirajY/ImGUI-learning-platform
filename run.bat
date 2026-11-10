@echo off
cd /d "%~dp0"
title ImGui Lab - Server

rem First time? Run the setup.
if not exist .setup_done call setup.bat /quiet

where node >nul 2>nul
if errorlevel 1 if exist "%ProgramFiles%\nodejs\node.exe" set "PATH=%ProgramFiles%\nodejs;%PATH%"
where node >nul 2>nul
if errorlevel 1 (
    echo Node.js not found - run setup.bat first.
    pause
    exit /b 1
)

if "%PORT%"=="" set PORT=5173

rem Open the browser shortly after the server starts
start "" /min cmd /c "timeout /t 2 /nobreak >nul & start http://localhost:%PORT%"

node server\server.js
pause
