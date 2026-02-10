/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-01-15 14:24:54
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-02-10 18:10:56
 * @FilePath: \GDf4_boot\User\main.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*!
    \file    main.h
    \brief   the header file of main

    \version 2025-07-31, V3.3.2, firmware for GD32F4xx
*/

/*
    Copyright (c) 2025, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#ifndef __MAIN_H
#define __MAIN_H


#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>
#include "usart.h"
#include "delay.h"
#include "iic.h"
#include "AT24c256.h"
#include "spi.h"
#include "gd25q40e.h"
#include "fmc.h"
#include "boot.h"


//BANK 0 1024KB
#define FLASH_START_ADDR 0X08000000   //flash起始地址
#define BOOT_SECTOR_SIZE 0X4000   //BOOT扇区大小16kB sector 16 16 16 16 // 0 1 2 3 
#define BOOT_SECTOR_NUM 4   //BOOT扇区数量
#define BOOT_SECTOR_START_ADDR 0X08000000   //BOOT扇区起始地址 


//APP_SECTOR_SIZE 0X10000    //APP扇区大小 sector 64  128 128 128 128 ... //4 5 6 ...
#define APP_ADDR 0X08010000   //用户程序起始地址，必须是扇区的整数倍
#define APP_SECTOR_NUM 8   //APP扇区数量  64kB + 128kB*7 = 960kB // 4 5 6 7 8 9 10 11



#endif /* __MAIN_H */


