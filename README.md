# GDf4_boot — GD32F470 Bootloader + APP Monorepo

基于 **GD32F470VGT6** (Cortex-M4F, 240 MHz) 的 **Boot + APP 双固件**架构，采用 Monorepo 管理。

## 功能特性

### Bootloader (Boot)
- **串口 IAP**：USART0 Ymodem 协议下载固件
- **OTA 搬运**：检测 EEPROM 标志 → 从外部 Flash 搬运固件到内部 Flash
- **CLI 操作台**：上电 2s 内按 `w` 进入，支持擦除 / 备份 / 恢复 / 版本管理 / 网络配置
- **网络配置 CLI**：首次使用时通过串口输入 WiFi 和 MQTT 凭据，持久化到 EEPROM

### APP (应用)
- **MQTT OTA**：ESP8266 连接 WiFi + MQTT Broker，自动接收固件并写入外部 Flash
- **自动重启**：未配置网络凭据时提示用户并 10s 后自动重启进入 Boot CLI
- **业务逻辑**：主循环中运行用户业务代码

## 架构概览

```
┌────────────────────────────────────────────────────────────┐
│                    上电 / 复位                              │
└──────────────────────┬─────────────────────────────────────┘
                       ▼
┌──────────────────────────────────────────────────────────┐
│  Boot (0x08000000, 64KB)                                 │
│                                                          │
│  1. 初始化核心外设 (USART0, I2C, SysTick)                │
│  2. 读 EEPROM OTA 标志                                   │
│  3. 有标志? → OTA 搬运 (外部Flash→内部Flash) → 跳转 APP   │
│  4. 无标志? → 等待 2s CLI 窗口 (按'w'进入)               │
│  5. 无输入? → 直接跳转 APP                                │
└───────────────────────┬──────────────────────────────────┘
                        ▼
┌──────────────────────────────────────────────────────────┐
│  APP (0x08010000, 960KB)                                 │
│                                                          │
│  1. VTOR 重定位 → 初始化外设                              │
│  2. 读 EEPROM NetConfig                                  │
│  3. 无配置? → 提示 + 10s 自动重启到 Boot CLI              │
│  4. 有配置? → ESP8266_Init → WiFi → MQTT                 │
│  5. 主循环: mqtt_ota_poll() + 业务逻辑                    │
└──────────────────────────────────────────────────────────┘
```

## 首次使用流程

```
上电 → Boot → 跳转 APP → 检测无 NetConfig → 提示 → 10s 自动重启
                                                         ↓
上电 → Boot → 2s 内按'w' → CLI → 输入网络配置 → 重启
                                    ↓
                              8 MySSID,MyPass        (设置 WiFi)
                              9 mqtt.io,1883,dev01,user,pass  (设置 MQTT)
                              0                      (查看配置)
                              7                      (重启)
                                    ↓
上电 → Boot → 跳转 APP → 读到 NetConfig → ESP8266 连接 → MQTT OTA 就绪 ✓
```

## 硬件平台

| 项目 | 规格 |
|------|------|
| MCU | GD32F470VGT6 (Cortex-M4F @ 240 MHz) |
| Flash | 1024 KB 内部 + 4 Mbit GD25Q40E (SPI) |
| RAM | 192 KB SRAM + 64 KB TCMRAM |
| WiFi | ESP8266 (AT 固件, USART1 @ 115200) |
| EEPROM | AT24C256 (I2C 软件模拟) |
| 调试口 | USART0 @ 921600 (PA9/PA10) |
| 编译器 | arm-none-eabi-gcc 10.3.1 (Docker 容器) |

## 存储分区

### 内部 Flash (1024 KB)

```
┌────────────────────────────────┐
│  Boot (64 KB)                  │  0x0800_0000 - 0x0800_FFFF   Sector 0-3
├────────────────────────────────┤
│  APP  (960 KB)                 │  0x0801_0000 - 0x080F_FFFF   Sector 4-11
└────────────────────────────────┘
```

### 外部 SPI Flash (GD25Q40E, 512 KB)

```
┌────────────────────────────────┐
│ Page 0: OTA_Header (256 B)     │  OTA 描述信息 (magic + 段表)
├────────────────────────────────┤
│ Page 1+: 固件数据              │  hex-decoded 原始 bin
└────────────────────────────────┘
```

### EEPROM (AT24C256) 布局

| 地址 | 大小 | 内容 |
|------|------|------|
| `0x0000` | 8 B | `OTA_InfoCB` — boot_flag + header_addr |
| `0x0008` | 4 B | OTA 版本号 |
| `0x0040` | 156 B | `NetConfig` — WiFi/MQTT 凭据 (magic + ssid + pass + host + port + client_id + user + pass) |

## MQTT OTA 升级流程

