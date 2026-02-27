#include "esp8266.h"
#include "mqtt.h"
#include "AT24c256.h"
#include "net_config.h"
#include "ota_types.h"


/* ====================================================================
 *  MQTT OTA 协议定义
 *  
 *  服务端往 "device/ota" 发布:
 *    第 1 帧 (起始帧):  "OTA:START:<total_size>"    如 "OTA:START:12345"
 *    第 2~N 帧 (数据帧): 二进制固件分块 (≤ 1024B, hex 编码)
 *    最后 1 帧 (结束帧): "OTA:END"
 *
 *  ESP8266 AT 固件收到 MQTT 推送后会通过 USART1 上报:
 *    +MQTTSUBRECV:0,"device/ota",<len>,<payload>
 * ==================================================================== */



ota_mqtt_state_t ota_state      = OTA_IDLE;
uint32_t         ota_total_size = 0;
uint32_t         ota_received   = 0;
uint32_t         ota_ext_offset = 256;
uint16_t         ota_page_num   = 1;

/* ========== ESP8266 初始化 (带返回值，不死循环) ========== */

esp8266_err_t ESP8266_Init(const NetConfig *cfg) {
    char cmd_buf[160];

    /* 1. AT 测试 (重试 3 次) */
    uint8_t at_ok = 0;
    for (uint8_t i = 0; i < 3; i++) {
        if (ESP8266_SendCmd("AT\r\n", "OK", 1000)) { at_ok = 1; break; }
        delay_ms(500);
    }
    if (!at_ok) {
        u0_printf("[ESP] AT no response\r\n");
        return ESP_ERR_AT;
    }
    u0_printf("[ESP] ESP8266 Ready\r\n");

    ESP8266_SendCmd("ATE0\r\n", "OK", 500);       /* 关回显 */
    ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 500); /* Station 模式 */

    /* 2. 连 WiFi */
    snprintf(cmd_buf, sizeof(cmd_buf),
             "AT+CWJAP=\"%s\",\"%s\"\r\n", cfg->wifi_ssid, cfg->wifi_pass);
    if (!ESP8266_SendCmd(cmd_buf, "WIFI GOT IP", 15000)) {
        u0_printf("[ESP] WiFi connect failed\r\n");
        return ESP_ERR_WIFI;
    }
    u0_printf("[ESP] WiFi Connected\r\n");

    /* 3. MQTT 配置 */
    ESP8266_SendCmd("AT+MQTTCLEAN=0\r\n", "OK", 500);
    delay_ms(300);

    snprintf(cmd_buf, sizeof(cmd_buf),
             "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\" \"\r\n",
             cfg->mqtt_client_id, cfg->mqtt_user, cfg->mqtt_pass);
    if (!ESP8266_SendCmd(cmd_buf, "OK", 5000)) {
        u0_printf("[ESP] MQTT config failed\r\n");
        return ESP_ERR_MQTT_CFG;
    }

    snprintf(cmd_buf, sizeof(cmd_buf),
             "AT+MQTTCONN=0,\"%s\",%u,0\r\n", cfg->mqtt_host, cfg->mqtt_port);
    if (!ESP8266_SendCmd(cmd_buf, "OK", 15000)) {
        u0_printf("[ESP] MQTT connect failed\r\n");
        return ESP_ERR_MQTT_CONN;
    }
    u0_printf("[ESP] MQTT Connected, ready\r\n");
    return ESP_OK;
}

/* Hex 字符转半字节 */
static uint8_t hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

/* 十六进制字符串解码为二进制, 返回解码后的字节数 */
static uint16_t hex_decode(const char *hex, uint16_t hex_len, uint8_t *out) {
    uint16_t bin_len = hex_len / 2;
    for (uint16_t i = 0; i < bin_len; i++) {
        out[i] = (hex_char_to_nibble(hex[2*i]) << 4) | hex_char_to_nibble(hex[2*i + 1]);
    }
    return bin_len;
}



/**
 * @brief  处理 OTA 起始帧  "OTA:START:<size>"
 */
void ota_handle_start(const char *payload, uint16_t len) {
    /* 解析 "OTA:START:12345" */
    const char *p = payload + 10;  /* 跳过 "OTA:START:" */
    ota_total_size = 0;
    while (*p >= '0' && *p <= '9' && (p - payload) < len) {
        ota_total_size = ota_total_size * 10 + (*p - '0');
        p++;
    }

    if (ota_total_size == 0 || ota_total_size > 512 * 1024) {
        u0_printf("[OTA] Invalid size: %lu\r\n", ota_total_size);
        ota_state = OTA_ERROR;
        return;
    }

    /* 擦除外部 Flash (按 64KB 块, GD25Q40E = 512KB = 8 blocks) */
    uint8_t blocks = (ota_total_size + 256 + 65535) / 65536;  /* +256 for header */
    if (blocks > 8) blocks = 8;
    u0_printf("[OTA] Start, size=%lu, erasing %d blocks...\r\n", ota_total_size, blocks);
    for (uint8_t b = 0; b < blocks; b++) {
        gd25_erase64k(b);
    }

    ota_received   = 0;
    ota_ext_offset = 256;  /* 前 256B 留给 OTA_Header */
    ota_page_num   = 1;
    ota_state      = OTA_RECEIVING;
    u0_printf("[OTA] Flash erased, ready to receive\r\n");
}

/**
 * @brief  处理 OTA 数据帧 (hex 编码的二进制固件分块)
 */
