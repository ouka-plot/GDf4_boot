#include "boot.h"

OTA_InfoCB ota_info = {0};

void bootloader_branch (void){


    if(ota_info.boot_flag == BOOT_FLAG_SET){
        //进入bootloader
        u0_printf("Enter bootloader\r\n");
       // bootloader_init();
       // bootloader_run();
    }else{
        //进入用户程序
        u0_printf("Enter app\r\n");
      //  jump2app(ota_info.app_addr);
    }






}
