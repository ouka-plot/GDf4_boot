/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-02-18 21:44:12
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-02-26 00:07:39
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
	volatile uart_rxbuff_ptr *in;
	volatile uart_rxbuff_ptr *out;
	volatile uart_rxbuff_ptr *end;

}uart_ucb;



/* ---------- USART0 (PA9/PA10) 调试串口 ---------- */
extern uint8_t uart_rxbuf[rx_bufmax];
extern uint8_t uart_txbuf[tx_bufmax];
extern uart_rxbuff_ptr u0_rxbuff_ptr;
extern volatile uart_ucb u0_ucb;

void usart0_init(uint32_t baudrate);
void uart_ptr_init(void);
void dma_init(void);
void u0_printf(char *format,...);

/* ---------- USART1 (PD5-TX / PD6-RX) 通信串口 ---------- */
/*  DMA0_CH5, sub-peripheral 4, USART1_RX               */
#define u1_rx_max    1100         /* 单帧最大字节 */
#define u1_rx_bufmax 4096         /* 接收环形缓冲区 */
#define u1_tx_bufmax 2048         /* 发送缓冲区 */
#define u1_ucb_num   4            /* 控制块槽数 */

extern uint8_t  u1_rxbuf[u1_rx_bufmax];
extern uint8_t  u1_txbuf[u1_tx_bufmax];
extern volatile uart_ucb u1_ucb;

void usart1_init(uint32_t baudrate);
void u1_uart_ptr_init(void);
void u1_dma_init(void);
void u1_printf(char *format,...);
void u1_send_bytes(const uint8_t *data, uint16_t len);
void u1_send_string(const char *str);

#endif


