#!/bin/bash
# 编译验证脚本 (在Git Bash中运行)

echo "========================================"
echo " 编译 APP 固件"
echo "========================================"
echo ""

# 清理旧构建
echo "[1/3] 清理旧构建..."
rm -rf build_app
echo ""

# 配置
echo "[2/3] 配置 CMake..."
docker run --rm -v "$(pwd):/app" gd32-env cmake -S app -B build_app -G Ninja
if [ $? -ne 0 ]; then
    echo "[错误] CMake 配置失败！"
    exit 1
fi
echo ""

# 编译
echo "[3/3] 编译固件..."
docker run --rm -v "$(pwd):/app" gd32-env ninja -C build_app
if [ $? -ne 0 ]; then
    echo "[错误] 编译失败！"
    exit 1
fi
echo ""

echo "========================================"
echo " 编译成功！"
echo "========================================"
ls -lh build_app/GDf4_app.*
echo ""
echo "运行以下命令烧录固件："
echo "  ./flash_app.bat"
