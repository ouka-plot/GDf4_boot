# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a GD32F470VGT6 (Cortex-M4F) embedded firmware monorepo containing:
- **Bootloader** (boot/): 64KB bootloader with OTA update capability, CLI console, and Ymodem IAP
- **Application** (app/): 960KB application with MQTT OTA, ESP8266 WiFi, and OneNET IoT integration
- **Shared libraries** (shared/): Common BSP drivers, CMSIS, and GD32F4xx peripheral libraries

The bootloader and application are separate binaries that share common driver code but are built independently.

## Build System

This project uses CMake + Ninja with Docker for cross-compilation.

### Building Boot

```powershell
# Configure
docker run --rm -v "${PWD}:/app" gd32-env cmake -S boot -B build_boot -G Ninja

# Build
docker run --rm -v "${PWD}:/app" gd32-env ninja -C build_boot
```

Output: `build_boot/GDf4_boot.{elf,hex,bin,map}`

### Building APP

```powershell
# Configure
docker run --rm -v "${PWD}:/app" gd32-env cmake -S app -B build_app -G Ninja

# Build
docker run --rm -v "${PWD}:/app" gd32-env ninja -C build_app
```

Output: `build_app/GDf4_app.{elf,hex,bin,map}`

### Flashing

```powershell
.\flash_boot.bat    # Flash bootloader via J-Link
.\flash_app.bat     # Flash application via J-Link
```

Requires J-Link software installed at path specified in .bat files.

### VS Code Build Tasks

VS Code tasks are configured for one-click build and flash:

- `Ctrl+Shift+B`: Build and flash (select Boot or APP)
- Tasks available: Build Boot, Build APP, Flash Boot, Flash APP, Clean Output

### Docker Environment Setup (First Time Only)

Place `gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2` in project root, then:

```powershell
docker build -t gd32-env .
```

## Architecture

### Memory Layout

**Internal Flash (1024 KB):**
- Boot: 0x08000000 - 0x0800FFFF (64 KB, Sectors 0-3)
- APP:  0x08010000 - 0x080FFFFF (960 KB, Sectors 4-11)

**External SPI Flash (GD25Q40E, 512 KB):**
- Page 0: OTA_Header (256 B) - OTA metadata
- Page 1+: Firmware binary data

**EEPROM (AT24C256):**
- 0x0000: OTA_InfoCB (8 B) - boot_flag + header_addr
- 0x0008: OTA version (4 B)
- 0x0040: NetConfig (156 B) - WiFi/MQTT credentials

### Boot Flow

1. Initialize hardware (USART0, I2C, SysTick, SPI Flash)
2. Read OTA flag from EEPROM
3. If OTA flag set: Apply update (external Flash → internal Flash), clear flag, jump to APP
4. Wait 2s for CLI entry (press 'w' on USART0 @ 921600 baud)
5. If no CLI entry: Jump to APP

### APP Flow

1. Relocate VTOR to 0x08010000
2. Read NetConfig from EEPROM
3. If no credentials: Prompt user, auto-reboot to Boot CLI after 10s
4. If credentials valid: Initialize ESP8266, connect WiFi, connect MQTT
5. Main loop: Poll MQTT OTA + business logic

### Shared Code Architecture

The `shared/` directory contains code used by both Boot and APP:

**BSP drivers (shared/BSP/):**
- `boot.c/.h`: Bootloader-specific (CLI, Ymodem, OTA apply, jump to APP) - **Boot only**
- `esp8266.c/.h`: ESP8266 AT commands + MQTT OTA state machine - **APP only**
- `mqtt.c/.h`: MQTT protocol handling - **APP only**
- `net_config.c/.h`: WiFi/MQTT credential management (EEPROM) - **Both**
- `ota_types.h`: OTA data structures (OTA_Header, OTA_Segment, OTA_InfoCB) - **Both**
- `usart.c/.h`, `spi.c/.h`, `iic.c/.h`, `fmc.c/.h`: Hardware drivers - **Both**
- `gd25q40e.c/.h`: External SPI Flash driver - **Both**
- `AT24c256.c/.h`: EEPROM driver - **Both**

**Key distinction:**
- Boot CMakeLists.txt includes `boot.c`, excludes `esp8266.c`
- APP CMakeLists.txt includes `esp8266.c`, excludes `boot.c`

## OTA Update Protocol

The APP receives MQTT messages via ESP8266 on topic `device/ota`:

1. **Start frame**: `OTA:START:<total_size>` - Initializes external Flash write
2. **Data frames**: Hex-encoded binary chunks (1024B each) - Written to external Flash
3. **End frame**: `OTA:END` - Writes OTA_Header to page 0, sets boot_flag in EEPROM, reboots

On next boot, bootloader detects flag and copies firmware from external Flash to internal Flash APP region.

## Bootloader CLI Commands

Press 'w' within 2s of boot on USART0 (921600 baud) to enter CLI:

