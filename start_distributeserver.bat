@echo off
REM =====================================================
REM Murim MMORPG DistributeServer Startup Script
REM =====================================================

cd /d D:\moxiang-server
title Murim DistributeServer (Port 8000)
color 0B

echo ========================================================
echo       Murim MMORPG DistributeServer
echo ========================================================
echo 端口: 8000
echo 功能: 负载均衡、服务器发现、健康检查
echo ========================================================
echo.

REM 设置依赖库路径
set PATH=%PATH%;D:\vcpkg\installed\x64-windows\bin

REM 检查可执行文件
if not exist "build\distribute-server\Release\murim_distributeserver.exe" (
    echo [错误] 找不到可执行文件，请先编译服务器
    pause
    exit /b 1
)

echo 启动 DistributeServer...
echo.

"build\distribute-server\Release\murim_distributeserver.exe"

echo.
echo DistributeServer 已停止。
pause
