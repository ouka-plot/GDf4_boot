#include "fmc.h"
#include "gd32f4xx_fmc.h"

//mcu内部的flash
// GD32F4xx Flash 扇区结构:
// Sector 0-3: 16KB each (0x08000000 - 0x0800FFFF)
// Sector 4: 64KB (0x08010000 - 0x0801FFFF)
// Sector 5-11: 128KB each

// 擦除指定扇区
void gd32_eraseflash(uint16_t sector_start, uint16_t sector_num){

	uint16_t i;
	fmc_unlock();
	
	for(i=0; i<sector_num; i++)
	{
		// CTL_SN 宏会将扇区号左移3位
		fmc_sector_erase(CTL_SN(sector_start + i));
	}
	
	fmc_lock();
}

void gd32_writeflash(uint32_t saddr, uint32_t *wdata, uint32_t wnum){
	
	fmc_unlock();
	//word 4字节
	uint32_t i;
	for(i=0; i<wnum; i++)
	{
		fmc_word_program(saddr, wdata[i]);
		saddr += 4;
	}
	fmc_lock();

}

// 读取内部Flash
uint32_t gd32_readflash(uint32_t addr){
	return *(volatile uint32_t*)addr;
}





