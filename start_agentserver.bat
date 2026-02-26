@echo off
REM =====================================================
REM Murim MMORPG AgentServer Startup Script
REM =====================================================

cd /d D:\moxiang-server
title Murim AgentServer (Port 7000)
color 0E

echo ========================================================
echo       Murim MMORPG AgentServer
echo ========================================================
echo 端口: 7000
echo 功能: 用户认证、角色创建/选择、负载均衡
echo ========================================================
echo.

REM 设置依赖库路径
set PATH=%PATH%;C:\Program Files\PostgreSQL\17\bin;D:\vcpkg\installed\x64-windows\bin

REM 检查可执行文件
if not exist "build\agent-server\Release\murim_agentserver.exe" (
    echo [错误] 找不到可执行文件，请先编译服务器
    pause
    exit /b 1
)

echo 启动 AgentServer...
echo 测试账号: test / test123
echo.

"build\agent-server\Release\murim_agentserver.exe"

echo.
echo AgentServer 已停止。
pause
