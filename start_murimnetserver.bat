@echo off
REM =====================================================
REM Murim MMORPG MurimNetServer Startup Script
REM =====================================================

cd /d D:\moxiang-server

echo =====================================================
echo       Murim MMORPG MurimNetServer
echo =====================================================
echo Starting MurimNetServer on port 8500...
echo =====================================================
echo.

REM Set PostgreSQL and vcpkg paths for DLL dependencies
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM Start MurimNetServer
start "MurimNetServer" D:\moxiang-server\build\murimnet-server\Release\murim_murimnetserver.exe

echo MurimNetServer started. Check console window for logs.
echo Press any key to stop...
pause >nul

echo Stopping MurimNetServer...
taskkill /F /IM murim_murimnetserver.exe 2>nul
echo MurimNetServer stopped.
