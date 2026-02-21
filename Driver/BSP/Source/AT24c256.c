#include "AT24c256.h"
#include "iic.h"
#include "boot.h"
#include "string.h"
uint8_t AT24_write_byte(uint8_t addr,uint8_t data){

	
	 iic_start();
	 iic_send_byte(AT24_WADDR);
	 if(iic_wait_ack(100)!=0) return 1;
	 iic_send_byte(addr);
	 if(iic_wait_ack(100)!=0) return 2;
	 iic_send_byte(data);
	 if(iic_wait_ack(100)!=0) return 3;
	 iic_stop();
	 delay_ms(5);
	 return 0;
}

 //页写入，最大64字节
uint8_t AT24_write_page(uint16_t addr,uint8_t *databuf,uint16_t datalen ){

	 iic_start();
	 iic_send_byte(AT24_WADDR);
	 if(iic_wait_ack(100)!=0) return 1;
	 iic_send_byte((uint8_t)(addr >> 8)); // 地址高字节
	 if(iic_wait_ack(100)!=0) return 2;
	 iic_send_byte((uint8_t)(addr & 0xFF)); // 地址低字节
	 if(iic_wait_ack(100)!=0) return 3;
	 for(uint16_t i=0;i<datalen;i++)
	 {
	 
		 iic_send_byte(databuf[i]);
		 if(iic_wait_ack(100)!=0) return 4+i;
	 
	 }
	 iic_stop();
	 delay_ms(10);
	 return 0;
}

//AT24C256: 262144 bit = 32768 B = 32KB，页操作最大64字节
uint8_t AT24_read_page(uint16_t addr,uint8_t *databuf,uint16_t datalen ){

	 iic_start();
	 iic_send_byte(AT24_WADDR);
	 if(iic_wait_ack(100)!=0) return 1;
	 iic_send_byte((uint8_t)(addr >> 8)); // 地址高字节
	 if(iic_wait_ack(100)!=0) return 2;
	 iic_send_byte((uint8_t)(addr & 0xFF)); // 地址低字节
	 if(iic_wait_ack(100)!=0) return 3;
	 iic_start();
	 iic_send_byte(AT24_RADDR);
	 if(iic_wait_ack(100)!=0) return 4;
	 for(uint16_t i=0;i<datalen-1;i++)
	 {
		 databuf[i]=iic_recv_byte(1);
	 }
	 databuf[datalen-1]=iic_recv_byte(0);
	 iic_stop();
	 delay_ms(5);
	 return 0;
}

void AT24_ReadOTAInfo(void){

	memset(&ota_info,0,OTA_INFO_SIZE);//清空ota_info结构体
	AT24_read_page(0,(uint8_t *)&ota_info,OTA_INFO_SIZE);//从地址0读取OTA信息到ota_info

}


//存储OTA信息到AT24C256
void AT24_WriteOTAInfo(void){

	AT24_write_page(0,(uint8_t *)&ota_info,OTA_INFO_SIZE);//将ota_info结构体写入到地址0

}




