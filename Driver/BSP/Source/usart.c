/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-01-17 16:31:39
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-01-19 13:39:06
 * @FilePath: \Projectd:\desktop\GDf4_boot\Driver\BSP\Source\usart.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "usart.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"
/*
  1，存数据的数组 2048 uint8_t
  2.管理每存入一个数据块的uint8_t  st ed 指针。统计累计存多少数据量 uint16_t 存结构体 
  3.所有数据块指针的结构体 存入结构体数组 
  4，管理数组内 结构体，存 取的指针 in out 以及防止溢出的end指针
  

*/

uint8_t uart_rxbuf[rx_bufmax];
uint8_t uart_txbuf[tx_bufmax];

uart_rxbuff_ptr u0_rxbuff_ptr;
uart_ucb u0_ucb;

/*

串口初始化 init dma 中断 空闲
		  项目树添加库文件 rte 包含头文件宏定义
*/
//gpio  PA9  PA10 rcu   GPIOA USART0
/* reset USART */
void usart0_init(uint32_t baudrate)
{
 //rcu
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_USART0);
/* set GPIO alternate function */
	gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9);     //tx    复用 推免输出 50mhz
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
	gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
   
	gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_10);     //rx
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
	gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

//usart config
	usart_deinit(USART0);
/* configure usart baud rate value */
	usart_baudrate_set(USART0, baudrate);
/* configure usart parity function */
	usart_parity_config(USART0, USART_PM_NONE);
/* configure usart word length */
	usart_word_length_set(USART0,USART_WL_8BIT);
/* configure usart stop bit length */
	usart_stop_bit_set(USART0,USART_STB_1BIT);
	usart_transmit_config(USART0,USART_TRANSMIT_ENABLE);
	usart_receive_config(USART0,USART_RECEIVE_ENABLE);
//dma
	usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);

//空闲中断
	nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
	nvic_irq_enable(USART0_IRQn,0,0);

/* enable USART interrupt */
	usart_interrupt_enable(USART0 , USART_INT_IDLE);
	dma_init();
	uart_ptr_init();
	
/* enable usart */
	usart_enable(USART0);



}


/*指针初始化    先声明       buf st 指向buf头 ed在接收完确定，
                            ucb count为0 
                            in 在infro buf头 out在infro buf头 end在infro buf最后一个 
                            infto_buf 内容放st 
*/
void uart_ptr_init(){
	
	u0_ucb.totol_date=0;
	u0_ucb.in=u0_ucb.uart_infro_buf;//1004    u0_ucb.uart_infro_buf[0]  1024
	u0_ucb.out=u0_ucb.uart_infro_buf;
	u0_ucb.end=&u0_ucb.uart_infro_buf[ucb_num-1];
	u0_ucb.in->st=uart_rxbuf;


}
//dma 初始化
void dma_init(){
	rcu_periph_clock_enable(RCU_DMA1);
	
	
	dma_deinit(DMA1,DMA_CH2);
	
	dma_single_data_parameter_struct  dma1_struct;
	
	dma1_struct.periph_addr         = (uint32_t)&USART_DATA(USART0);//USART0+4;  //usart0 data rigister
    dma1_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma1_struct.memory0_addr        = (uint32_t)uart_rxbuf;
    dma1_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma1_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma1_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma1_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma1_struct.number              = rx_max+1;   //传输最大值 +1
    dma1_struct.priority            = DMA_PRIORITY_HIGH;

	dma_single_data_mode_init(DMA1,DMA_CH2,&dma1_struct) ;
	dma_channel_subperipheral_select(DMA1,DMA_CH2,DMA_SUBPERI4);

	
	dma_channel_enable(DMA1,DMA_CH2);



}
//串口空闲中断处理 rx 接收数据 in

//printf编写 
void u0_printf(char *format,...){

	uint16_t i;
	va_list  listdata;
	va_start(listdata,format);
	vsprintf((char*)uart_txbuf,format,listdata);
	va_end(listdata);
	
	for(i=0;i<strlen((const char*)uart_txbuf);i++){
	while(usart_flag_get(USART0,USART_FLAG_TBE)!=1)
		;
        usart_data_transmit(USART0,(uint16_t)uart_txbuf[i]);
	
	}

while(usart_flag_get(USART0,USART_FLAG_TBE)!=1);  //传输缓冲区为空


}

//main tx处理数据 ->printf out



