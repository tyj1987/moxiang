@echo off
REM =====================================================
REM Murim MMORPG 服务器启动 (修复 DLL 依赖)
REM =====================================================

cd /d D:\moxiang-server

echo ========================================================
echo       Murim MMORPG 服务器启动
echo ========================================================
echo.

REM 设置 PATH 包含所有依赖 DLL
set "PATH=D:\vcpkg\installed\x64-windows\bin;C:\Program Files\PostgreSQL\17\bin;%PATH%"

REM 显示 PATH 设置
echo [INFO] DLL 搜索路径已设置
echo.

REM 检查 DLL 文件
echo [INFO] 检查必需的 DLL 文件...
if exist "D:\vcpkg\installed\x64-windows\bin\fmt.dll" (
    echo     [OK] fmt.dll
) else (
    echo     [WARNING] fmt.dll not found
)
if exist "C:\Program Files\PostgreSQL\17\bin\libpq.dll" (
    echo     [OK] libpq.dll
) else (
    echo     [WARNING] libpq.dll not found
)
echo.

REM 使用 start 命令在新窗口中启动，继承当前环境
echo [1/4] 启动 DistributeServer (端口 8000)...
start "Murim-DistributeServer" /D "%CD%" cmd /k "echo DistributeServer running... && build\distribute-server\Release\murim_distributeserver.exe"
timeout /t 2 /nobreak >nul

echo [2/4] 启动 AgentServer (端口 7000)...
start "Murim-AgentServer" /D "%CD%" cmd /k "echo AgentServer running... && build\agent-server\Release\murim_agentserver.exe"
timeout /t 2 /nobreak >nul

echo [3/4] 启动 MurimNetServer (端口 8500)...
start "Murim-MurimNetServer" /D "%CD%" cmd /k "echo MurimNetServer running... && build\murimnet-server\Release\murim_murimnetserver.exe"
timeout /t 2 /nobreak >nul

echo [4/4] 启动 MapServer (端口 9001)...
start "Murim-MapServer" /D "%CD%" cmd /k "echo MapServer running... && build\map-server\Release\murim_mapserver.exe"
timeout /t 3 /nobreak >nul

echo.
echo ========================================================
echo 服务器启动完成！
echo ========================================================
echo.
echo 检查运行状态:
tasklist | findstr /i murim_
echo.
echo 测试账号:
echo   用户名: test
echo   密码: test123
echo.
echo 按任意键打开状态监控...
pause >nul

call check_status.bat
