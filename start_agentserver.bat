@echo off
REM =====================================================
REM Murim MMORPG AgentServer Startup Script
REM =====================================================

cd /d D:\moxiang-server

echo =====================================================
echo       Murim MMORPG AgentServer
echo =====================================================
echo Starting AgentServer on port 7000...
echo =====================================================
echo.

REM Set PostgreSQL and vcpkg paths for DLL dependencies
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM Start AgentServer
start "AgentServer" D:\moxiang-server\build\agent-server\Release\murim_agentserver.exe

echo AgentServer started. Check console window for logs.
echo Press any key to stop...
pause >nul

echo Stopping AgentServer...
taskkill /F /IM murim_agentserver.exe 2>nul
echo AgentServer stopped.
