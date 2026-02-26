@echo off
REM =====================================================
REM Murim MMORPG All Servers Startup Script
REM =====================================================

cd /d D:\moxiang-server

REM 设置控制台窗口标题和颜色
title Murim MMORPG Servers
color 0A

echo.
echo ========================================================
echo       Murim MMORPG Server Startup
echo ========================================================
echo.
echo 启动所有服务器 (推荐启动顺序):
echo   1. DistributeServer (端口 8000) - 负载均衡
echo   2. AgentServer (端口 7000) - 认证和角色选择
echo   3. MurimNetServer (端口 8500) - 跨服务器服务
echo   4. MapServer (端口 9001) - 游戏世界
echo ========================================================
echo.

REM 设置依赖库路径
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM 检查可执行文件是否存在
if not exist "build\distribute-server\Release\murim_distributeserver.exe" (
    echo [错误] 找不到 DistributeServer 可执行文件
    echo 请先编译服务器: cd build ^&^& cmake .. ^&^& cmake --build . --config Release
    pause
    exit /b 1
)

REM 启动服务器 - 使用 start 命令在新窗口中运行
echo [1/4] 启动 DistributeServer...
start "Murim-DistributeServer" /D "%CD%" "build\distribute-server\Release\murim_distributeserver.exe"
timeout /t 2 /nobreak >nul

echo [2/4] 启动 AgentServer...
start "Murim-AgentServer" /D "%CD%" "build\agent-server\Release\murim_agentserver.exe"
timeout /t 2 /nobreak >nul

echo [3/4] 启动 MurimNetServer...
start "Murim-MurimNetServer" /D "%CD%" "build\murimnet-server\Release\murim_murimnetserver.exe"
timeout /t 2 /nobreak >nul

echo [4/4] 启动 MapServer...
start "Murim-MapServer" /D "%CD%" "build\map-server\Release\murim_mapserver.exe"
timeout /t 2 /nobreak >nul

echo.
echo ========================================================
echo 所有服务器已启动！
echo ========================================================
echo.
echo 服务器进程 (可以在任务管理器中查看):
echo   - murim_distributeserver.exe
echo   - murim_agentserver.exe
echo   - murim_murimnetserver.exe
echo   - murim_mapserver.exe
echo.
echo 测试账号:
echo   用户名: test
echo   密码: test123
echo.
echo 按任意键查看服务器状态...
pause >nul

:status_menu
cls
echo.
echo ========================================================
echo       Murim MMORPG 服务器状态
echo ========================================================
echo.
tasklist | findstr /i "murim_"
echo.
echo ========================================================
echo.
echo [1] 刷新状态
echo [2] 停止所有服务器
echo [0] 退出 (服务器继续运行)
echo.
set /p choice=请选择操作:

if "%choice%"=="1" goto status_menu
if "%choice%"=="2" goto stop_servers
if "%choice%"=="0" exit /b 0
goto status_menu

:stop_servers
echo.
echo 正在停止所有服务器...
taskkill /F /IM murim_distributeserver.exe >nul 2>&1
taskkill /F /IM murim_agentserver.exe >nul 2>&1
taskkill /F /IM murim_murimnetserver.exe >nul 2>&1
taskkill /F /IM murim_mapserver.exe >nul 2>&1
timeout /t 1 /nobreak >nul
echo 所有服务器已停止。
pause
exit /b 0
