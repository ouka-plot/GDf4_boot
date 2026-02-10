#include "gd25q40e.h"
#include "spi.h"
void gd25q40e_init(void){

	rcu_periph_clock_enable(RCU_GPIOB);
	rcu_periph_clock_enable(RCU_SPI1);
	
	//先初始化SPI
	spi1_init();
	
	//再单独配置CS脚
	gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_12);
	gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);
	
	GD25_CS_DISENABLE;
}

void gd25_waitbusy(void){
	uint16_t res=0;
	//cs
	do{
	GD25_CS_ENABLE;
	spi1_WR(0x05);
	res=spi1_WR(0xff);
	GD25_CS_DISENABLE;
	}while((res&0x01)==0x01);   //不忙再执行

}

void gd25_enable(void){

	gd25_waitbusy();
	GD25_CS_ENABLE;
	spi1_WR(0x06);
	GD25_CS_DISENABLE;


}

void gd25_erase64k(uint8_t blocknum){

   uint8_t data[4];   //1指令 3地址 高位到低位

	uint32_t addr = blocknum * 65536;  // 块号转换为字节地址（64KB = 65536字节）
	data[0]=0xD8;  // 64KB Block Erase 命令
	data[1]=addr>>16;
	data[2]=addr>>8;
	data[3]=addr>>0;

	gd25_waitbusy();
	gd25_enable();
	GD25_CS_ENABLE;
	spi1_WriteBytes(data,4);
	GD25_CS_DISENABLE;
	gd25_waitbusy();//保证擦除完成

}

void gd25_pagewrite(uint8_t *buf,uint16_t pagenum){

   uint8_t data[4];   //1指令 3地址 高位到低位

	uint32_t addr = pagenum * 256;  // 页号转换为字节地址（每页256字节）
	data[0]=0x02;  // Page Program 命令
	data[1]=addr>>16;
	data[2]=addr>>8;
	data[3]=addr>>0;

	gd25_waitbusy();
	gd25_enable();
	GD25_CS_ENABLE;
	spi1_WriteBytes(data,4);
	spi1_WriteBytes(buf,256);  // 每页256字节
	GD25_CS_DISENABLE;
	gd25_waitbusy();  // 等待页写完成

}

void gd25_read(uint8_t *buf,uint32_t addr,uint32_t datalen){

   uint8_t data[4];   //1指令 3地址 高位到低位

	data[0]=0x03;  // Read Data 命令
	data[1]=addr>>16;
	data[2]=addr>>8;
	data[3]=addr>>0;

	gd25_waitbusy();
	// 读操作不需要 Write Enable
	GD25_CS_ENABLE;
	spi1_WriteBytes(data,4);
	spi1_ReadBytes(buf,datalen);
	GD25_CS_DISENABLE;

}
