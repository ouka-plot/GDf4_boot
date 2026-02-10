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
	
	// 测试OTA flag写入
	u0_printf("====== Testing OTA Flag ======\r\n");
	
	// 先清空EEPROM地址0的数据
	u0_printf("Clearing EEPROM at address 0...\r\n");
	OTA_InfoCB clear_ota = {0};
	uint8_t ret = AT24_write_page(0, (uint8_t *)&clear_ota, OTA_INFO_SIZE);
	if(ret != 0){
		u0_printf("Clear failed, error: %d\r\n", ret);
	}
	delay_ms(100);
	
	// 构造OTA信息结构
	OTA_InfoCB test_ota = {0};  // 明确初始化为0
	test_ota.boot_flag = BOOT_FLAG_SET;    // 设置bootloader flag
	test_ota.app_addr = 0x08020000;        // 设置应用地址
	
	u0_printf("Before write - boot_flag: 0x%x, app_addr: 0x%x\r\n", test_ota.boot_flag, test_ota.app_addr);
	
	// 打印结构体的实际字节
	u0_printf("test_ota bytes to write:\r\n");
	uint8_t *ptr = (uint8_t *)&test_ota;
	for(int i = 0; i < 8; i++){
		u0_printf("[%d]=0x%02x ", i, ptr[i]);
	}
	u0_printf("\r\n");
	
	// 单独测试写一个字节
	u0_printf("Testing single byte write at address 100...\r\n");
	uint8_t test_byte = 0xAB;
	ret = AT24_write_page(100, &test_byte, 1);
	u0_printf("Single byte write result: %d\r\n", ret);
	delay_ms(50);
	uint8_t read_test = 0;
	AT24_read_page(100, &read_test, 1);
	u0_printf("Read back: 0x%02x\r\n", read_test);
	
	// 将OTA信息写入EEPROM地址0
	u0_printf("Writing full OTA structure to EEPROM...\r\n");
	ret = AT24_write_page(0, (uint8_t *)&test_ota, OTA_INFO_SIZE);
	if(ret != 0){
		u0_printf("Write OTA failed, error: %d\r\n", ret);
	}else{
		u0_printf("Write OTA success\r\n");
	}
	
	delay_ms(100);
	
	// 读取原始字节数据进行调试
	u0_printf("Raw bytes at address 0:\r\n");
	uint8_t raw_bytes[8] = {0};
	AT24_read_page(0, raw_bytes, 8);
	for(int i = 0; i < 8; i++){
		u0_printf("[%d]=0x%02x ", i, raw_bytes[i]);
	}
	u0_printf("\r\n");
	
	// 重新读取OTA信息到ota_info全局变量
	u0_printf("Reading OTA flag from EEPROM...\r\n");
	AT24_ReadOTAInfo();
	
	u0_printf("OTA flag value: 0x%x\r\n", ota_info.boot_flag);
	u0_printf("OTA app_addr: 0x%x\r\n", ota_info.app_addr);
	
	// 检查boot branch
	u0_printf("Checking bootloader branch...\r\n");
    bootloader_branch();


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
