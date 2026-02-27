cmake_minimum_required(VERSION 3.15)

# 生成 compile_commands.json 供 IntelliSense 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 工具链文件（必须在 project() 之前）
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/arm-none-eabi.cmake")

# -------------------------------------------------------------------------
# 1. 项目与语言设置
# -------------------------------------------------------------------------
project(GDf4_boot C ASM)

# -------------------------------------------------------------------------
# 2. GD32F470 硬件参数 (Cortex-M4F)
# -------------------------------------------------------------------------
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

add_definitions(-DGD32F470)
add_definitions(-DUSE_STDPERIPH_DRIVER)

add_compile_options(
    ${CPU_FLAGS}
    -O2
    -g
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

# -------------------------------------------------------------------------
# 3. 链接脚本与选项
# -------------------------------------------------------------------------
# Boot 链接脚本：0x08000000, 整片 1024K（Boot 本身只用前 64K）
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/../Project/gd32f470xE_flash.ld")

add_link_options(
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--gc-sections
    -specs=nosys.specs
)

# -------------------------------------------------------------------------
# 4. 头文件包含路径
# -------------------------------------------------------------------------
set(SHARED_DIR "${CMAKE_SOURCE_DIR}/../shared")

include_directories(
    ${CMAKE_SOURCE_DIR}            # boot/ 下的 main.h
    ${SHARED_DIR}/CMSIS/Include
    ${SHARED_DIR}/SYSTEM/Include
    ${SHARED_DIR}/BSP/Include
    ${SHARED_DIR}/LIB/Include
)

# -------------------------------------------------------------------------
# 5. 查找源文件
# -------------------------------------------------------------------------
# Boot 自己的源码
file(GLOB BOOT_SOURCES "${CMAKE_SOURCE_DIR}/*.c")

# 共享驱动源码 — Boot 不需要 esp8266.c（MQTT OTA 在 APP 层实现）
file(GLOB_RECURSE SHARED_SOURCES
    "${SHARED_DIR}/CMSIS/Source/*.c"
    "${SHARED_DIR}/LIB/Source/*.c"
    "${SHARED_DIR}/SYSTEM/Source/*.c"
    "${SHARED_DIR}/BSP/Source/usart.c"
    "${SHARED_DIR}/BSP/Source/spi.c"
    "${SHARED_DIR}/BSP/Source/iic.c"
    "${SHARED_DIR}/BSP/Source/gd25q40e.c"
    "${SHARED_DIR}/BSP/Source/AT24c256.c"
    "${SHARED_DIR}/BSP/Source/fmc.c"
    "${SHARED_DIR}/BSP/Source/boot.c"
    "${SHARED_DIR}/BSP/Source/net_config.c"
    # esp8266.c 不编入 Boot
)

# 启动汇编
set(STARTUP_FILE "${CMAKE_SOURCE_DIR}/../Project/startup_gd32f450_470.S")

# -------------------------------------------------------------------------
# 6. 生成目标文件
# -------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
    ${BOOT_SOURCES}
    ${SHARED_SOURCES}
    ${STARTUP_FILE}
)

# 编译后自动生成 HEX, BIN 并打印内存占用情况
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex  ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley ${PROJECT_NAME}.elf
    COMMENT "Generating Flash images and showing size..."
)
