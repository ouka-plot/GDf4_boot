#ifndef  __iic_h_
#define __iic_h_

#include "main.h"









#define IIC_SDA_H  do{gpio_bit_set(GPIOE,GPIO_PIN_0);}while(0)
#define IIC_SDA_L  do{gpio_bit_reset(GPIOE,GPIO_PIN_0);}while(0)
#define IIC_SCL_H  do{gpio_bit_set(GPIOE,GPIO_PIN_1);}while(0)
#define IIC_SCL_L  do{gpio_bit_reset(GPIOE,GPIO_PIN_1);}while(0)
#define IIC_SDA_READ  gpio_input_bit_get(GPIOE,GPIO_PIN_0)



void iic_init(void);
void iic_start(void);
void iic_stop(void);
void iic_send_byte(uint8_t data);
uint8_t iic_wait_ack(int16_t timeout);
uint8_t iic_recv_byte(uint8_t ack);
#endif




