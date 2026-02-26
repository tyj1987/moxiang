@echo off
cd /d D:\moxiang-server
echo Starting all Murim servers...

start "Murim-DistributeServer" /D "%CD%" cmd /k "title Murim-DistributeServer (Port 8000) ^&^& echo Starting DistributeServer... ^&^& build\distribute-server\Release\murim_distributeserver.exe"

timeout /t 1 /nobreak >nul

start "Murim-AgentServer" /D "%CD%" cmd /k "title Murim-AgentServer (Port 7000) ^&^& echo Starting AgentServer... ^&^& build\agent-server\Release\murim_agentserver.exe"

timeout /t 1 /nobreak >nul

start "Murim-MurimNetServer" /D "%CD%" cmd /k "title Murim-MurimNetServer (Port 8500) ^&^& echo Starting MurimNetServer... ^&^& build\murimnet-server\Release\murim_murimnetserver.exe"

timeout /t 1 /nobreak >nul

start "Murim-MapServer" /D "%CD%" cmd /k "title Murim-MapServer (Port 9001) ^&^& echo Starting MapServer... ^&^& build\map-server\Release\murim_mapserver.exe"

timeout /t 1 /nobreak >nul

echo.
echo All servers launched! Check the server windows for status.
echo.
echo Test Account: test / test123
echo.
pause
