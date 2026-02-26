# Murim MMORPG Server Launcher
# 使用 PowerShell 启动所有服务器

$ErrorActionPreference = "Stop"

# 设置依赖库路径
$vcpkgBin = "D:\vcpkg\installed\x64-windows\bin"
$pgBin = "C:\Program Files\PostgreSQL\17\bin"
$env:PATH = "$env:PATH;$vcpkgBin;$pgBin"

$serverDir = "D:\moxiang-server"

Write-Host ""
Write-Host "========================================================"  -ForegroundColor Cyan
Write-Host "       Murim MMORPG 服务器启动" -ForegroundColor Cyan
Write-Host "========================================================"  -ForegroundColor Cyan
Write-Host ""

# 定义服务器
$servers = @(
    @{ Name = "DistributeServer"; Port = 8000; Exe = "build\distribute-server\Release\murim_distributeserver.exe"; Color = "Blue" },
    @{ Name = "AgentServer"; Port = 7000; Exe = "build\agent-server\Release\murim_agentserver.exe"; Color = "Yellow" },
    @{ Name = "MurimNetServer"; Port = 8500; Exe = "build\murimnet-server\Release\murim_murimnetserver.exe"; Color = "Green" },
    @{ Name = "MapServer"; Port = 9001; Exe = "build\map-server\Release\murim_mapserver.exe"; Color = "Red" }
)

# 启动每个服务器
for ($i = 0; $i -lt $servers.Count; $i++) {
    $server = $servers[$i]
    $num = $i + 1

    Write-Host "[$num/4] 启动 $($server.Name) (端口 $($server.Port))..." -ForegroundColor $server.Color

    $exePath = Join-Path $serverDir $server.Exe

    if (Test-Path $exePath) {
        # 在新窗口中启动服务器
        Start-Process cmd -ArgumentList "/k", "title Murim-$($server.Name) && cd /d `""$serverDir`"" && `"">$($server.Name) 启动...`"" && $exePath && pause" -WindowStyle Normal
        Start-Sleep -Milliseconds 1000
        Write-Host "      ✓ $($server.Name) 已启动" -ForegroundColor Green
    } else {
        Write-Host "      ✗ 找不到可执行文件: $exePath" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "========================================================"  -ForegroundColor Cyan
Write-Host " 所有服务器启动完成！" -ForegroundColor Green
Write-Host "========================================================"  -ForegroundColor Cyan
Write-Host ""
Write-Host " 测试账号:" -ForegroundColor White
Write-Host "   用户名: test" -ForegroundColor Gray
Write-Host "   密码: test123" -ForegroundColor Gray
Write-Host ""
Write-Host " 按任意键查看服务器状态..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# 显示状态
while ($true) {
    Clear-Host
    Write-Host ""
    Write-Host "========================================================"  -ForegroundColor Cyan
    Write-Host "       Murim MMORPG 服务器状态" -ForegroundColor Cyan
    Write-Host "========================================================"  -ForegroundColor Cyan
    Write-Host ""

    $running = 0
    foreach ($server in $servers) {
        $process = Get-Process -Name ($server.Exe -replace '.*\\', '' -replace '\.exe$', '') -ErrorAction SilentlyContinue
        if ($process) {
            Write-Host " [RUNNING] $($server.Name) (端口 $($server.Port))" -ForegroundColor Green
            $running++
        } else {
            Write-Host " [STOPPED] $($server.Name) (端口 $($server.Port))" -ForegroundColor Red
        }
    }

    Write-Host ""
    Write-Host " 运行中: $running / $($servers.Count) 个服务器" -ForegroundColor White
    Write-Host ""
    Write-Host " 按 'Q' 退出, 'R' 刷新状态..." -ForegroundColor Gray
    Write-Host ""

    $key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    if ($key.Character -eq 'q' -or $key.Character -eq 'Q') {
        break
    }
}
