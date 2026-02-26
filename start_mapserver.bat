@echo off
REM =====================================================
REM Murim MMORPG MapServer Startup Script
REM =====================================================

cd /d D:\moxiang-server
title Murim MapServer (Port 9001)
color 0C

echo ========================================================
echo       Murim MMORPG MapServer
echo ========================================================
echo 端口: 9001
echo 地图ID: 1 (主世界)
echo 功能: 游戏世界模拟、战斗、AI、技能、物品
echo ========================================================
echo.

REM 设置依赖库路径
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM 检查可执行文件
if not exist "build\map-server\Release\murim_mapserver.exe" (
    echo [错误] 找不到可执行文件，请先编译服务器
    pause
    exit /b 1
)

echo 启动 MapServer...
echo.

"build\map-server\Release\murim_mapserver.exe"

echo.
echo MapServer 已停止。
pause
