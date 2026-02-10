#include "AT24c02.h"
#include "iic.h"

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

 //Ò³ 64 ×Ö½Ú
uint8_t AT24_write_page(uint16_t addr,uint8_t *databuf,uint16_t datalen ){

	 iic_start();
	 iic_send_byte(AT24_WADDR);
	 if(iic_wait_ack(100)!=0) return 1;
	 iic_send_byte((uint8_t)(addr >> 8)); // ?????
	 if(iic_wait_ack(100)!=0) return 2;
	 iic_send_byte((uint8_t)(addr & 0xFF)); // ?????
	 if(iic_wait_ack(100)!=0) return 3;
	 for(uint16_t i=0;i<datalen;i++)
	 {
	 
		 iic_send_byte(databuf[i]);
		 if(iic_wait_ack(100)!=0) return 4+i;
	 
	 }
	 iic_stop();
	 delay_ms(5);
	 return 0;
}

							//0 64 128
//262144 bit  32768 B  256KB  Ò³²Ù×÷64 ×Ö½Ú
uint8_t AT24_read_page(uint16_t addr,uint8_t *databuf,uint16_t datalen ){

	 iic_start();
	 iic_send_byte(AT24_WADDR);
	 if(iic_wait_ack(100)!=0) return 1;
	 iic_send_byte((uint8_t)(addr >> 8)); // ?????
	 if(iic_wait_ack(100)!=0) return 2;
	 iic_send_byte((uint8_t)(addr & 0xFF)); // ?????
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




