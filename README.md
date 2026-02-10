# GDf4_boot

## Overview
This project involves the creation of a bootloader using C and Assembly language. The bootloader is essential for initializing hardware and loading the operating system into memory.

## Features
- Supports basic hardware initialization.
- Loads the kernel from a specified location in the storage.
- Implements essential interrupt handling.

## Directory Structure
```
GDF4_BOOT/
│
├── src/        # Source files
│   ├── main.c  # Main bootloader entry point
│   ├── loader.c # Kernel loader function
│   └── init.s   # Assembly initializer
│
├── include/    # Header files
│   └── boot.h   # Bootloader definitions
│
└── Makefile    # Build script
```

## Building the Bootloader
To compile the bootloader, use the `Makefile` provided in the root directory. Ensure you have `gcc` and `nasm` installed.

```bash
make
```

## Usage
1. Write the bootloader to a disk or USB drive.
2. Boot the machine from the device containing the bootloader.
3. The bootloader initializes the system and loads the kernel.

## License
This project is licensed under the MIT License.