@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo.
echo ========================================
echo    停止 Murim 服务器
echo ========================================
echo.

echo 正在停止所有服务器...
taskkill /F /IM murim_distributeserver.exe >nul 2>&1
taskkill /F /IM murim_agentserver.exe >nul 2>&1
taskkill /F /IM murim_murimnetserver.exe >nul 2>&1
taskkill /F /IM murim_mapserver.exe >nul 2>&1

timeout /t 2 /nobreak >nul

echo.
echo 剩余进程:
tasklist | findstr /i "murim_"
echo.

echo 完成！
timeout /t 2 /nobreak >nul