- `0`: Show current configuration
- `1`: Erase APP Flash (Sectors 4-11)
- `2`: Ymodem IAP upload
- `3`: Reset OTA version
- `4`: Query OTA version
- `5`: Backup (internal → external Flash)
- `6`: Restore (external → internal Flash)
- `7`: System reset
- `8 SSID,PASSWORD`: Configure WiFi
- `9 HOST,PORT,CLIENT_ID,USER,PASS`: Configure MQTT
- `d`: Dump first 32 bytes of APP
- `h`: Show help

## Hardware Configuration

**MCU:** GD32F470VGT6 (Cortex-M4F @ 240 MHz, 1024KB Flash, 192KB SRAM)

**Peripherals:**
- USART0 (PA9/PA10, AF7): Debug console @ 921600 baud
- USART1 (PD5/PD6, AF7): ESP8266 @ 115200 baud
- SPI1 (PB12-15): GD25Q40E external Flash
- I2C (software): AT24C256 EEPROM

## Important Implementation Details

- **VTOR relocation**: APP must set `SCB->VTOR = 0x08010000` before using interrupts
- **Jump safety**: Before jumping to APP, Boot disables interrupts, stops SysTick, clears NVIC, flushes cache, deinitializes peripherals, sets MSP, then jumps
- **Timing**: `delay_us()` uses DWT cycle counter (non-blocking), `delay_ms()` uses SysTick 1kHz interrupt
- **UART buffering**: USART0/1 use DMA + circular interrupt buffers for non-blocking I/O
- **Linker scripts**: Boot uses `Project/gd32f470xE_flash.ld` (0x08000000), APP uses `app/gd32f470_app.ld` (0x08010000)
- **Startup file**: Both use `Project/startup_gd32f450_470.S`
- **Optimization**: `-O2 -ffunction-sections -fdata-sections` with `--gc-sections` for size reduction

## Code Modification Guidelines

- When modifying shared BSP code, verify impact on both Boot and APP builds
- Boot binary must stay under 64KB (check .map file)
- APP starts at 0x08010000 - never change this without updating both linker scripts and Boot jump address
- OTA data structures in `ota_types.h` are shared - changes affect both Boot and APP
- NetConfig structure is persisted in EEPROM - structure changes require EEPROM format migration
- ESP8266 initialization returns `esp8266_err_t` enum - APP should handle errors gracefully
- MQTT OTA state machine in `esp8266.c` uses states: OTA_IDLE, OTA_RECEIVING, OTA_COMPLETE

### EEPROM Data Handling (CRITICAL)

When reading EEPROM data, always check for erased state (`0xFF`):

```c
// WRONG - treats 0xFF as valid data
if (net_cfg.mqtt_pass[0] != '\0') { ... }

// CORRECT - checks both null terminator and erased state
if (net_cfg.mqtt_pass[0] != '\0' && net_cfg.mqtt_pass[0] != 0xFF) { ... }
```

- Erased EEPROM bytes are `0xFF`, not `'\0'` (0x00)
- `strlen()` on uninitialized EEPROM will return incorrect lengths
- Always validate magic bytes before trusting EEPROM data
- See BUGFIX_REPORT.md for detailed case study

### OneNET IoT Platform Integration

The APP uses hardcoded credentials in `shared/BSP/Include/net_config.h`:

```c
#define DeviceName "device"
#define ProductID "0qK3k8n0M2"
#define DeviceKey "ME9KN21xNUd5UkNpN2M2U3ZFQmNJN3hkUk9CVkpqNHo="
#define UNIX "1893456000"
#define IPADDR "mqtts.heclouds.com"
#define PORTNUMBER "1883"
```

Authentication flow:

1. Base64 decode DeviceKey
2. Build signature string: `<et>\nsha1\n<res>\n2018-10-31`
3. HMAC-SHA1 sign with decoded key
4. Base64 encode signature
5. URL encode res and sign
6. Build token: `version=2018-10-31&res=<resURL>&et=<et>&method=sha1&sign=<signURL>`

Token generation is in `password_Init()` in `shared/BSP/Source/net_config.c`

## File Encoding Standard

All source files must use UTF-8 encoding:

- Check file encoding: `file -i filename.c` (should show `charset=utf-8`)
- Convert GBK to UTF-8: `iconv -f GBK -t UTF-8 input.c > output.c`
- CMake compiler flags already set: `-finput-charset=UTF-8 -fexec-charset=GBK`
- Some files (`app/main.c`, `shared/BSP/Source/net_config.c`, `shared/BSP/Source/esp8266.c`) have corrupted UTF-8 comments - see ENCODING_FIX_SUMMARY.md

## Debugging

- VS Code: Press F5 for Cortex-Debug (J-Link SWD)
- SEGGER Ozone: Use .jdebug project files
- Serial console: USART0 @ 921600 baud for debug output
