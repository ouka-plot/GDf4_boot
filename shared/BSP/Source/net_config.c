/**
 * @file   net_config.c
 * @brief  网络配置 EEPROM 读写实现
 */
#include "net_config.h"
#include "AT24c256.h"
#include "usart.h"
#include <string.h>

/* 全局网络配置实例 */
NetConfig net_cfg;

void Net_ReadConfig(NetConfig *cfg) {
    memset(cfg, 0, NET_CFG_SIZE);
    AT24_read_page(NET_CFG_ADDR, (uint8_t*)cfg, NET_CFG_SIZE);
}

void Net_WriteConfig(const NetConfig *cfg) {
    AT24_write_bytes(NET_CFG_ADDR, (const uint8_t*)cfg, NET_CFG_SIZE);
}

void Net_ShowConfig(const NetConfig *cfg) {
    if (cfg->magic != NET_CFG_MAGIC) {
        u0_printf("[NET] Config not set (magic=0x%08lX)\r\n", cfg->magic);
        return;
    }
    u0_printf("=== Network Config ===\r\n");
    u0_printf("  WiFi SSID : %s\r\n", cfg->wifi_ssid);
    u0_printf("  WiFi Pass : %s\r\n", cfg->wifi_pass);
    u0_printf("  MQTT Host : %s\r\n", cfg->mqtt_host);
    u0_printf("  MQTT Port : %u\r\n", cfg->mqtt_port);
    u0_printf("  Client ID : %s\r\n", cfg->mqtt_client_id);
    u0_printf("  MQTT User : %s\r\n", cfg->mqtt_user);
    u0_printf("  MQTT Pass : %s\r\n", cfg->mqtt_pass);
}
