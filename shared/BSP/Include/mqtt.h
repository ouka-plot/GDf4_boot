/**
 * @file   mqtt.h
 * @brief  MQTT 
 *

 *
 * : esp8266.h (ESP8266_SendCmd), usart.h (u1_ucb)
 *
 * @version 2026-02-26, V1.0.0
 */
#ifndef __MQTT_H__
#define __MQTT_H__

#include "gd32f4xx.h"
#include <stdint.h>


#define MQTT_MAX_SUBSCRIPTIONS   8    
#define MQTT_TOPIC_MAX_LEN      48   
#define MQTT_PAYLOAD_BUF_SIZE  512   


/**
 * @brief MQTT 
 * @param topic       
 * @param payload    
 * @param payload_len 
 */
typedef void (*mqtt_msg_cb_t)(const char *topic,
                               const char *payload,
                               uint16_t    payload_len);

/**
 * @brief 
 */
typedef struct {
    char          topic[MQTT_TOPIC_MAX_LEN];  
    mqtt_msg_cb_t callback;                   
    uint8_t       active;                      
} mqtt_sub_entry_t;

/* ===================== API ===================== */

/**
 * @brief  
 */
void mqtt_init(void);


int mqtt_subscribe(const char *topic, uint8_t qos, mqtt_msg_cb_t callback);


int mqtt_unsubscribe(const char *topic);


int mqtt_publish(const char *topic, const char *data, uint8_t qos, uint8_t retain);


int mqtt_publish_bin(const char *topic, const uint8_t *data, uint16_t len,
                     uint8_t qos, uint8_t retain);


void mqtt_poll(void);


int mqtt_publish_json(const char *topic, const char *json_data, uint8_t qos, uint8_t retain);





void mqtt_set_default_callback(mqtt_msg_cb_t callback);

uint8_t mqtt_get_sub_count(void);

#endif /* __MQTT_H__ */
