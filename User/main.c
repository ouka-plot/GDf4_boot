/*!
    \file    main.c
    \brief   led spark with systick

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


#include "main.h"




int main(void){
	usart0_init(921600);
	u0_printf("%d %c %x",0x30,0x30,0x30);
    delay_init();
	iic_init();
	gd25q40e_init();
	
	uint8_t buf[256]={0};
	
	//先擦除后写
//	gd25_erase64k(0);
//	//64KB,256个页，每页256B
	uint16_t i,j=0;
//	for(i=0;i<256;i++) 
//	{
//		for(j=0;j<256;j++)
//		{
//			buf[j]=i;  // 每页存储页号
//			
//		}
//		gd25_pagewrite(buf,i);
//	
//	}
//	delay_ms(100);
//	
//	uint8_t bufrecv[256]={0};
//	// 读 页（总共256字节）
//	for(i=0; i<256; i++) 
//	{
//		gd25_read(bufrecv,i*256,256); //一次读256B
//		for(j=0;j<256;j++)
//		{
//		
//			u0_printf("Read page %d address %d status: %d\r\n", i,j,bufrecv[j] );

//			
//		}
//	
//	}
//	
//	
	uint32_t wbuf[256];
	for (i=0;i<256;i++){
		wbuf[i]=0x12345678;
		
	}
	
//	// 注意：Sector 0-3 是程序区，不能擦除！
//	// 使用 Sector 4 (64KB, 起始地址 0x08010000) 进行测试
//	#define TEST_FLASH_ADDR  0x08010000  // Sector 4 起始地址
//	
//	gd32_eraseflash(4, 1);  // 擦除 Sector 4
//	gd32_writeflash(TEST_FLASH_ADDR, wbuf, 256);  // 写入 256 个 word (1KB)
//	
//	// 读取并验证
//	for(j=0; j<256; j++)
//	{
//		uint32_t readval = gd32_readflash(TEST_FLASH_ADDR + j*4);
//		u0_printf("Read flash addr 0x%08X: 0x%08X\r\n", TEST_FLASH_ADDR + j*4, readval);
//	}
	while(1) 
    {
    }
}
	











































///*!
//    \brief    toggle the led every 500ms
//    \param[in]  none
//    \param[out] none
//    \retval     none
//*/
//void led_spark(void)
//{
//    static __IO uint32_t timingdelaylocal = 0U;

//    if(timingdelaylocal) {

//        if(timingdelaylocal < 500U) {
//            gd_eval_led_on(LED1);
//        } else {
//            gd_eval_led_off(LED1);
//        }

//        timingdelaylocal--;
//    } else {
//        timingdelaylocal = 1000U;
//    }
//}

///*!
//    \brief    main function
//    \param[in]  none
//    \param[out] none
//    \retval     none
//*/
//int main(void)
//{
////#ifdef __FIRMWARE_VERSION_DEFINE
////    uint32_t fw_ver = 0;
////#endif

////    gd_eval_led_init(LED1);
////    gd_eval_com_init(EVAL_COM0);
////    systick_config();

////#ifdef __FIRMWARE_VERSION_DEFINE
////    fw_ver = gd32f4xx_firmware_version_get();
////    /* print firmware version */
////    printf("\r\nGD32F4xx series firmware version: V%d.%d.%d", (uint8_t)(fw_ver >> 24), (uint8_t)(fw_ver >> 16), (uint8_t)(fw_ver >> 8));
////#endif /* __FIRMWARE_VERSION_DEFINE */

//    while(1) {
//    }
//}

//#ifdef GD_ECLIPSE_GCC
///* retarget the C library printf function to the USART, in Eclipse GCC environment */
//int __io_putchar(int ch)
//{
//    usart_data_transmit(EVAL_COM0, (uint8_t)ch);
//    while(RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE));
//    return ch;
//}
//#else
///* retarget the C library printf function to the USART */
//int fputc(int ch, FILE *f)
//{
//    usart_data_transmit(EVAL_COM0, (uint8_t)ch);
//    while(RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE));

//    return ch;
//}
//#endif /* GD_ECLIPSE_GCC */
