@echo off
REM =====================================================
REM Murim MMORPG 服务器状态检查
REM =====================================================

cd /d D:\moxiang-server
title Murim 服务器状态

cls
echo.
echo ========================================================
echo       Murim MMORPG 服务器状态
echo ========================================================
echo.
echo 检查运行中的服务器...
echo.

REM 检查各个服务器
set COUNT=0

tasklist | findstr /i "murim_distributeserver.exe" >nul
if %errorlevel%==0 (
    echo [RUNNING] DistributeServer (端口 8000) - 负载均衡
    set /a COUNT+=1
) else (
    echo [STOPPED] DistributeServer (端口 8000)
)

tasklist | findstr /i "murim_agentserver.exe" >nul
if %errorlevel%==0 (
    echo [RUNNING] AgentServer (端口 7000) - 认证服务
    set /a COUNT+=1
) else (
    echo [STOPPED] AgentServer (端口 7000)
)

tasklist | findstr /i "murim_murimnetserver.exe" >nul
if %errorlevel%==0 (
    echo [RUNNING] MurimNetServer (端口 8500) - 跨服务器服务
    set /a COUNT+=1
) else (
    echo [STOPPED] MurimNetServer (端口 8500)
)

tasklist | findstr /i "murim_mapserver.exe" >nul
if %errorlevel%==0 (
    echo [RUNNING] MapServer (端口 9001) - 游戏世界
    set /a COUNT+=1
) else (
    echo [STOPPED] MapServer (端口 9001)
)

echo.
echo ========================================================
echo 运行中: %COUNT% / 4 个服务器
echo ========================================================
echo.
echo 操作选项:
echo   [1] 启动所有服务器
echo   [2] 停止所有服务器
echo   [3] 刷新状态
echo   [0] 退出
echo.

set /p choice=请选择操作:

if "%choice%"=="1" goto start_all
if "%choice%"=="2" goto stop_all
if "%choice%"=="3" goto check_again
if "%choice%"=="0" exit /b 0
exit /b 0

:start_all
echo.
echo 正在启动所有服务器...
start "启动服务器" /MIN cmd /c "start_all_servers.bat"
timeout /t 3 /nobreak >nul
goto check_again

:stop_all
echo.
echo 正在停止所有服务器...
taskkill /F /IM murim_distributeserver.exe >nul 2>&1
taskkill /F /IM murim_agentserver.exe >nul 2>&1
taskkill /F /IM murim_murimnetserver.exe >nul 2>&1
taskkill /F /IM murim_mapserver.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo 所有服务器已停止。
pause
exit /b 0

:check_again
cls
goto :EOF
