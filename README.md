# GDf4_boot — GD32F470 Bootloader

基于 **GD32F470** (Cortex-M4F @ 240 MHz) 的串口 IAP + OTA Bootloader，使用 Docker 容器化交叉编译。

## 功能特性

| 功能 | 说明 |
|------|------|
| **串口 IAP** | USART0 + Ymodem 协议，将固件直接烧写到内部 Flash APP 区 |
| **OTA 升级** | APP 将固件包存入外部 SPI Flash → 设置 EEPROM 标志 → 重启后 Bootloader 自动搬运 |
| **备份 / 恢复** | 内部 Flash APP 区 ↔ 外部 SPI Flash 双向搬运 |
| **串口 CLI** | 上电 2 s 内按 `w` 进入命令行，支持擦除、下载、备份、恢复、查询等 |
| **安全跳转** | MSP 校验 → NVIC 清理 → I/D-Cache 刷新 → 反初始化外设 → 跳转 APP |

## 硬件平台

| 组件 | 型号 / 参数 |
|------|-------------|
| MCU | GD32F470VG — Cortex-M4F, 1024 KB Flash (Bank0), 192 KB SRAM + 64 KB TCMRAM |
| 外部 Flash | GD25Q40E — 4 Mbit SPI Flash（存储 OTA 固件包） |
| EEPROM | AT24C256 — 256 Kbit I2C EEPROM（存储 OTA 升级标志） |
| 调试器 | J-Link (SWD) |
| 串口 | USART0 — PA9(TX) / PA10(RX)，921600 baud，8N1 |

## Flash 分区

```
  0x08000000 ┌────────────────────────┐
             │  Bootloader (64 KB)    │  Sector 0-3  (16 KB × 4)
  0x08010000 ├────────────────────────┤
             │                        │  Sector 4    (64 KB)
             │    APP 区 (960 KB)     │  Sector 5-11 (128 KB × 7)
  0x08100000 └────────────────────────┘

  外部 SPI Flash (GD25Q40E, 512 KB):
  0x00000000 ┌────────────────────────┐
             │  OTA Header (144 B)    │  magic + 段描述
             ├────────────────────────┤
             │  固件数据              │  按段存放
             └────────────────────────┘
```

## OTA 升级流程

```
  APP 端                          Bootloader 端
  ─────                           ──────────
  ① 接收固件包，写入 GD25Q40E
  ② 构造 OTA_Header 写入地址 0
  ③ EEPROM 置 boot_flag  ──────→  ④ 重启后检测到 boot_flag
                                   ⑤ 从外部 Flash 读取 OTA_Header
                                   ⑥ 逐段搬运: 外部 Flash → 内部 Flash
                                   ⑦ 清除 boot_flag，回写 EEPROM
                                   ⑧ 跳转 APP
```

## CLI 命令

上电后 2 秒内通过串口发送 `w` 进入命令行：

| 按键 | 功能 |
|------|------|
| `1` | 擦除 APP 区 Flash (Sector 4-11) |
| `2` | 串口 Ymodem IAP 下载 |
| `3` | 设置 OTA 版本号 |
| `4` | 查询 OTA 版本号 |
| `5` | 内部 Flash → 外部 Flash（备份） |
| `6` | 外部 Flash → 内部 Flash（恢复） |
| `7` | 软件复位 |
| `8` | 打印 APP 向量表前 32 字节 |
| `h` | 显示帮助 |

## 构建 & 烧录

