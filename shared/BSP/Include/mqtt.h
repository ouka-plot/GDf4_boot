/**
 * @file   mqtt.h
 * @brief  MQTT 应用层 — 订阅/发布/消息分发
 *
 * 基于 ESP8266 AT 固件的 MQTT 指令集，提供：
 *   - 多主题订阅 + 回调分发
 *   - 消息发布 (字符串 / 二进制)
 *   - 主循环轮询接收
 *
 * 依赖: esp8266.h (ESP8266_SendCmd), usart.h (u1_ucb)
 *
 * @version 2026-02-26, V1.0.0
 */
#ifndef __MQTT_H__
#define __MQTT_H__

#include "gd32f4xx.h"
#include <stdint.h>

/* ===================== 配置 ===================== */
#define MQTT_MAX_SUBSCRIPTIONS   8    /* 最多同时订阅的主题数      */
#define MQTT_TOPIC_MAX_LEN      48    /* 主题字符串最大长度        */
#define MQTT_PAYLOAD_BUF_SIZE  512    /* 单条消息 payload 缓冲区  */

/* ===================== 类型 ===================== */

/**
 * @brief MQTT 消息回调函数类型
 * @param topic       接收到的主题 (以 '\0' 结尾)
 * @param payload     消息内容 (不保证以 '\0' 结尾)
 * @param payload_len 消息长度
 */
typedef void (*mqtt_msg_cb_t)(const char *topic,
                               const char *payload,
                               uint16_t    payload_len);

/**
 * @brief 订阅表项
 */
typedef struct {
    char          topic[MQTT_TOPIC_MAX_LEN];  /* 主题过滤器      */
    mqtt_msg_cb_t callback;                    /* 消息到达回调    */
    uint8_t       active;                      /* 是否有效 0/1    */
} mqtt_sub_entry_t;

/* ===================== API ===================== */

/**
 * @brief  初始化 MQTT 模块（清空订阅表）
 *         应在 ESP8266_Init() 成功后调用
 */
void mqtt_init(void);

/**
 * @brief  订阅主题并绑定回调
 * @param  topic    主题字符串, 例 "device/cmd"
 * @param  qos      QoS 等级 (0 / 1 / 2)
 * @param  callback 收到该主题消息时的处理函数, NULL 表示仅订阅不处理
 * @retval  0 成功
 * @retval -1 订阅表已满
 * @retval -2 AT 指令失败
 */
int mqtt_subscribe(const char *topic, uint8_t qos, mqtt_msg_cb_t callback);

/**
 * @brief  取消订阅
 * @param  topic   要取消的主题
 * @retval  0 成功
 * @retval -1 未找到该主题
 * @retval -2 AT 指令失败
 */
int mqtt_unsubscribe(const char *topic);

/**
 * @brief  发布消息 (字符串)
 * @param  topic   发布主题
 * @param  data    消息内容 (C 字符串, '\0' 结尾)
 * @param  qos     QoS 等级 (0 / 1 / 2)
 * @param  retain  是否保留消息 (0 / 1)
 * @retval  0 成功
 * @retval -1 AT 指令失败
 */
int mqtt_publish(const char *topic, const char *data, uint8_t qos, uint8_t retain);

/**
 * @brief  发布二进制数据 (先转 hex 再发)
 * @param  topic   发布主题
 * @param  data    二进制数据指针
 * @param  len     数据长度
 * @param  qos     QoS 等级
 * @param  retain  是否保留
 * @retval  0 成功
 * @retval -1 AT 指令失败
 */
int mqtt_publish_bin(const char *topic, const uint8_t *data, uint16_t len,
                     uint8_t qos, uint8_t retain);

/**
 * @brief  主循环轮询: 解析 USART1 收到的 +MQTTSUBRECV 并分发回调
 *         应在 while(1) 中周期调用
 */
void mqtt_poll(void);

/**
 * @brief  发布 JSON 格式字符串 (data 必须是合法 JSON)
 * @param  topic   发布主题
 * @param  json_data JSON 字符串 (必须以 '\0' 结尾)
 * @param  qos     QoS 等级
 * @param  retain  是否保留
 * @retval  0 成功
 * @retval -1 AT 指令失败
 */
int mqtt_publish_json(const char *topic, const char *json_data, uint8_t qos, uint8_t retain);




/**
 * @brief  设置默认回调 (当收到的主题不在订阅表中时触发)
 * @param  callback  默认处理函数, NULL 表示忽略未注册主题
 */
void mqtt_set_default_callback(mqtt_msg_cb_t callback);

/**
 * @brief  获取当前有效订阅数
 */
uint8_t mqtt_get_sub_count(void);

#endif /* __MQTT_H__ */
