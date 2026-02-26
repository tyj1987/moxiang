@echo off
REM =====================================================
REM Murim MMORPG All Servers Startup Script
REM =====================================================

cd /d D:\moxiang-server

echo =====================================================
echo       Murim MMORPG Server Startup
echo =====================================================
echo.
echo Starting all servers in recommended order:
echo   1. DistributeServer (port 8000)
echo   2. AgentServer (port 7000)
echo   3. MurimNetServer (port 8500)
echo   4. MapServer (port 9001)
echo =====================================================
echo.

REM Set PostgreSQL and vcpkg paths for DLL dependencies
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM Start servers in order
echo [1/4] Starting DistributeServer...
start "DistributeServer" D:\moxiang-server\build\distribute-server\Release\murim_distributeserver.exe
timeout /t 2 /nobreak >nul

echo [2/4] Starting AgentServer...
start "AgentServer" D:\moxiang-server\build\agent-server\Release\murim_agentserver.exe
timeout /t 2 /nobreak >nul

echo [3/4] Starting MurimNetServer...
start "MurimNetServer" D:\moxiang-server\build\murimnet-server\Release\murim_murimnetserver.exe
timeout /t 2 /nobreak >nul

echo [4/4] Starting MapServer...
start "MapServer" D:\moxiang-server\build\map-server\Release\murim_mapserver.exe
timeout /t 2 /nobreak >nul

echo.
echo =====================================================
echo All servers started successfully!
echo =====================================================
echo.
echo Press any key to stop all servers...
pause >nul

echo.
echo Stopping all servers...
taskkill /F /IM murim_distributeserver.exe 2>nul
taskkill /F /IM murim_agentserver.exe 2>nul
taskkill /F /IM murim_murimnetserver.exe 2>nul
taskkill /F /IM murim_mapserver.exe 2>nul
echo All servers stopped.
