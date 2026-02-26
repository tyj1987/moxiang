@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo.
echo ========================================
echo    Murim MMORPG 服务器启动
echo ========================================
echo.

REM 设置 DLL 路径
set "PATH=D:\vcpkg\installed\x64-windows\bin;C:\Program Files\PostgreSQL\17\bin;%PATH%"

REM 启动 DistributeServer
echo [1/4] 启动 DistributeServer...
start "DistributeServer-8000" "%CD%\build\distribute-server\Release\murim_distributeserver.exe"

REM 启动 AgentServer
echo [2/4] 启动 AgentServer...
timeout /t 1 /nobreak >nul
start "AgentServer-7000" "%CD%\build\agent-server\Release\murim_agentserver.exe"

REM 启动 MurimNetServer
echo [3/4] 启动 MurimNetServer...
timeout /t 1 /nobreak >nul
start "MurimNetServer-8500" "%CD%\build\murimnet-server\Release\murim_murimnetserver.exe"

REM 启动 MapServer
echo [4/4] 启动 MapServer...
timeout /t 1 /nobreak >nul
start "MapServer-9001" "%CD%\build\map-server\Release\murim_mapserver.exe"

echo.
echo ========================================
echo    所有服务器已启动！
echo ========================================
echo.
timeout /t 2 /nobreak >nul

echo 服务器进程:
tasklist | findstr /i "murim_"
echo.
echo.
echo 测试账号: test / test123
echo.
echo 按任意键查看状态...
pause >nul

REM 状态监控
:status
cls
echo.
echo ========================================
echo    服务器状态监控
echo ========================================
echo.
echo 运行中的服务器:
tasklist | findstr /i "murim_" | find /v /v ""
echo.
echo.
echo [R] 刷新  [S] 停止所有  [Q] 退出监控
echo.
choice /c RSQ /n /m "请选择: "
if errorlevel 3 goto :eof
if errorlevel 2 goto stop
if errorlevel 1 goto status

:stop
echo.
echo 正在停止所有服务器...
taskkill /F /IM murim_distributeserver.exe >nul 2>&1
taskkill /F /IM murim_agentserver.exe >nul 2>&1
taskkill /F /IM murim_murimnetserver.exe >nul 2>&1
taskkill /F /IM murim_mapserver.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo.
echo 所有服务器已停止
pause
