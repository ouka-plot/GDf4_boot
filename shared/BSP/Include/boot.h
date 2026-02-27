#ifndef __BOOT_H__
#define __BOOT_H__      

#include "main.h"
#include "ota_types.h"

typedef void (*load_a)(void);


void bootloader_branch(void);
void jump2app(uint32_t app_addr);
void bootloader_clear(void);

// 从外部 Flash 读取 OTA_Header 并按段搬运到片上 Flash
// 返回 0 成功，非 0 失败码
uint8_t boot_apply_update(void);
_Bool bootloader_cli(uint32_t timeout_ms);
void bootloader_cli_help(void);
void bootloader_cli_event(uint8_t *data, uint16_t len);


#endif