```
╔══════════════════╗                          ╔══════════════════╗
║  MQTT Broker     ║                          ║  GD32F470 (APP)  ║
║  (服务器发布 OTA) ║ ─────── WiFi ─────────→ ║  ESP8266 AT      ║
╚══════════════════╝                          ╚══════════════════╝
                                                      │
                      ┌───────────────────────────────┘
                      ▼                                    ┌──── Boot ────────────┐
  ① 发布 "OTA:START:<size>"  →  解析起始帧，擦除外部 Flash
  ② 发布 hex 编码数据帧      →  解码写入 GD25Q40E (每 256B 一页)
  ③ 发布 "OTA:END"           →  构造 OTA_Header 写入第 0 页
                                 EEPROM 置 boot_flag
                                 软复位               ─────→  ④ 检测到 boot_flag
                                                              ⑤ 读取 OTA_Header
                                                              ⑥ 逐段搬运: 外部 Flash → 内部 Flash
                                                              ⑦ 清除 boot_flag，回写 EEPROM
                                                              ⑧ 跳转 APP
```

### OTA 协议格式

ESP8266 AT 固件收到 MQTT 推送后通过 USART1 上报 `+MQTTSUBRECV:0,"device/ota",<len>,<payload>`

| 帧类型 | payload 格式 | 说明 |
|--------|-------------|------|
| 起始帧 | `OTA:START:<total_size>` | 如 `OTA:START:12345` |
| 数据帧 | hex 编码的二进制固件分块 | 每帧 ≤ 1024B |
| 结束帧 | `OTA:END` | 触发写 Header + 重启 |

### OTA 数据结构 (ota_types.h, Boot + APP 共用)

| 结构体 | 大小 | 说明 |
|--------|------|------|
| `OTA_Segment` | 16 B | target_addr / data_len / ext_offset / crc32 |
| `OTA_Header` | 144 B | magic + seg_count + total_len + crc32 + segs[8] |
| `OTA_InfoCB` | 8 B | boot_flag + header_addr (存 EEPROM) |

## CLI 命令

上电后 2 秒内通过 USART0 (921600) 发送 `w` 进入命令行：

| 按键 | 功能 |
|------|------|
| `0` | 显示当前网络配置 |
| `1` | 擦除 APP 区 Flash (Sector 4-11) |
| `2` | 串口 Ymodem IAP 下载 |
| `3` | 设置 OTA 版本号 |
| `4` | 查询 OTA 版本号 |
| `5` | 内部 Flash → 外部 Flash（备份） |
| `6` | 外部 Flash → 内部 Flash（恢复） |
| `7` | 软件复位 |
| `8` | 设置 WiFi：`8 SSID,PASSWORD` |
| `9` | 设置 MQTT：`9 HOST,PORT,CLIENT_ID,USER,PASS` |
| `d` | 打印 APP 向量表前 32 字节 |
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

### 2. 编译 Boot

```powershell
# CMake 配置
docker run --rm -v "${PWD}:/app" gd32-env cmake -S boot -B build_boot -G Ninja

# 编译
docker run --rm -v "${PWD}:/app" gd32-env ninja -C build_boot
```

产物位于 `build_boot/`：`GDf4_boot.elf` / `.hex` / `.bin` / `.map`

### 3. 编译 APP

```powershell
# CMake 配置
docker run --rm -v "${PWD}:/app" gd32-env cmake -S app -B build_app -G Ninja

# 编译
docker run --rm -v "${PWD}:/app" gd32-env ninja -C build_app
```

产物位于 `build_app/`：`GDf4_app.elf` / `.hex` / `.bin` / `.map`

### 4. 烧录

```powershell
.\flash_boot.bat    # 烧录 Bootloader
.\flash_app.bat     # 烧录 APP
```

### VS Code 快捷任务

| 任务 | 说明 |
|------|------|
| 🚀 **一键构建烧录** | 编译 + J-Link 烧录（`Ctrl+Shift+B`） |
| 🔄 **完整重建烧录** | CMake 配置 → 编译 → 烧录 |
| **Clean Output** | 清理编译产物 |

### 调试

- **VS Code**: 按 `F5` 启动 Cortex-Debug (J-Link SWD)
- **SEGGER Ozone**: 打开 `.jdebug` 工程文件

## 引脚配置

| 外设 | 引脚 | 复用 | 备注 |
|------|------|------|------|
| USART0 TX | PA9 | AF7 | 调试串口 921600 |
| USART0 RX | PA10 | AF7 | |
| USART1 TX | PD5 | AF7 | ESP8266 115200 |
| USART1 RX | PD6 | AF7 | |
| SPI1 CS | PB12 | GPIO | GD25Q40E 片选 |
| SPI1 SCK | PB13 | AF5 | |
| SPI1 MISO | PB14 | AF5 | |
| SPI1 MOSI | PB15 | AF5 | |
| I2C SDA/SCL | — | GPIO | 软件模拟, AT24C256 |

## 目录结构

