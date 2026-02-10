#ifndef  __delay_h_
#define  __delay_h_

#include "gd32f4xx.h"
#include "gd32f4xx_it.h"

void delay_init(void);
void delay_us(uint32_t num);
void delay_ms(uint32_t num);


#endif


