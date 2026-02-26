@echo off
REM =====================================================
REM Murim MMORPG DistributeServer Startup Script
REM =====================================================

cd /d D:\moxiang-server

echo =====================================================
echo       Murim MMORPG DistributeServer
echo =====================================================
echo Starting DistributeServer on port 8000...
echo =====================================================
echo.

REM Start DistributeServer
start "DistributeServer" D:\moxiang-server\build\distribute-server\Release\murim_distributeserver.exe

echo DistributeServer started. Check console window for logs.
echo Press any key to stop...
pause >nul

echo Stopping DistributeServer...
taskkill /F /IM murim_distributeserver.exe 2>nul
echo DistributeServer stopped.
