# GDf4_boot

## Overview
This is a bare-metal embedded firmware project for the **GD32F4xx** series microcontroller (ARM Cortex-M4). The project demonstrates peripheral initialization and usage, including USART communication, SPI Flash memory, I2C EEPROM, and internal Flash memory operations.

## Hardware
- **MCU**: GigaDevice GD32F4xx (ARM Cortex-M4)
- **External Flash**: GD25Q40E (4Mbit SPI Flash)
- **EEPROM**: AT24C02 (2Kbit I2C EEPROM)
- **Communication**: USART0 (921600 baud)
- **Debugger**: SWD/JTAG interface

## Features
- **USART Communication**: Serial port with printf support for debugging (PA9/PA10, 921600 baud)
- **SPI Flash**: Read/write operations for GD25Q40E external Flash memory
- **I2C EEPROM**: Support for AT24C02 EEPROM access
- **Internal Flash**: Flash Memory Controller (FMC) operations for reading/writing/erasing internal Flash sectors
- **System Timing**: SysTick and delay functions for precise timing control
- **Peripheral Drivers**: Modular BSP (Board Support Package) architecture

## Directory Structure
```
GDf4_boot/
│
├── Project/           # Keil MDK project files
│   ├── Boot.uvprojx  # Keil μVision project
│   └── Boot.uvoptx   # Project options
│
├── User/             # Application layer
│   ├── main.c        # Main application entry point
│   ├── main.h        # Main header file
│   └── RTE_Components.h
│
├── Driver/           # Hardware drivers and libraries
│   ├── BSP/          # Board Support Package
│   │   ├── Source/   # Driver implementations
│   │   │   ├── usart.c      # USART driver
│   │   │   ├── spi.c        # SPI driver
│   │   │   ├── iic.c        # I2C driver
│   │   │   ├── gd25q40e.c   # SPI Flash driver
│   │   │   ├── AT24c02.c    # I2C EEPROM driver
│   │   │   └── fmc.c        # Flash Memory Controller
│   │   └── Include/  # Driver headers
│   │
│   ├── LIB/          # GD32F4xx Standard Peripheral Library
│   │   ├── Source/   # Library source files
│   │   └── Include/  # Library headers
│   │
│   ├── CMSIS/        # ARM CMSIS (Cortex Microcontroller Software Interface Standard)
│   │   ├── Source/   # CMSIS implementations
│   │   └── Include/  # CMSIS headers (gd32f4xx.h, system_gd32f4xx.h, core_cm4.h)
│   │
│   └── SYSTEM/       # System-level drivers
│       ├── Source/   # System implementations (systick.c, delay.c, interrupt handlers)
│       └── Include/  # System headers
│
└── README.md         # This file
```

## Building the Project

### Prerequisites
- **Keil MDK-ARM** (μVision 5.06 or later)
- **ARM Compiler 5** or **ARM Compiler 6**
- **GD32F4xx Device Family Pack** installed in Keil

### Build Steps
1. Open `Project/Boot.uvprojx` in Keil μVision
2. Select the target configuration (Target 1)
3. Build the project: **Project → Build Target** (or press F7)
4. The output files (HEX/BIN) will be generated in the build output directory

### Flash Programming
1. Connect your debugger (J-Link, ST-Link, or compatible)
2. In Keil μVision: **Flash → Download** (or press F8)
3. The firmware will be programmed to the GD32F4xx MCU

## Usage

### Serial Communication
Connect a USB-to-Serial adapter to USART0:
- **TX**: PA9
- **RX**: PA10
- **Baud Rate**: 921600
- **Data Format**: 8N1

Use a serial terminal (like PuTTY, TeraTerm, or screen) to view debug output:
```bash
screen /dev/ttyUSB0 921600
```

### Main Application Functions
The main application in `User/main.c` demonstrates:
- USART initialization and printf debugging
- SPI Flash operations (erase, write, read)
- I2C EEPROM operations
- Internal Flash memory operations

### SPI Flash Operations (GD25Q40E)
```c
// Erase 64KB block
gd25_erase64k(0);

// Write page (256 bytes)
uint8_t buf[256] = {0};
gd25_pagewrite(buf, page_num);

// Read data
uint8_t bufrecv[256] = {0};
gd25_read(bufrecv, address, length);
```

### Internal Flash Operations
```c
// Note: Sectors 0-3 contain program code (DO NOT ERASE!)
// Use Sector 4 or higher for data storage

// Erase sector 4 (starts at 0x08010000)
gd32_eraseflash(4, 1);

// Write data (256 words = 1KB)
uint32_t wbuf[256];
gd32_writeflash(0x08010000, wbuf, 256);

// Read data
uint32_t value = gd32_readflash(0x08010000);
```

## Peripheral Pins Configuration

### USART0
- **TX**: PA9 (AF7)
- **RX**: PA10 (AF7)

### SPI1 (for GD25Q40E Flash)
- **CS**: PB12 (GPIO)
- **SCK**: PB13 (AF5)
- **MISO**: PB14 (AF5)
- **MOSI**: PB15 (AF5)

### I2C (for AT24C02 EEPROM)
- Configured via I2C peripheral driver

## Development Notes
- The project uses GigaDevice's GD32F4xx Standard Peripheral Library (V3.3.2)
- Based on ARM CMSIS standard for portability
- Flash sectors 0-3 (16KB each) are reserved for program code
- Sector 4 (64KB, starting at 0x08010000) and above can be used for data storage

## License
This project contains code with the following licenses:
- **GigaDevice SDK**: BSD 3-Clause License (Copyright © 2025, GigaDevice Semiconductor Inc.)
- **Application Code**: Check individual source files for specific license information

See individual source files for detailed copyright and license information.