/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-02-18 21:44:12
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-02-18 23:26:44
 * @FilePath: \GDf4_boot\GDf4_boot\Driver\BSP\Include\usart.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __USART_H 
#define __USART_H


#include "gd32f4xx.h"

 /*
  1，存数据的数组 2048 uint8_t
  2.管理每存入一个数据块的uint8_t  st ed 指针。统计累计存多少数据量 uint16_t 存结构体 
  3.所有数据块指针的结构体 存入结构体数组 
  4，管理数组内 结构体，存 取的指针 in out 以及防止溢出的end指针
  

*/



#define rx_max 1100 //单次传输最大数据字节（Ymodem 1K包需要1029字节）
#define rx_bufmax 4096
#define tx_bufmax 2048
#define ucb_num 4//最多标记控制块


typedef struct
{

	uint8_t *st;
	uint8_t *ed;

}uart_rxbuff_ptr;

typedef struct{
	uint16_t totol_date;
	uart_rxbuff_ptr uart_infro_buf[ucb_num];
	uart_rxbuff_ptr *in;
	uart_rxbuff_ptr *out;
	uart_rxbuff_ptr *end;

}uart_ucb;



extern uint8_t uart_rxbuf[rx_bufmax];
extern uint8_t uart_txbuf[tx_bufmax];

extern uart_rxbuff_ptr u0_rxbuff_ptr;
extern volatile uart_ucb u0_ucb;


void usart0_init(uint32_t baudrate);
void uart_ptr_init(void);
void dma_init(void);
void u0_printf(char *format,...);
void usart0_init(uint32_t baudrate);


#endif


