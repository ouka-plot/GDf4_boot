#include "iic.h"



//时钟

void iic_init(){

   //rcu
	rcu_periph_clock_enable(RCU_GPIOE);

	/* set GPIO mode and output options */
	gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0|GPIO_PIN_1);
	gpio_output_options_set(GPIOE, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_0|GPIO_PIN_1);

    IIC_SCL_H;
    IIC_SDA_H;
	delay_us(10);
}

//start
void iic_start(){
    IIC_SCL_H;
    IIC_SDA_H; 
	delay_us(2);
	IIC_SDA_L;
    delay_us(2);
	IIC_SCL_L;   //让sda可变化
    
 
}
//stop
void iic_stop(){
    IIC_SCL_L;      // 先拉低SCL
    delay_us(2);
    IIC_SDA_L;      // SDA拉低（SCL低时可以改变SDA）
    delay_us(2);
    IIC_SCL_H;      // SCL拉高
    delay_us(2);
    IIC_SDA_H;      // SCL高时SDA从低到高 = STOP信号
    delay_us(2);
}
//send 1 type
void iic_send_byte(uint8_t data){
	//0-7  从高到低发  1001 1100
	int i = 7;
	for(i=7;i>=0;i--)
	{    
		IIC_SCL_L;
		delay_us(2);
		if( data & BIT(i))
		{			
			IIC_SDA_H;
		}
		else
		{
		    IIC_SDA_L;
		}
		delay_us(2);
		IIC_SCL_H;
		delay_us(4);
	}
	IIC_SCL_L;
	delay_us(2);
	IIC_SDA_H;//释放总线
	delay_us(2);
}


//wait ack
uint8_t iic_wait_ack(int16_t timeout){
	
	//释放总线，让从机拉低SDA
	IIC_SDA_H;
	delay_us(2);
	IIC_SCL_H;  //提升SCL，让从机应答
	delay_us(2);
	
	//读取应答 
	do{
		timeout--;
		delay_us(2);
	  }while(IIC_SDA_READ && timeout > 0);
	 
	 if(timeout <= 0) {
		IIC_SCL_L;
		return 1;  //超时，无应答
	 }
	 
	 IIC_SCL_L;
	 delay_us(2);
	 
	 return 0;
}
	 



uint8_t iic_recv_byte(uint8_t ack){

	uint8_t res = 0;
	int8_t i = 0;
	
	// 释放SDA，让从机发送数据
	IIC_SDA_H;
	delay_us(2);
	
	for(i = 7; i >= 0; i--)
	{
		IIC_SCL_L;
		delay_us(2);
		IIC_SCL_H;  // SCL 高电平时读取数据
		delay_us(2);
		
		if(gpio_input_bit_get(GPIOE, GPIO_PIN_0)){
			res |= BIT(i);
		}
		delay_us(2);
	}
	
	IIC_SCL_L;
	delay_us(2);
	
	if(ack)   //要应答
	{
		IIC_SDA_L;
		delay_us(2);
		IIC_SCL_H;
		delay_us(2);
		IIC_SCL_L;
		delay_us(2);
		IIC_SDA_H; //释放总线
		delay_us(2);
	}
	else      //不应答
	{
		IIC_SDA_H;
		delay_us(2);
		IIC_SCL_H;
		delay_us(2);
		IIC_SCL_L;
		delay_us(2);
	}
	
	return res;
}






