/*!
    \file    main.h
    \brief   APP main header

    \version 2026-02-26, V1.0.0
*/

#ifndef __MAIN_H
#define __MAIN_H

#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "delay.h"
#include "iic.h"
#include "AT24c256.h"
#include "spi.h"
#include "gd25q40e.h"
#include "fmc.h"
#include "esp8266.h"
#include "ota_types.h"
#include "mqtt.h"

/* APP 运行在 0x08010000，由 Bootloader 跳转过来 */
#define APP_ADDR 0x08010000

#endif /* __MAIN_H */
