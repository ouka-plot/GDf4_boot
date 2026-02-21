cmake_minimum_required(VERSION 3.15)

# 生成 compile_commands.json 供 IntelliSense 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------------------------------------------------------
# 1. 项目与语言设置
# -------------------------------------------------------------------------
project(GDf4_boot C ASM)

# 告诉 CMake 这是一个交叉编译项目
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 设置编译器（对应你 Docker 镜像里的路径）
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# -------------------------------------------------------------------------
# 2. GD32F470 硬件参数 (Cortex-M4F)
# -------------------------------------------------------------------------
# 使用列表方式设置，绝对不要加双引号传递，防止编译器解析为单个字符串
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)


add_definitions(-DGD32F470)
add_definitions(-DUSE_STDPERIPH_DRIVER)


# 编译选项：优化等级、调试信息、所有警告
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
# 确保你的 .ld 文件名和路径正确
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/Project/gd32f470xE_flash.ld")

add_link_options(
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--gc-sections
    -specs=nosys.specs
)

# -------------------------------------------------------------------------
# 4. 头文件包含路径 (根据你的 Driver 目录微调)
# -------------------------------------------------------------------------
# -------------------------------------------------------------------------
# 4. 自动搜寻并包含所有头文件路径 (彻底解决找不到头文件的问题)
# -------------------------------------------------------------------------
# 2. 别用自动搜寻了，手动列出这几个核心路径，确保大小写和文件夹一模一样！
include_directories(
    User
    Driver/CMSIS/Include
    Driver/SYSTEM/Include
    Driver/BSP/Include  # 刚才报错说找不到 AT24c256.h，确保这个路径写对了
    Driver/LIB/Include  # 确保 LIB 这三个字的大小写和硬盘上一致

)
# 去除重复的路径
list(REMOVE_DUPLICATES DYNAMIC_INCLUDE_DIRS)

# 将搜寻到的所有路径喂给编译器
include_directories(${DYNAMIC_INCLUDE_DIRS})
# -------------------------------------------------------------------------
# 5. 查找源文件
# -------------------------------------------------------------------------
# 递归搜索 User, Driver, Project 目录下的所有 C 文件和汇编文件
file(GLOB_RECURSE SOURCES 
    "User/*.c"
    "Driver/*.c"
    "Project/*.S"  # 包含启动文件
)
# 明确指定启动文件路径（根据你实际的文件名改）
set(STARTUP_FILE "${CMAKE_SOURCE_DIR}/Project/startup_gd32f450_470.S")


# -------------------------------------------------------------------------
# 6. 生成目标文件
# -------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf ${SOURCES})

# 编译后自动生成 HEX, BIN 并打印内存占用情况
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley ${PROJECT_NAME}.elf
    COMMENT "Generating Flash images and showing size..."
)