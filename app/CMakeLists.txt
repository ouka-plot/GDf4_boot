cmake_minimum_required(VERSION 3.15)

# 锟斤拷锟斤拷 compile_commands.json 锟斤拷 IntelliSense 使锟斤拷
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 锟斤拷锟斤拷锟斤拷锟侥硷拷锟斤拷锟斤拷锟斤拷锟斤拷 project() 之前锟斤拷
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/arm-none-eabi.cmake")

# -------------------------------------------------------------------------
# 1. 锟斤拷目锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
# -------------------------------------------------------------------------
project(GDf4_app C ASM)

# -------------------------------------------------------------------------
# 2. GD32F470 硬锟斤拷锟斤拷锟斤拷 (Cortex-M4F)
# -------------------------------------------------------------------------
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

add_definitions(-DGD32F470)
add_definitions(-DUSE_STDPERIPH_DRIVER)
add_definitions(-DUSE_MQTT_TOKEN)

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
# 3. 锟斤拷锟接脚憋拷锟斤拷选锟斤拷
# -------------------------------------------------------------------------
# APP 锟斤拷锟接脚憋拷锟斤拷锟斤拷始锟斤拷址 0x08010000, 960K
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/gd32f470_app.ld")

add_link_options(
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--gc-sections
    -specs=nosys.specs
)

# -------------------------------------------------------------------------
# 4. 头锟侥硷拷锟斤拷锟斤拷路锟斤拷
# -------------------------------------------------------------------------
set(SHARED_DIR "${CMAKE_SOURCE_DIR}/../shared")

include_directories(
    ${CMAKE_SOURCE_DIR}            # app/ 锟铰碉拷 main.h
    ${SHARED_DIR}/CMSIS/Include
    ${SHARED_DIR}/SYSTEM/Include
    ${SHARED_DIR}/BSP/Include
    ${SHARED_DIR}/LIB/Include
    ${CMAKE_SOURCE_DIR}/../HMAC/inc
)

# -------------------------------------------------------------------------
# 5. 锟斤拷锟斤拷源锟侥硷拷
# -------------------------------------------------------------------------
# APP 锟皆硷拷锟斤拷源锟斤拷
file(GLOB APP_SOURCES "${CMAKE_SOURCE_DIR}/*.c")

# 锟斤拷锟斤拷锟斤拷锟斤拷源锟斤拷 锟斤拷 锟斤拷锟斤拷选锟今，憋拷锟斤拷锟斤拷锟�1锟�7 Boot 专锟叫达拷锟斤拷
# 锟斤拷锟�1锟�7 APP 锟斤拷锟斤拷要某些锟斤拷锟斤拷锟斤拷锟斤拷锟皆达拷锟斤拷锟斤拷锟狡筹拷
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
    "${SHARED_DIR}/BSP/Source/esp8266.c"
    "${SHARED_DIR}/BSP/Source/net_config.c"
    "${SHARED_DIR}/BSP/Source/mqtt.c"
    "${SHARED_DIR}/BSP/Source/onenet_http_ota.c"
    "${SHARED_DIR}/HMAC/utils.c"
    # 注锟解：锟斤拷锟斤拷锟斤拷 boot.c锟斤拷锟斤拷锟斤拷 Bootloader 专锟矫碉拷
)

file(GLOB HMAC_SOURCES "${CMAKE_SOURCE_DIR}/../HMAC/scr/*.c")

# 锟斤拷锟斤拷锟斤拷锟�1锟�7
set(STARTUP_FILE "${CMAKE_SOURCE_DIR}/../Project/startup_gd32f450_470.S")

# -------------------------------------------------------------------------
# 6. 锟斤拷锟斤拷目锟斤拷锟侥硷拷
# -------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${SHARED_SOURCES}
    ${HMAC_SOURCES}
    ${STARTUP_FILE}
)

# 锟斤拷锟斤拷锟斤拷远锟斤拷锟斤拷锟�1锟�7 HEX, BIN 锟斤拷锟斤拷印锟节达拷占锟斤拷锟斤拷锟�1锟�7
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex  ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley ${PROJECT_NAME}.elf
    COMMENT "Generating Flash images and showing size..."
)
