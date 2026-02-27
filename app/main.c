/*!
    \file    main.c
    \brief   APP entry — 由 Bootloader 跳转到此

    \version 2026-02-26, V1.0.0
*/

#include "main.h"

/* ========== MQTT 消息回调 ========== */

/**
 * @brief  "device/cmd" 主题回调 — 接收远程命令
 *
 * 示例命令格式 (纯文本):
 *   "LED:ON"   — 开灯
 *   "LED:OFF"  — 关灯
 *   "REBOOT"   — 软复位
 *   "STATUS"   — 上报状态
 */
static void on_cmd_msg(const char *topic, const char *payload, uint16_t len)
{
    u0_printf("[CMD] topic=%s, payload=%.*s\r\n", topic, len, payload);

    if (len >= 6 && strncmp(payload, "LED:ON", 6) == 0) {
        u0_printf("[CMD] LED ON\r\n");
        /* TODO: gpio_bit_set(LED_PORT, LED_PIN); */
    }
    else if (len >= 7 && strncmp(payload, "LED:OFF", 7) == 0) {
        u0_printf("[CMD] LED OFF\r\n");
        /* TODO: gpio_bit_reset(LED_PORT, LED_PIN); */
    }
    else if (len >= 6 && strncmp(payload, "REBOOT", 6) == 0) {
        u0_printf("[CMD] Rebooting...\r\n");
        delay_ms(100);
        NVIC_SystemReset();
    }
    else if (len >= 6 && strncmp(payload, "STATUS", 6) == 0) {
        /* 收到查询 → 发布状态到 "device/status" */
        mqtt_publish("device/status", "OK,running", 0, 0);
    }
}

/**
 * @brief  "device/config" 主题回调 — 接收配置更新
 */
static void on_config_msg(const char *topic, const char *payload, uint16_t len)
{
    u0_printf("[CFG] topic=%s, payload=%.*s\r\n", topic, len, payload);
    /* TODO: 解析并应用配置 */
}

/**
 * @brief  默认回调 — 处理所有未注册主题的消息
 */
static void on_default_msg(const char *topic, const char *payload, uint16_t len)
{
    u0_printf("[MQTT-DEFAULT] topic=%s, len=%u, data=%.*s\r\n",
              topic, len, len, payload);
}

int main(void)
{
    /* 重定位向量表到 APP 起始地址 */
    SCB->VTOR = APP_ADDR;
    __DSB();
    __ISB();

    /* Bootloader 跳转前执行了 __disable_irq()，必须重新开启 */
    __enable_irq();

    /* ===== 基础外设初始化 ===== */
    systick_config();
    delay_init();
    usart0_init(921600);
    iic_init();

    u0_printf("\r\n[APP] Running from 0x%08X\r\n", APP_ADDR);

    /* ===== 读取网络配置 ===== */
    Net_ReadConfig(&net_cfg);

    if (net_cfg.magic != NET_CFG_MAGIC) {
        u0_printf("\r\n========================================\r\n");
        u0_printf("[APP] Network config not found!\r\n");
        u0_printf("  Please reboot and press 'w' within 2s\r\n");
        u0_printf("  to enter Boot CLI, then:\r\n");
        u0_printf("    8 SSID,PASSWORD\r\n");
        u0_printf("    9 HOST,PORT,ID,USER,PASS\r\n");
        u0_printf("    7  (reboot)\r\n");
        u0_printf("========================================\r\n");
        u0_printf("[APP] Auto reboot in 10s...\r\n");
        delay_ms(10000);
        NVIC_SystemReset();
    }

    /* ===== ESP8266 初始化 ===== */
    usart1_init(115200);
    gd25q40e_init();

    esp8266_err_t err = ESP8266_Init(&net_cfg);
    if (err != ESP_OK) {
        u0_printf("[APP] ESP8266 init failed (err=%d), MQTT disabled\r\n", err);
    } else {
        /* ===== MQTT 模块初始化 + 主题订阅 ===== */
        mqtt_init();

        /* 订阅 OTA 主题 (固件升级) */
        mqtt_subscribe("device/ota", 1, mqtt_ota_callback);

        /* 订阅命令主题 (远程控制) */
        mqtt_subscribe("device/cmd", 1, on_cmd_msg);

        /* 订阅配置主题 (远程配置) */
        mqtt_subscribe("device/config", 1, on_config_msg);

        /* 设置默认回调 (兜底) */
        mqtt_set_default_callback(on_default_msg);

        u0_printf("[APP] MQTT ready, %u topics subscribed\r\n",
                  mqtt_get_sub_count());

        /* 上线通知 */
        mqtt_publish("device/status", "online", 0, 0);
    }

    /* ===== 主循环 ===== */
    char cmd_buf[128];
    uint16_t cmd_pos = 0;
    while (1) {
        /* MQTT 消息轮询 + 分发 */
        mqtt_poll();

        /* 串口命令解析（USART0） */
        while (u0_ucb.out != u0_ucb.in) {
            uint16_t len = u0_ucb.out->ed - u0_ucb.out->st + 1;
            for (uint16_t i = 0; i < len; i++) {
                char c = u0_ucb.out->st[i];
                if (cmd_pos < sizeof(cmd_buf) - 1) {
                    cmd_buf[cmd_pos++] = c;
                }
                if (c == '\n' || c == '\r') {
                    cmd_buf[cmd_pos] = '\0';
                    /* 解析命令 */
                    int op[8], topic[64], payload[64];
                    int qos = 0, retain = 0;
                    int n = 0;
                    if (sscanf(cmd_buf, "SUB %63s %d", topic, &qos) == 2) {
                        mqtt_subscribe(topic, qos, on_default_msg);
                        u0_printf("[CMD] SUB %s qos=%d\r\n", topic, qos);
                    } else if ((n = sscanf(cmd_buf, "PUB %63s %63s %d %d", topic, payload, &qos, &retain)) >= 2) {
                        if (n < 4) { qos = 0; retain = 0; }
                        mqtt_publish_json(topic, payload, qos, retain);
                        u0_printf("[CMD] PUB %s %s qos=%d retain=%d\r\n", topic, payload, qos, retain);
                    }
                    cmd_pos = 0;
                }
            }
            u0_ucb.out++;
            if (u0_ucb.out > u0_ucb.end) {
                u0_ucb.out = &u0_ucb.uart_infro_buf[0];
            }
        }

        /* 其他应用逻辑 */
        delay_ms(5);
    }
}
