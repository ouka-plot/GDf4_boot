#ifndef __BOOT_H__
#define __BOOT_H__      

#include "main.h"



#define BOOT_FLAG_SET 0x12345678

typedef struct{
    uint32_t boot_flag;  //0x12345678表示进入bootloader
    uint32_t app_addr;   //用户程序地址 
}OTA_InfoCB;

#define OTA_INFO_SIZE sizeof(OTA_InfoCB)

extern OTA_InfoCB ota_info;

void bootloader_branch (void);

#endif



