#ifndef  __spi_h_
#define  __spi_h_

#include "main.h"

#define GD25_CS_ENABLE      do{gpio_bit_reset(GPIOB,GPIO_PIN_12);}while(0);
#define GD25_CS_DISENABLE   do{gpio_bit_set(GPIOB,GPIO_PIN_12);}while(0);


void spi1_init(void);
uint16_t spi1_WR(uint16_t data);
void spi1_Write(uint16_t *buf,uint16_t buflen);
void spi1_WriteBytes(uint8_t *buf,uint16_t buflen);
void spi1_Read(uint16_t *buf,uint16_t buflen);
void spi1_ReadBytes(uint8_t *buf,uint16_t buflen);

#endif

