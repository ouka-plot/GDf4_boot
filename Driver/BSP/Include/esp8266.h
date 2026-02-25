/*
 * @Author: oukaa 3328236081@qq.com
 * @Date: 2026-02-25 23:41:44
 * @LastEditors: oukaa 3328236081@qq.com
 * @LastEditTime: 2026-02-25 23:51:59
 * @FilePath: \GDf4_boot\Driver\BSP\Include\esp8266.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef __ESP8266_H__
#define __ESP8266_H__


#include "main.h"
#include "gd32f4xx.h"
/* OTA 状态机 */
typedef enum {
    OTA_IDLE = 0,       /* 等待起始帧 */
    OTA_RECEIVING,      /* 正在接收数据帧 */
    OTA_COMPLETE,       /* 接收完毕，等待写入 */
    OTA_ERROR           /* 出错 */
} ota_mqtt_state_t;

extern ota_mqtt_state_t ota_state;
extern uint32_t ota_total_size;
extern uint32_t ota_received;
extern uint32_t ota_ext_offset;
extern uint16_t ota_page_num;

void ESP8266_Init(void);
void mqtt_ota_poll(void);
uint8_t ESP8266_SendCmd(char* cmd, char* expect_reply, uint32_t timeout);

#endif