void ota_handle_data(const char *payload, uint16_t len) {
    static uint8_t page_buf[256];
    static uint16_t buf_pos = 0;

    /* 将 hex payload 解码为二进制 */
    uint8_t tmp[512];
    uint16_t bin_len = hex_decode(payload, len, tmp);

    for (uint16_t i = 0; i < bin_len; i++) {
        page_buf[buf_pos++] = tmp[i];
        ota_received++;

        /* 攒满 256 字节写一页 */
        if (buf_pos >= 256) {
            gd25_pagewrite(page_buf, ota_page_num);
            ota_page_num++;
            ota_ext_offset += 256;
            buf_pos = 0;
            memset(page_buf, 0xFF, 256);
        }
    }

    /* 如果最后一块不足 256B 且已全部接收, 补 0xFF 写入 */
    if (ota_received >= ota_total_size && buf_pos > 0) {
        memset(&page_buf[buf_pos], 0xFF, 256 - buf_pos);
        gd25_pagewrite(page_buf, ota_page_num);
        buf_pos = 0;
    }

    u0_printf("[OTA] %lu / %lu bytes\r\n", ota_received, ota_total_size);
}

/**
 * @brief  处理 OTA 结束帧, 写 Header + 置 EEPROM 标志 + 重启
 */
void ota_handle_end(void) {
    u0_printf("[OTA] Transfer complete, writing header...\r\n");

    /* 构造 OTA_Header, 单段: 整个 APP 区 */
    OTA_Header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic      = OTA_HEADER_MAGIC;   /* 0xAA55AA55 */
    hdr.seg_count  = 1;
    hdr.total_len  = ota_received;
    hdr.header_crc32 = 0;                /* TODO: 计算 CRC */
    hdr.segs[0].target_addr = APP_ADDR;  /* 0x08010000 */
    hdr.segs[0].data_len    = ota_received;
    hdr.segs[0].ext_offset  = 256;       /* 数据从外部 Flash 第 256 字节开始 */
    hdr.segs[0].crc32       = 0;         /* TODO: 计算 CRC */

    /* 写 Header 到外部 Flash 第 0 页 */
    uint8_t hdr_buf[256];
    memset(hdr_buf, 0xFF, 256);
    memcpy(hdr_buf, &hdr, OTA_HEADER_SIZE);
    gd25_pagewrite(hdr_buf, 0);

    /* 设 EEPROM 升级标志 */
    ota_info.boot_flag   = BOOT_FLAG_SET;    /* 0x12345678 */
    ota_info.header_addr = 0;
    AT24_WriteOTAInfo();

    u0_printf("[OTA] Done! Rebooting...\r\n");
    delay_ms(200);
    NVIC_SystemReset();  /* 软复位 → Bootloader 检测标志 → 搬运 → 跳 APP */
}

/**
 * @brief  OTA MQTT 回调 — 注册到 mqtt_subscribe("device/ota", ...)
 *         根据 OTA 状态机分发起始帧/数据帧/结束帧
 */
void mqtt_ota_callback(const char *topic, const char *payload, uint16_t pay_len)
{
    (void)topic;

    switch (ota_state) {
    case OTA_IDLE:
        if (pay_len >= 10 && strncmp(payload, "OTA:START:", 10) == 0) {
            ota_handle_start(payload, pay_len);
        } else {
            u0_printf("[OTA] Unexpected: %.*s\r\n", pay_len, payload);
        }
        break;

    case OTA_RECEIVING:
        if (pay_len >= 7 && strncmp(payload, "OTA:END", 7) == 0) {
            ota_handle_end();
        } else {
            ota_handle_data(payload, pay_len);
        }
        break;

    default:
        break;
    }
}


/*  发送 AT 指令并等待期望回复
 *  通过 USART1 (u1_ucb) 收发，USART0 (u0_printf) 打印调试日志
 *  返回 1: 成功, 0: 超时
 */
uint8_t ESP8266_SendCmd(char* cmd, char* expect_reply, uint32_t timeout) {
    // 1. 清空 USART1 接收缓冲，让 out 追上 in
    u1_ucb.out = (uart_rxbuff_ptr *)u1_ucb.in;

    // 2. 发送指令
    u0_printf("[CMD] %s", cmd);          // USART0 调试 Log
    u1_send_string(cmd);                  // USART1 发给 ESP8266

    // 3. 循环等待
    uint32_t start = Get_Tick();
    while ((Get_Tick() - start) < timeout) {
        if (u1_ucb.out != u1_ucb.in) {
            // 收到一帧数据，计算长度并打印
            uint16_t len = u1_ucb.out->ed - u1_ucb.out->st + 1;

            // 安全地拷贝到临时缓冲区再做字符串操作，避免越界写 '\0'
            char tmp[256];
            uint16_t copy_len = (len < sizeof(tmp) - 1) ? len : (sizeof(tmp) - 1);
            memcpy(tmp, u1_ucb.out->st, copy_len);
            tmp[copy_len] = '\0';

            u0_printf("[RECV] %s\r\n", tmp);

            // 检查是否包含期望字符串
            if (strstr(tmp, expect_reply) != NULL) {
                return 1;   // 成功
            }
             if (strstr(tmp, "ERROR") != NULL&&strcmp(cmd, "AT+MQTTCLEAN=0\r\n") == 0) {
                return 2;   // 失败
            }

            // 没命中，继续消费下一帧
            u1_ucb.out++;
            if (u1_ucb.out > u1_ucb.end) {
                u1_ucb.out = &u1_ucb.uart_infro_buf[0];
            }
        }
    }
    u0_printf("[ERR] Timeout!\r\n");
    return 0;
}



