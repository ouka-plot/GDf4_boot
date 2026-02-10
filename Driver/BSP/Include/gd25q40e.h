#ifndef  __gd25q40e_h_
#define  __gd25q40e_h_

#include "main.h"

#define GD25_CS_ENABLE      do{gpio_bit_reset(GPIOB,GPIO_PIN_12);}while(0);
#define GD25_CS_DISENABLE   do{gpio_bit_set(GPIOB,GPIO_PIN_12);}while(0);

void gd25q40e_init(void);

void gd25_waitbusy(void);

void gd25_enable(void);

void gd25_erase64k(uint8_t blocknum);

void gd25_pagewrite(uint8_t *buf,uint16_t pagenum);

void gd25_read(uint8_t *buf,uint32_t addr,uint32_t datalen);

#endif

