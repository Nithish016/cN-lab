@echo off
title Multi-Client Chat Client
cd /d "%~dp0"

if not exist "client.exe" (
    if exist "client\client.exe" (
        copy /Y "client\client.exe" "client.exe" >nul 2>&1
    ) else (
        echo [ERROR] client.exe not found! Running build.bat first...
        call build.bat
    )
)

client.exe
if %errorlevel% neq 0 (
    echo.
    echo Client exited with code %errorlevel%.
    pause
)
