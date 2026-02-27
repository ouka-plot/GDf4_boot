/**
 * @file   mqtt.c
 * @brief  MQTT 应用层实现 — 基于 ESP8266 AT 指令
 *
 * 使用 ESP8266 AT 固件的 MQTT 指令集:
 *   AT+MQTTSUB   — 订阅
 *   AT+MQTTUNSUB — 取消订阅
 *   AT+MQTTPUB   — 发布
 *   +MQTTSUBRECV — 接收推送
 *
 * @version 2026-02-26, V1.0.0
 */

#include "mqtt.h"
#include "esp8266.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

/* ===================== 私有数据 ===================== */

static mqtt_sub_entry_t sub_table[MQTT_MAX_SUBSCRIPTIONS];
static mqtt_msg_cb_t    default_callback = NULL;

/* ===================== 内部辅助 ===================== */

/**
 * @brief  从 +MQTTSUBRECV 帧中解析 topic 和 payload
 *
 * 格式: +MQTTSUBRECV:0,"<topic>",<len>,<payload>
 *
 * @param  frame      帧字符串
 * @param  frame_len  帧长度
 * @param  topic      [out] 主题起始指针
 * @param  topic_len  [out] 主题长度
 * @param  payload    [out] payload 起始指针
 * @param  pay_len    [out] payload 长度
 * @return 1=成功  0=不是 MQTTSUBRECV
 */
static uint8_t mqtt_parse_subrecv(const char *frame, uint16_t frame_len,
                                   const char **topic, uint16_t *topic_len,
                                   const char **payload, uint16_t *pay_len)
{
    if (frame_len < 20) return 0;
    const char *p = strstr(frame, "+MQTTSUBRECV:");
    if (p == NULL) return 0;

    /* 找第一个引号 → topic 开始 */
    const char *q1 = strchr(p, '\"');
    if (q1 == NULL) return 0;
    q1++;  /* 跳过 " */

    /* 找第二个引号 → topic 结束 */
    const char *q2 = strchr(q1, '\"');
    if (q2 == NULL) return 0;

    *topic     = q1;
    *topic_len = (uint16_t)(q2 - q1);

    /* 跳过 ",<len>," 找第 3 个逗号后面即为 payload */
    /* q2 后面应该是 ",<len>,<payload>" */
    const char *c1 = q2 + 1;  /* 应该是 ',' */
    if (*c1 != ',') return 0;
    c1++;  /* 跳过逗号，现在指向 <len> */

    const char *c2 = strchr(c1, ',');
    if (c2 == NULL) return 0;
    c2++;  /* 跳过逗号，指向 payload */

    *payload = c2;
    *pay_len = (uint16_t)(frame_len - (uint16_t)(c2 - frame));

    return 1;
}

/**
 * @brief  在订阅表中查找匹配的主题
 * @return 匹配的回调指针, NULL = 未找到
 */
static mqtt_msg_cb_t mqtt_find_callback(const char *topic, uint16_t topic_len)
{
    for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (!sub_table[i].active) continue;
        uint16_t slen = (uint16_t)strlen(sub_table[i].topic);
        if (slen == topic_len &&
            strncmp(sub_table[i].topic, topic, topic_len) == 0) {
            return sub_table[i].callback;
        }
    }
    return NULL;
}

/* ===================== 公开 API ===================== */

void mqtt_init(void)
{
    memset(sub_table, 0, sizeof(sub_table));
    default_callback = NULL;
}

int mqtt_subscribe(const char *topic, uint8_t qos, mqtt_msg_cb_t callback)
{
    /* 检查是否已订阅（可更新回调） */
    for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (sub_table[i].active &&
            strcmp(sub_table[i].topic, topic) == 0) {
            sub_table[i].callback = callback;
            return 0;
        }
    }

    /* 找空位 */
    int8_t slot = -1;
    for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (!sub_table[i].active) { slot = (int8_t)i; break; }
    }
    if (slot < 0) {
        u0_printf("[MQTT] Sub table full!\r\n");
        return -1;
    }

    /* 发送 AT+MQTTSUB */
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=0,\"%s\",%u\r\n", topic, qos);
    if (!ESP8266_SendCmd(cmd, "OK", 5000)) {
        u0_printf("[MQTT] Subscribe '%s' failed\r\n", topic);
        return -2;
    }

    /* 记录到表 */
    strncpy(sub_table[slot].topic, topic, MQTT_TOPIC_MAX_LEN - 1);
    sub_table[slot].topic[MQTT_TOPIC_MAX_LEN - 1] = '\0';
    sub_table[slot].callback = callback;
    sub_table[slot].active   = 1;

    u0_printf("[MQTT] Subscribed: %s\r\n", topic);
    return 0;
}

