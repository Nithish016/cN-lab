@echo off
setlocal enabledelayedexpansion
title Build Multi-Client Chat Application (Windows Winsock 2)

echo ======================================================================
echo    BUILD SCRIPT: MULTI-CLIENT CHAT APPLICATION (C / WINDOWS)
echo    COMPUTER NETWORKS LAB PROJECT
echo ======================================================================
echo.

:: 1. Search for GCC in PATH or common Windows installation directories
set "CC="

where gcc.exe >nul 2>&1
if !errorlevel! equ 0 (
    set "CC=gcc"
    goto FOUND_CC
)

:: Search WinGet Package location
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW*") do (
    if exist "%%D\llvm-mingw-*\bin\gcc.exe" (
        for /d %%B in ("%%D\llvm-mingw-*\bin") do (
            set "PATH=%%~fB;!PATH!"
            set "CC=%%~fB\gcc.exe"
            goto FOUND_CC
        )
    )
)

:: Search standard MinGW and MSYS2 locations
if exist "C:\MinGW\bin\gcc.exe" (
    set "PATH=C:\MinGW\bin;!PATH!"
    set "CC=C:\MinGW\bin\gcc.exe"
    goto FOUND_CC
)

if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;!PATH!"
    set "CC=C:\msys64\mingw64\bin\gcc.exe"
    goto FOUND_CC
)

if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set "PATH=C:\msys64\ucrt64\bin;!PATH!"
    set "CC=C:\msys64\ucrt64\bin\gcc.exe"
    goto FOUND_CC
)

:: Check for MSVC cl.exe
where cl.exe >nul 2>&1
if !errorlevel! equ 0 (
    echo [*] Microsoft Visual C++ Compiler (cl.exe) detected.
    echo [*] Compiling Server with MSVC...
    cl.exe /W3 /O2 /MD /Fe:server.exe server\server.c ws2_32.lib
    if !errorlevel! neq 0 (
        echo [ERROR] Server build failed with MSVC!
        pause
        exit /b 1
    )
    copy /Y server.exe server\server.exe >nul 2>&1

    echo [*] Compiling Client with MSVC...
    cl.exe /W3 /O2 /MD /Fe:client.exe client\client.c ws2_32.lib
    if !errorlevel! neq 0 (
        echo [ERROR] Client build failed with MSVC!
        pause
        exit /b 1
    )
    copy /Y client.exe client\client.exe >nul 2>&1
    del *.obj >nul 2>&1
    goto BUILD_SUCCESS
)

echo [ERROR] No C compiler (GCC or MSVC cl.exe) found on PATH or standard locations.
echo.
echo To install MinGW GCC quickly on Windows 10/11, run PowerShell:
echo   winget install MartinStorsjo.LLVM-MinGW.UCRT
echo Or install MinGW-w64 from https://www.msys2.org/
echo.
pause
exit /b 1

:FOUND_CC
echo [*] Found C Compiler: !CC!
echo.

echo [*] Compiling Server (server/server.c) ...
!CC! -Wall -Wextra -O2 server/server.c -o server.exe -lws2_32
if !errorlevel! neq 0 (
    echo.
    echo [ERROR] Failed to compile server.exe!
    pause
    exit /b 1
)
copy /Y server.exe server\server.exe >nul 2>&1
echo [+] server.exe built successfully.

echo [*] Compiling Client (client/client.c) ...
!CC! -Wall -Wextra -O2 client/client.c -o client.exe -lws2_32
if !errorlevel! neq 0 (
    echo.
    echo [ERROR] Failed to compile client.exe!
    pause
    exit /b 1
)
copy /Y client.exe client\client.exe >nul 2>&1
echo [+] client.exe built successfully.

:BUILD_SUCCESS
:: Ensure runtime directories exist
if not exist "received_files" mkdir "received_files"
if not exist "files\received_files" mkdir "files\received_files"

echo.
echo ======================================================================
echo    BUILD COMPLETED SUCCESSFULLY!
echo ======================================================================
echo Executables created:
echo   - server.exe (and server\server.exe)
echo   - client.exe (and client\client.exe)
echo.
echo How to Run:
echo   1. Double click 'run_server.bat' to start the server.
echo   2. Double click 'run_client.bat' in multiple windows for clients.
echo ======================================================================
echo.
pause
