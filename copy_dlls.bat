@echo off
REM =====================================================
REM 复制所有必需的 DLL 到服务器目录
REM 此脚本将 vcpkg 和 PostgreSQL 的 DLL 复制到各个服务器的 Release 目录
REM =====================================================

cd /d D:\moxiang-server
echo ========================================
echo    复制服务器依赖 DLL
echo ========================================
echo.

set VCPKG_BIN=D:\vcpkg\installed\x64-windows\bin
set PG_BIN=C:\Program Files\PostgreSQL\17\bin

echo [1/2] 复制 vcpkg DLL...
for %%S in (
    fmt.dll
    spdlog.dll
    libprotobuf.dll
    libprotobuf-lite.dll
    abseil_dll.dll
) do (
    if exist "%VCPKG_BIN%\%%S" (
        for %%D in (
            build\distribute-server\Release
            build\agent-server\Release
            build\murimnet-server\Release
            build\map-server\Release
        ) do (
            copy /Y "%VCPKG_BIN%\%%S" "%%D\" >nul 2>&1
        )
        echo     [OK] %%S
    ) else (
        echo     [SKIP] %%S (not found)
    )
)

echo.
echo [2/2] 复制 PostgreSQL DLL...
for %%S in (
    libpq.dll
    libintl-9.dll
    libiconv-2.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    libwinpthread-1.dll
    libxml2.dll
    libxslt.dll
    libzstd.dll
    liblz4.dll
    zlib1.dll
) do (
    if exist "%PG_BIN%\%%S" (
        for %%D in (
            build\distribute-server\Release
            build\agent-server\Release
            build\murimnet-server\Release
            build\map-server\Release
        ) do (
            copy /Y "%PG_BIN%\%%S" "%%D\" >nul 2>&1
        )
        echo     [OK] %%S
    ) else (
        echo     [SKIP] %%S (not found)
    )
)

echo.
echo ========================================
echo    DLL 复制完成！
echo ========================================
echo.
pause
