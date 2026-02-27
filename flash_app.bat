@echo off
:: ===== Flash APP =====
set DEVICE=GD32F470VG
set JLINK_PATH=""D:\software\jlink\JLink_V916a\JLink.exe""

if not exist "build_app\GDf4_app.hex" (
    echo ERROR: build_app\GDf4_app.hex not found! Build app first.
    pause
    exit /b
)

echo r > tmp_flash.jlink
echo h >> tmp_flash.jlink
echo loadfile build_app\GDf4_app.hex >> tmp_flash.jlink
echo r >> tmp_flash.jlink
echo g >> tmp_flash.jlink
echo q >> tmp_flash.jlink

%JLINK_PATH% -device %DEVICE% -if SWD -speed 4000 -autoconnect 1 -CommanderScript tmp_flash.jlink
del tmp_flash.jlink
echo [APP] Flash done.
