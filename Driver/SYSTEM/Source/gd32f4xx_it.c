/*!
    \file    gd32f4xx_it.c
    \brief   interrupt service routines

    \version 2025-07-31, V3.3.2, firmware for GD32F4xx
*/

/*
    Copyright (c) 2025, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx_it.h"
#include "main.h"
#include "systick.h"
#include "usart.h"

void USART0_IRQHandler(void){

			/*判断标志位 清除标志位 
       //	保证接收 1 buf不溢出 2048-count<256,in指向 buf[0]  已传输 （rx_max-dma未传输量 ）
	                 2.rx接收 ++ 接收完成记录ed   in 递增  同时count加本次接收量
					 3.dma normal 模式 最后要重新开启中断
	
		 200data   st 0   ed 199     count 200
	     200 data    st200    ed399   count 400
	   
	  */
	 if(usart_interrupt_flag_get(USART0,USART_INT_FLAG_IDLE)==1){
		//usart_interrupt_flag_clear(USART0,USART_INT_FLAG_IDLE);
		
        volatile uint32_t tmp = USART_STAT0(USART0); 
        tmp = USART_DATA(USART0); // 读数据寄存器清标志
		 
		uint32_t count=  (rx_max+1)-dma_transfer_number_get(DMA1,DMA_CH2);  //因为最开始设置number 为rx_max+1
		u0_ucb.totol_date+=count;
		u0_ucb.in->ed=&uart_rxbuf[u0_ucb.totol_date-1];//(u0_ucb.in->st)+count-1;
		u0_ucb.in++;
		if( u0_ucb.in == u0_ucb.end){
				  u0_ucb.in=&u0_ucb.uart_infro_buf[0];
			 
			 }
		 
		if(rx_bufmax-u0_ucb.totol_date<rx_max){
					 u0_ucb.in->st=uart_rxbuf;
					
				     u0_ucb.totol_date=0;
			 }
		else{
			  u0_ucb.in->st=&uart_rxbuf[u0_ucb.totol_date];
		
		}

	//重开dma
	 //负责重新配置 number memory_addr
	 dma_channel_disable(DMA1,DMA_CH2);
	 dma_flag_clear(DMA1, DMA_CH2, DMA_FLAG_FTF | DMA_FLAG_HTF);
	 dma_transfer_number_config(DMA1,DMA_CH2,rx_max+1);
	 dma_memory_address_config(DMA1,DMA_CH2,DMA_MEMORY_0,(uint32_t)(u0_ucb.in->st));
	 dma_channel_enable(DMA1,DMA_CH2);
	 }


 }

	







/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
    /* if NMI exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
    /* if Hard Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    /* if Memory Manage exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    /* if Bus Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    /* if Usage Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
    /* if SVC exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
    /* if DebugMon exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
    /* if PendSV exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief    this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
//    led_spark();
//    delay_decrement();
}
