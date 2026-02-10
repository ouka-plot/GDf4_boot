#ifndef  __fmc_h_
#define __fmc_h_

#include "main.h"

void gd32_eraseflash(uint16_t sector_start, uint16_t sector_num);
void gd32_writeflash(uint32_t saddr, uint32_t *wdata, uint32_t wnum);
uint32_t gd32_readflash(uint32_t addr);

#endif


