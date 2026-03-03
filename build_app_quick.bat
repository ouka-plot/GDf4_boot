@echo off
REM 快速编译并烧录 APP 固件

echo ========================================
echo  GD32F470 APP 快速编译脚本
echo ========================================
echo.

echo [1/4] 清理旧构建...
if exist build_app (
    rmdir /s /q build_app
    echo 已删除 build_app 目录
)
echo.

echo [2/4] 配置 CMake...
docker run --rm -v "%CD%:/app" gd32-env cmake -S app -B build_app -G Ninja
if %errorlevel% neq 0 (
    echo [错误] CMake 配置失败！
    pause
    exit /b 1
)
echo.

echo [3/4] 编译固件...
docker run --rm -v "%CD%:/app" gd32-env ninja -C build_app
if %errorlevel% neq 0 (
    echo [错误] 编译失败！
    pause
    exit /b 1
)
echo.

echo [4/4] 检查输出文件...
if exist build_app\GDf4_app.bin (
    echo [成功] 固件编译完成！
    echo.
    dir build_app\GDf4_app.*
    echo.
    echo ========================================
    echo  固件已生成，可以烧录了
    echo ========================================
    echo.
    echo 运行以下命令烧录固件：
    echo   flash_app.bat
    echo.
) else (
    echo [错误] 未找到输出文件！
    pause
    exit /b 1
)

pause
