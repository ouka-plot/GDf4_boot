#include "spi.h"
#include "gd32f4xx_spi.h"

void spi1_init(){


	//rcu
	rcu_periph_clock_enable(RCU_SPI1);
	rcu_periph_clock_enable(RCU_GPIOB);
	/* set GPIO mode and output options */
	//SPI   PB 12 CS 13 SCK  14 MISO  15MOSI
	//13 15 OUT  

	gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_13|GPIO_PIN_15);
	gpio_af_set(GPIOB, GPIO_AF_5,GPIO_PIN_13|GPIO_PIN_15);
	
	gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_14);
	
	//spi init
	spi_i2s_deinit(SPI1);
	
	spi_parameter_struct spi1_struct;
	spi1_struct.device_mode          = SPI_MASTER;
    spi1_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;   //双线全双工
    spi1_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi1_struct.nss                  = SPI_NSS_SOFT;
    spi1_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    spi1_struct.prescale             = SPI_PSC_2; //30mhz
    spi1_struct.endian               = SPI_ENDIAN_MSB; //大端序
	
	 spi_init(SPI1,&spi1_struct);
	 
	 spi_enable(SPI1);
	 
}


//spi 全双工 收发同步
uint16_t spi1_WR(uint16_t data){
	uint16_t res=0;
	//没有达到flag就死等
	while(spi_i2s_flag_get(SPI1,SPI_FLAG_TBE)!=1);
	
	spi_i2s_data_transmit(SPI1,data);
	
	
	while(spi_i2s_flag_get(SPI1,SPI_FLAG_RBNE)!=1);
	
	return res=spi_i2s_data_receive(SPI1);


}


//发 uint8_t版本
void spi1_WriteBytes(uint8_t *buf,uint16_t buflen) {
	uint16_t i=0;
	for(i=0;i<buflen;i++)
	{
	  spi1_WR(buf[i]);

   }

}


//收 uint8_t版本
void spi1_ReadBytes(uint8_t *buf,uint16_t buflen) {

	uint16_t i=0;
	for(i=0;i<buflen;i++)
	{
	 buf[i]=spi1_WR(0xff);

   }

}

