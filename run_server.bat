@echo off
title Multi-Client Chat Server (Port 8080)
cd /d "%~dp0"

echo ======================================================================
echo    STARTING MULTI-CLIENT CHAT SERVER
echo ======================================================================

if not exist "server.exe" (
    if exist "server\server.exe" (
        copy /Y "server\server.exe" "server.exe" >nul 2>&1
    ) else (
        echo [ERROR] server.exe not found! Running build.bat first...
        call build.bat
    )
)

server.exe 8888
if %errorlevel% neq 0 (
    echo.
    echo Server exited with error code %errorlevel%.
    pause
)
