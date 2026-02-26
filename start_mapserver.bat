@echo off
REM =====================================================
REM Murim MMORPG MapServer Startup Script
REM =====================================================

cd /d D:\moxiang-server

echo =====================================================
echo       Murim MMORPG MapServer
echo =====================================================
echo Starting MapServer on port 9001...
echo =====================================================
echo.

REM Set PostgreSQL and vcpkg paths for DLL dependencies
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM Start MapServer
start "MapServer" D:\moxiang-server\build\map-server\Release\murim_mapserver.exe

echo MapServer started. Check console window for logs.
echo Press any key to stop...
pause >nul

echo Stopping MapServer...
taskkill /F /IM murim_mapserver.exe 2>nul
echo MapServer stopped.
