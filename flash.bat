@echo off
:: --- 1. SETTINGS ---
set DEVICE=GD32F470VG
set JLINK_PATH=""D:\software\jlink\JLink_V916a\JLink.exe""

:: --- 2. CHECK FILE ---
if not exist "Output\GDf4_boot.hex" (
    echo ERROR: Hex file not found!
    pause
    exit /b
)

:: --- 3. CREATE JLINK SCRIPT ---
:: Using simple filenames to avoid redirection errors
echo r > tmp_flash.jlink
echo h >> tmp_flash.jlink
echo loadfile Output\GDf4_boot.hex >> tmp_flash.jlink
echo r >> tmp_flash.jlink
echo g >> tmp_flash.jlink
echo q >> tmp_flash.jlink

:: --- 4. EXECUTE ---
%JLINK_PATH% -device %DEVICE% -if SWD -speed 4000 -autoconnect 1 -CommanderScript tmp_flash.jlink

:: --- 5. CLEAN ---
del tmp_flash.jlink

echo Flash Process Finished.
pause