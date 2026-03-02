/**
 * @file   mqtt.h
 * @brief  MQTT 搴旂敤灞� 鈥� 璁㈤槄/鍙戝竷/娑堟伅鍒嗗彂
 *
 * 鍩轰簬 ESP8266 AT 鍥轰欢鐨� MQTT 鎸囦护闆嗭紝鎻愪緵锛�
 *   - 澶氫富棰樿闃� + 鍥炶皟鍒嗗彂
 *   - 娑堟伅鍙戝竷 (瀛楃涓� / 浜岃繘鍒�)
 *   - 涓诲惊鐜疆璇㈡帴鏀�
 *
 * 渚濊禆: esp8266.h (ESP8266_SendCmd), usart.h (u1_ucb)
 *
 * @version 2026-02-26, V1.0.0
 */
#ifndef __MQTT_H__
#define __MQTT_H__

#include "gd32f4xx.h"
#include <stdint.h>

/* ===================== 閰嶇疆 ===================== */
#define MQTT_MAX_SUBSCRIPTIONS   8    /* 鏈€澶氬悓鏃惰闃呯殑涓婚鏁�      */
#define MQTT_TOPIC_MAX_LEN      48    /* 涓婚瀛楃涓叉渶澶ч暱搴�        */
#define MQTT_PAYLOAD_BUF_SIZE  512    /* 鍗曟潯娑堟伅 payload 缂撳啿鍖�  */

/* ===================== 绫诲瀷 ===================== */

/**
 * @brief MQTT 娑堟伅鍥炶皟鍑芥暟绫诲瀷
 * @param topic       鎺ユ敹鍒扮殑涓婚 (浠� '\0' 缁撳熬)
 * @param payload     娑堟伅鍐呭 (涓嶄繚璇佷互 '\0' 缁撳熬)
 * @param payload_len 娑堟伅闀垮害
 */
typedef void (*mqtt_msg_cb_t)(const char *topic,
                               const char *payload,
                               uint16_t    payload_len);

/**
 * @brief 璁㈤槄琛ㄩ」
 */
typedef struct {
    char          topic[MQTT_TOPIC_MAX_LEN];  /* 涓婚杩囨护鍣�      */
    mqtt_msg_cb_t callback;                    /* 娑堟伅鍒拌揪鍥炶皟    */
    uint8_t       active;                      /* 鏄惁鏈夋晥 0/1    */
} mqtt_sub_entry_t;

/* ===================== API ===================== */

/**
 * @brief  鍒濆鍖� MQTT 妯″潡锛堟竻绌鸿闃呰〃锛�
 *         搴斿湪 ESP8266_Init() 鎴愬姛鍚庤皟鐢�
 */
void mqtt_init(void);

/**
 * @brief  璁㈤槄涓婚骞剁粦瀹氬洖璋�
 * @param  topic    涓婚瀛楃涓�, 渚� "device/cmd"
 * @param  qos      QoS 绛夌骇 (0 / 1 / 2)
 * @param  callback 鏀跺埌璇ヤ富棰樻秷鎭椂鐨勫鐞嗗嚱鏁�, NULL 琛ㄧず浠呰闃呬笉澶勭悊
 * @retval  0 鎴愬姛
 * @retval -1 璁㈤槄琛ㄥ凡婊�
 * @retval -2 AT 鎸囦护澶辫触
 */
int mqtt_subscribe(const char *topic, uint8_t qos, mqtt_msg_cb_t callback);

/**
 * @brief  鍙栨秷璁㈤槄
 * @param  topic   瑕佸彇娑堢殑涓婚
 * @retval  0 鎴愬姛
 * @retval -1 鏈壘鍒拌涓婚
 * @retval -2 AT 鎸囦护澶辫触
 */
int mqtt_unsubscribe(const char *topic);

/**
 * @brief  鍙戝竷娑堟伅 (瀛楃涓�)
 * @param  topic   鍙戝竷涓婚
 * @param  data    娑堟伅鍐呭 (C 瀛楃涓�, '\0' 缁撳熬)
 * @param  qos     QoS 绛夌骇 (0 / 1 / 2)
 * @param  retain  鏄惁淇濈暀娑堟伅 (0 / 1)
 * @retval  0 鎴愬姛
 * @retval -1 AT 鎸囦护澶辫触
 */
int mqtt_publish(const char *topic, const char *data, uint8_t qos, uint8_t retain);

/**
 * @brief  鍙戝竷浜岃繘鍒舵暟鎹� (鍏堣浆 hex 鍐嶅彂)
 * @param  topic   鍙戝竷涓婚
 * @param  data    浜岃繘鍒舵暟鎹寚閽�
 * @param  len     鏁版嵁闀垮害
 * @param  qos     QoS 绛夌骇
 * @param  retain  鏄惁淇濈暀
 * @retval  0 鎴愬姛
 * @retval -1 AT 鎸囦护澶辫触
 */
int mqtt_publish_bin(const char *topic, const uint8_t *data, uint16_t len,
                     uint8_t qos, uint8_t retain);

/**
 * @brief  涓诲惊鐜疆璇�: 瑙ｆ瀽 USART1 鏀跺埌鐨� +MQTTSUBRECV 骞跺垎鍙戝洖璋�
 *         搴斿湪 while(1) 涓懆鏈熻皟鐢�
 */
void mqtt_poll(void);

/**
 * @brief  鍙戝竷 JSON 鏍煎紡瀛楃涓� (data 蹇呴』鏄悎娉� JSON)
 * @param  topic   鍙戝竷涓婚
 * @param  json_data JSON 瀛楃涓� (蹇呴』浠� '\0' 缁撳熬)
 * @param  qos     QoS 绛夌骇
 * @param  retain  鏄惁淇濈暀
 * @retval  0 鎴愬姛
 * @retval -1 AT 鎸囦护澶辫触
 */
int mqtt_publish_json(const char *topic, const char *json_data, uint8_t qos, uint8_t retain);




/**
 * @brief  璁剧疆榛樿鍥炶皟 (褰撴敹鍒扮殑涓婚涓嶅湪璁㈤槄琛ㄤ腑鏃惰Е鍙�)
 * @param  callback  榛樿澶勭悊鍑芥暟, NULL 琛ㄧず蹇界暐鏈敞鍐屼富棰�
 */
void mqtt_set_default_callback(mqtt_msg_cb_t callback);

/**
 * @brief  鑾峰彇褰撳墠鏈夋晥璁㈤槄鏁�
 */
uint8_t mqtt_get_sub_count(void);

#endif /* __MQTT_H__ */
