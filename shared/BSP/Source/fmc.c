/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-02-06 22:06:53
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-02-10 18:25:45
 * @FilePath: \GDf4_boot\Driver\BSP\Source\fmc.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "fmc.h"
#include "gd32f4xx_fmc.h"
#include "usart.h"  // 添加调试输出

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
	
	/* 清除所有错误标志位 */
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR | 
				   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);
	
	fmc_unlock();
	
	/* 检查是否解锁成功 */
	if(FMC_CTL & FMC_CTL_LK){
		u0_printf("[FMC] ERROR: Flash still locked!\r\n");
		return;
	}
	
	/* 调试：打印 FMC_STAT 和 FMC_CTL */
	static uint8_t first_write = 1;
	if(first_write){
		u0_printf("[FMC] STAT=0x%08lX, CTL=0x%08lX\r\n", FMC_STAT, FMC_CTL);
		u0_printf("[FMC] First write: addr=0x%08lX, data=0x%08lX, num=%lu\r\n",
				  saddr, wdata[0], wnum);
	}
	
	//word 4字节
	uint32_t i;
	fmc_state_enum state;
	
	for(i=0; i<wnum; i++)
	{
		state = fmc_word_program(saddr, wdata[i]);
		if(state != FMC_READY){
			u0_printf("[FMC] ERROR: fmc_word_program failed! state=%d @0x%08lX\r\n", 
					  state, saddr);
			u0_printf("[FMC] STAT=0x%08lX after error\r\n", FMC_STAT);
			fmc_lock();
			first_write = 1;  // 重置以便下次调试
			return;
		}
		
		/* 调试：第一个字写入后验证 */
		if(first_write && i == 0){
			uint32_t readback = *(volatile uint32_t*)saddr;
			u0_printf("[FMC] Verify: wrote 0x%08lX, read 0x%08lX\r\n", 
					  wdata[i], readback);
			first_write = 0;
		}
		
		saddr += 4;
	}
	fmc_lock();

}

// 读取内部Flash
uint32_t gd32_readflash(uint32_t addr){
	return *(volatile uint32_t*)addr;
}