```
GDf4_boot/
├── Dockerfile                     # Docker 交叉编译环境 (Ubuntu 22.04 + arm-none-eabi-gcc 10.3)
├── flash_boot.bat                 # J-Link 烧录 Boot
├── flash_app.bat                  # J-Link 烧录 APP
├── README.md
│
├── cmake/
│   └── arm-none-eabi.cmake        # ★ 共享工具链文件 (Cortex-M4F)
│
├── boot/                          # ── Bootloader 固件 ──
│   ├── CMakeLists.txt             # Boot 构建脚本 (不编译 esp8266.c)
│   ├── main.c                     # 入口: 初始化 → OTA 搬运 → CLI → 跳转 APP
│   ├── main.h                     # Boot 头文件 (含 boot.h, net_config.h)
│   └── RTE_Components.h
│
├── app/                           # ── APP 固件 ──
│   ├── CMakeLists.txt             # APP 构建脚本 (不编译 boot.c)
│   ├── gd32f470_app.ld            # APP 链接脚本 (起始 0x08010000, 960KB)
│   ├── main.c                     # 入口: VTOR → NetConfig → ESP8266 → MQTT OTA 轮询
│   ├── main.h                     # APP 头文件 (含 esp8266.h, ota_types.h)
│   └── RTE_Components.h
│
├── shared/                        # ── 共享驱动 (Boot + APP 均引用) ──
│   ├── BSP/
│   │   ├── Include/
│   │   │   ├── boot.h             # Bootloader 核心声明 (CLI / 跳转 / OTA 搬运)
│   │   │   ├── esp8266.h          # ESP8266 AT 驱动 + MQTT OTA 状态机
│   │   │   ├── ota_types.h        # ★ OTA 数据结构 (OTA_Header / Segment / InfoCB)
│   │   │   ├── net_config.h       # ★ 网络配置结构体 + EEPROM 持久化
│   │   │   ├── usart.h            # USART0/1 驱动
│   │   │   ├── AT24c256.h         # AT24C256 EEPROM 驱动
│   │   │   ├── gd25q40e.h         # GD25Q40E SPI Flash 驱动
│   │   │   ├── spi.h / iic.h / fmc.h
│   │   │   └── ...
│   │   └── Source/
│   │       ├── boot.c             # ★ Bootloader 核心 (CLI / Ymodem / OTA 搬运 / 跳转)
│   │       ├── esp8266.c          # ESP8266 AT 指令 + MQTT OTA 状态机
│   │       ├── net_config.c       # ★ NetConfig EEPROM 读写
│   │       ├── AT24c256.c         # EEPROM 驱动 (含 ota_info/ota_header 全局变量)
│   │       ├── usart.c            # USART0/1 (DMA + 空闲中断 + 环形缓冲)
│   │       ├── gd25q40e.c / spi.c / iic.c / fmc.c
│   │       └── ...
│   ├── CMSIS/                     # ARM CMSIS + GD32F4xx 芯片头文件
│   ├── LIB/                       # GD32F4xx 标准外设库 V3.3.2
│   └── SYSTEM/                    # SysTick / DWT 延时 / 中断处理
│
├── Project/
│   ├── gd32f470xE_flash.ld        # Boot 链接脚本 (0x08000000, 全片 1024K)
│   └── startup_gd32f450_470.S     # 启动汇编 (Boot + APP 共用)
│
├── build_boot/                    # Boot 编译产物 (Docker 生成)
└── build_app/                     # APP 编译产物 (Docker 生成)
```

### Boot vs APP 编译差异

| | Boot (`boot/CMakeLists.txt`) | APP (`app/CMakeLists.txt`) |
|---|---|---|
| 项目名 | `GDf4_boot` | `GDf4_app` |
| 链接脚本 | `Project/gd32f470xE_flash.ld` (0x08000000) | `app/gd32f470_app.ld` (0x08010000) |
| 包含 `boot.c` | ✅ | ❌ |
| 包含 `esp8266.c` | ❌ | ✅ |
| 包含 `net_config.c` | ✅ | ✅ |

## 技术要点

- **Boot/APP 分离**: Boot 不依赖 ESP8266，只负责搬运 + CLI；WiFi/MQTT 全部在 APP 层
- **NetConfig 持久化**: 网络凭据存 EEPROM (0x0040)，156B packed 结构体，跨页写入
- **MQTT OTA 状态机**: `OTA_IDLE → OTA_RECEIVING → OTA_COMPLETE`，hex 解码 + 256B 页写入
- **ESP8266 错误处理**: `ESP8266_Init()` 返回 `esp8266_err_t` 枚举，无 `while(1)` 死循环
- **延时**: `delay_us()` 基于 DWT 周期计数器 (不占 SysTick)；`delay_ms()` 基于 SysTick 1 kHz 中断
- **串口接收**: USART0/1 DMA + 空闲中断，数据存入环形缓冲区，零拷贝解析
- **跳转安全**: 关中断 → 停 SysTick → 清 NVIC → 刷 Cache → deinit 外设 → 设 VTOR → 设 MSP → 跳转
- **编译优化**: `-O2 -ffunction-sections -fdata-sections` + `--gc-sections` 裁剪未用代码

## 许可证

- **GigaDevice SDK**: BSD 3-Clause (Copyright © 2025, GigaDevice Semiconductor Inc.)
- **应用代码**: 详见各源文件头部
