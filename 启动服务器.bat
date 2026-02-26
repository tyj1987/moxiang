@echo off
REM =====================================================
REM Murim MMORPG 服务器启动脚本
REM 此脚本会在每个服务器的目录中运行，确保找到所有 DLL
REM =====================================================

cd /d D:\moxiang-server
set "ORIGINAL_PATH=%PATH%"

echo ========================================================
echo       Murim MMORPG 服务器启动
echo ========================================================
echo.

REM 设置 DLL 搜索路径
set "PATH=D:\vcpkg\installed\x64-windows\bin;C:\Program Files\PostgreSQL\17\bin;%ORIGINAL_PATH%"

REM 检查 DLL 是否可访问
echo [检查] 验证依赖库...
where libpq.dll >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 libpq.dll，请检查 PostgreSQL 是否已安装
    pause
    exit /b 1
)
echo [OK] libpq.dll 可访问

where fmt.dll >nul 2>&1
if errorlevel 1 (
    echo [错误] 找不到 fmt.dll，请检查 vcpkg 是否已安装
    pause
    exit /b 1
)
echo [OK] fmt.dll 可访问
echo.

REM 在后台启动服务器（使用 start /B 并重定向输出）
echo [1/4] 启动 DistributeServer...
start "Murim-DistributeServer" /D "%CD%\build\distribute-server\Release" murim_distributeserver.exe >nul 2>&1
if errorlevel 1 (
    echo [错误] DistributeServer 启动失败
) else (
    echo [OK] DistributeServer 已启动
)
timeout /t 1 /nobreak >nul

echo [2/4] 启动 AgentServer...
start "Murim-AgentServer" /D "%CD%\build\agent-server\Release" murim_agentserver.exe >nul 2>&1
if errorlevel 1 (
    echo [错误] AgentServer 启动失败
) else (
    echo [OK] AgentServer 已启动
)
timeout /t 1 /nobreak >nul

echo [3/4] 启动 MurimNetServer...
start "Murim-MurimNetServer" /D "%CD%\build\murimnet-server\Release" murim_murimnetserver.exe >nul 2>&1
if errorlevel 1 (
    echo [错误] MurimNetServer 启动失败
) else (
    echo [OK] MurimNetServer 已启动
)
timeout /t 1 /nobreak >nul

echo [4/4] 启动 MapServer...
start "Murim-MapServer" /D "%CD%\build\map-server\Release" murim_mapserver.exe >nul 2>&1
if errorlevel 1 (
    echo [错误] MapServer 启动失败
) else (
    echo [OK] MapServer 已启动
)
timeout /t 2 /nobreak >nul

echo.
echo ========================================================
echo 服务器启动完成！
echo ========================================================
echo.

REM 等待 3 秒让服务器完全启动
timeout /t 3 /nobreak >nul

REM 检查运行状态
echo 运行中的服务器:
tasklist | findstr /i "murim_"
echo.

echo 测试账号:
echo   用户名: test
echo   密码: test123
echo.

REM 进入状态监控循环
:menu
echo ========================================================
echo 选项:
echo   [1] 刷新状态
echo   [2] 停止所有服务器
echo   [0] 退出
echo ========================================================
echo.
set /p choice=请选择:

if "%choice%"=="1" goto status
if "%choice%"=="2" goto stop
if "%choice%"=="0" goto end
goto menu

:status
cls
echo.
echo 服务器状态:
tasklist | findstr /i "murim_"
echo.
goto menu

:stop
echo.
echo 正在停止所有服务器...
taskkill /F /IM murim_distributeserver.exe >nul 2>&1
taskkill /F /IM murim_agentserver.exe >nul 2>&1
taskkill /F /IM murim_murimnetserver.exe >nul 2>&1
taskkill /F /IM murim_mapserver.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo 所有服务器已停止
pause
exit /b 0

:end
echo.
echo 服务器将继续在后台运行
exit /b 0
