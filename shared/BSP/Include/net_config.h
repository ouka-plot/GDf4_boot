/**
 * @file   net_config.h
 * @brief  网络配置结构体 + EEPROM 持久化
 *
 * 独立于 ESP8266 驱动，Boot 和 APP 均可使用。
 * Boot CLI 用来写入/读取配置，APP 用来读取后传给 ESP8266_Init()。
 */
#ifndef __NET_CONFIG_H__
#define __NET_CONFIG_H__

#include "gd32f4xx.h"
#include <stdint.h>

/* ========== 网络配置结构体 (存 EEPROM) ========== */
#define NET_CFG_ADDR    0x0040U         /* EEPROM 起始地址 (避开 OTA 信息区) */
#define NET_CFG_MAGIC   0xCF55CF55U     /* 有效标识 */

typedef struct __attribute__((packed)) {
    uint32_t magic;              /* NET_CFG_MAGIC = 配置有效 */
    char     wifi_ssid[33];      /* SSID  max 32 + '\0'      */
    char     wifi_pass[33];      /* 密码  max 32 + '\0'      */
    char     mqtt_host[33];      /* 主机  max 32 + '\0'      */
    uint16_t mqtt_port;          /* 端口  默认 1883          */
    char     mqtt_client_id[17]; /* ClientID max 16 + '\0'   */
    char     mqtt_user[17];      /* 用户名  max 16 + '\0'    */
    char     mqtt_pass[17];      /* 密码    max 16 + '\0'    */
} NetConfig;                     /* 156 bytes packed          */

#define NET_CFG_SIZE sizeof(NetConfig)

/* 全局实例 */
extern NetConfig net_cfg;

/* EEPROM 读写 */
void Net_ReadConfig(NetConfig *cfg);
void Net_WriteConfig(const NetConfig *cfg);
void Net_ShowConfig(const NetConfig *cfg);

#endif /* __NET_CONFIG_H__ */
