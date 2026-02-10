/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-02-05 18:06:04
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-02-10 17:02:44
 * @FilePath: \GDf4_boot\Driver\BSP\Include\AT24c256.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef  __AT24c256_h_
#define  __AT24c256_h_  

#include "main.h"

#define AT24_WADDR 0XA0
#define AT24_RADDR 0XA1


uint8_t AT24_write_page(uint16_t addr,uint8_t *databuf,uint16_t datalen );
uint8_t AT24_read_page(uint16_t addr,uint8_t *databuf,uint16_t datalen );
uint8_t AT24_write_byte(uint8_t addr,uint8_t data);
void AT24_ReadOTAInfo(void);

#endif


