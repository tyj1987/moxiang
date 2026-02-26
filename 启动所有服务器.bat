@echo off
REM =====================================================
REM Murim MMORPG 服务器快速启动
REM =====================================================

cd /d D:\moxiang-server
echo ========================================================
echo       Murim MMORPG 服务器启动
echo ========================================================
echo.

REM 使用 PowerShell 启动所有服务器
powershell -Command "$env:PATH = 'D:\vcpkg\installed\x64-windows\bin;C:\Program Files\PostgreSQL\17\bin;' + $env:PATH; Start-Process 'build\distribute-server\Release\murim_distributeserver.exe'; Start-Sleep -Seconds 1; Start-Process 'build\agent-server\Release\murim_agentserver.exe'; Start-Sleep -Seconds 1; Start-Process 'build\murimnet-server\Release\murim_murimnetserver.exe'; Start-Sleep -Seconds 1; Start-Process 'build\map-server\Release\murim_mapserver.exe'; Write-Host 'All servers started!' -ForegroundColor Green"

timeout /t 3 /nobreak >nul

echo.
echo ========================================================
echo 服务器状态:
echo ========================================================
powershell -Command "Get-Process -Name murim_* -ErrorAction SilentlyContinue | Format-Table Name, Id, @{Name='Status';Expression={'Running'}} -AutoSize"
echo.
echo 测试账号: test / test123
echo.
echo 按任意键查看状态菜单...
pause >nul

call check_status.bat