### 环境要求

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) (Windows / macOS / Linux)
- [J-Link Software](https://www.segger.com/downloads/jlink/)（烧录 & 调试）

### 1. 构建 Docker 镜像（仅首次）

将 `gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2` 放在项目根目录：

```powershell
docker build -t gd32-env .
```

### 2. 编译固件

```powershell
# CMake 配置（仅首次 / CMakeLists.txt 变更后）
docker run --rm -v "${PWD}:/app" gd32-env cmake -BOutput -GNinja

# 编译
docker run --rm -v "${PWD}:/app" gd32-env ninja -C Output
```

编译产物位于 `Output/`：

| 文件 | 用途 |
|------|------|
| `GDf4_boot.elf` | 调试用，含完整符号 |
| `GDf4_boot.hex` | J-Link 烧录用 |
| `GDf4_boot.bin` | 原始二进制 |
| `GDf4_boot.map` | 内存映射 |

### 3. 烧录

```powershell
.\flash.bat
```

### VS Code 快捷任务

| 任务 | 说明 |
|------|------|
|  **一键构建烧录** | 编译 + J-Link 烧录（`Ctrl+Shift+B`） |
|  **完整重建烧录** | CMake 配置 → 编译 → 烧录 |
| **Clean Output** | 清理编译产物 |

### 调试

- **VS Code**: 按 `F5` 启动 Cortex-Debug (J-Link SWD)
- **SEGGER Ozone**: 打开 `GDf4_boot.jdebug`（已配置 Docker `/app` → 本地路径映射）

## 引脚配置

| 外设 | 引脚 | 复用 |
|------|------|------|
| USART0 TX | PA9 | AF7 |
| USART0 RX | PA10 | AF7 |
| SPI1 CS | PB12 | GPIO |
| SPI1 SCK | PB13 | AF5 |
| SPI1 MISO | PB14 | AF5 |
| SPI1 MOSI | PB15 | AF5 |
| I2C (软件) | — | GPIO 模拟 |

## 目录结构

```
GDf4_boot/
├── CMakeLists.txt              # CMake 构建脚本
├── Dockerfile                  # Docker 交叉编译环境
├── flash.bat                   # J-Link 一键烧录脚本
├── GDf4_boot.jdebug            # SEGGER Ozone 调试工程
│
├── User/                       # 应用层
│   ├── main.c                  # 入口: 初始化 → bootloader_branch()
│   └── main.h                  # Flash 分区宏 (APP_ADDR 等)
│
├── Driver/
│   ├── BSP/                    # 板级驱动 (Board Support Package)
│   │   ├── Source/
│   │   │   ├── boot.c          # ★ Bootloader 核心 (CLI / Ymodem / OTA / 跳转)
│   │   │   ├── usart.c         # USART0 (DMA 接收 + 环形缓冲)
│   │   │   ├── spi.c           # SPI1 驱动
│   │   │   ├── iic.c           # 软件 I2C
│   │   │   ├── gd25q40e.c      # GD25Q40E SPI Flash
│   │   │   ├── AT24c256.c      # AT24C256 EEPROM
│   │   │   └── fmc.c           # 内部 Flash 擦写
│   │   └── Include/
│   │
│   ├── CMSIS/                  # ARM CMSIS + GD32F4xx 芯片头文件
│   ├── LIB/                    # GD32F4xx 标准外设库 V3.3.2
│   └── SYSTEM/                 # SysTick / DWT 延时 / 中断处理
│
├── Project/
│   ├── gd32f470xE_flash.ld     # 链接脚本 (1024K Flash / 192K RAM)
│   └── startup_gd32f450_470.S  # 启动汇编
│
└── Output/                     # 编译产物 (Docker 生成)
```

## 技术要点

- **延时**: `delay_us()` 基于 DWT 周期计数器 (不占 SysTick)；`delay_ms()` / `delay_1ms()` 基于 SysTick 1 kHz 中断
- **串口接收**: USART0 DMA + 空闲中断，数据存入环形缓冲区，零拷贝解析
- **跳转安全**: 关中断 → 停 SysTick → 清 NVIC → 刷 Cache → deinit 外设 → 设 VTOR → 设 MSP → 跳转
- **编译优化**: `-O2 -ffunction-sections -fdata-sections` + `--gc-sections` 裁剪未用代码

## 许可证

- **GigaDevice SDK**: BSD 3-Clause (Copyright © 2025, GigaDevice Semiconductor Inc.)
- **应用代码**: 详见各源文件头部