int mqtt_unsubscribe(const char *topic)
{
    int8_t idx = -1;
    for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (sub_table[i].active &&
            strcmp(sub_table[i].topic, topic) == 0) {
            idx = (int8_t)i;
            break;
        }
    }
    if (idx < 0) return -1;

    /* 发送 AT+MQTTUNSUB */
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "AT+MQTTUNSUB=0,\"%s\"\r\n", topic);
    if (!ESP8266_SendCmd(cmd, "OK", 5000)) {
        u0_printf("[MQTT] Unsubscribe '%s' failed\r\n", topic);
        return -2;
    }

    sub_table[idx].active = 0;
    u0_printf("[MQTT] Unsubscribed: %s\r\n", topic);
    return 0;
}


int mqtt_publish_json(const char *topic, const char *json_data, uint8_t qos, uint8_t retain)
{
    char cmd[128];
    uint16_t len = strlen(json_data);

    // 1. 发送 RAW 模式请求
    snprintf(cmd, sizeof(cmd), "AT+MQTTPUBRAW=0,\"%s\",%u,%u,%u\r\n", 
             topic, len, qos, retain);
    
    // 这里需要修改你的 SendCmd，让它支持等待 ">" 符号
    if (!ESP8266_SendCmd(cmd, ">", 2000)) {
        return -1;
    }

    // 2. 收到 ">" 后，直接发送纯数据（不要加回车换行，不要加引号）
    u1_send_bytes((uint8_t*)json_data, len);
    if (!ESP8266_SendCmd(cmd, "OK", 5000)) {
        u0_printf("[MQTT] Publish to '%s' failed\r\n", topic);
        return -2;
    }
    
    // 3. 等待最终的 OK
    // 这里视你的底层实现而定，可能需要检测是否返回了 "OK"
    return 0;
}



int mqtt_publish(const char *topic, const char *data, uint8_t qos, uint8_t retain)
{
    /* AT+MQTTPUB=0,"<topic>","<data>",<qos>,<retain> */
    static char cmd[1100];
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTPUB=0,\"%s\",\"%s\",%u,%u\r\n",
             topic, data, qos, retain);
             
    if (!ESP8266_SendCmd(cmd, "OK", 5000)) {
        u0_printf("[MQTT] Publish to '%s' failed\r\n", topic);
        return -1;
    }
    return 0;
}

int mqtt_publish_bin(const char *topic, const uint8_t *data, uint16_t len,
                     uint8_t qos, uint8_t retain)
{
    /* 二进制数据转 hex 字符串后发布 */
    static char hex_buf[MQTT_PAYLOAD_BUF_SIZE * 2 + 1];
    if (len > MQTT_PAYLOAD_BUF_SIZE) {
        u0_printf("[MQTT] Data too large (%u > %u)\r\n", len, MQTT_PAYLOAD_BUF_SIZE);
        return -1;
    }

    for (uint16_t i = 0; i < len; i++) {
        snprintf(&hex_buf[i * 2], 3, "%02X", data[i]);
    }
    hex_buf[len * 2] = '\0';

    return mqtt_publish(topic, hex_buf, qos, retain);
}

void mqtt_poll(void)
{
    while (u1_ucb.out != u1_ucb.in) {
        uint16_t len = u1_ucb.out->ed - u1_ucb.out->st + 1;

        /* 安全拷贝到临时缓冲区 */
        char tmp[MQTT_PAYLOAD_BUF_SIZE];
        uint16_t copy_len = (len < sizeof(tmp) - 1) ? len : (uint16_t)(sizeof(tmp) - 1);
        memcpy(tmp, u1_ucb.out->st, copy_len);
        tmp[copy_len] = '\0';

        /* 消费这一帧 */
        u1_ucb.out++;
        if (u1_ucb.out > u1_ucb.end) {
            u1_ucb.out = &u1_ucb.uart_infro_buf[0];
        }

        /* 解析 +MQTTSUBRECV */
        const char *topic   = NULL;
        const char *payload = NULL;
        uint16_t topic_len  = 0;
        uint16_t pay_len    = 0;

        if (!mqtt_parse_subrecv(tmp, copy_len, &topic, &topic_len,
                                &payload, &pay_len)) {
            /* 不是 MQTT 推送 (可能是 AT 响应)，打印调试 */
            u0_printf("[U1] %s\r\n", tmp);
            continue;
        }

        /* 临时构造以 '\0' 结尾的 topic 字符串 */
        char topic_str[MQTT_TOPIC_MAX_LEN];
        uint16_t tlen = (topic_len < MQTT_TOPIC_MAX_LEN - 1)
                        ? topic_len : (uint16_t)(MQTT_TOPIC_MAX_LEN - 1);
        memcpy(topic_str, topic, tlen);
        topic_str[tlen] = '\0';

        /* 查找回调 */
        mqtt_msg_cb_t cb = mqtt_find_callback(topic, topic_len);
        if (cb) {
            cb(topic_str, payload, pay_len);
        } else if (default_callback) {
            default_callback(topic_str, payload, pay_len);
        } else {
            u0_printf("[MQTT] Unhandled: topic=%s, len=%u\r\n", topic_str, pay_len);
        }
    }
}

void mqtt_set_default_callback(mqtt_msg_cb_t callback)
{
    default_callback = callback;
}

uint8_t mqtt_get_sub_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (sub_table[i].active) count++;
    }
    return count;
}
