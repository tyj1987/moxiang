@echo off
REM =====================================================
REM Murim MMORPG MurimNetServer Startup Script
REM =====================================================

cd /d D:\moxiang-server
title Murim MurimNetServer (Port 8500)
color 0A

echo ========================================================
echo       Murim MMORPG MurimNetServer
echo ========================================================
echo 端口: 8500
echo 功能: 公会聊天、好友系统、市场、邮件
echo ========================================================
echo.

REM 设置依赖库路径
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM 检查可执行文件
if not exist "build\murimnet-server\Release\murim_murimnetserver.exe" (
    echo [错误] 找不到可执行文件，请先编译服务器
    pause
    exit /b 1
)

echo 启动 MurimNetServer...
echo.

"build\murimnet-server\Release\murim_murimnetserver.exe"

echo.
echo MurimNetServer 已停止。
pause
