#ifndef  __AT24c02_h_
#define  __AT24c02_h_

#include "main.h"

#define AT24_WADDR 0XA0
#define AT24_RADDR 0XA1


uint8_t AT24_write_page(uint16_t addr,uint8_t *databuf,uint16_t datalen );
uint8_t AT24_read_page(uint16_t addr,uint8_t *databuf,uint16_t datalen );
uint8_t AT24_write_byte(uint8_t addr,uint8_t data);


#endif


