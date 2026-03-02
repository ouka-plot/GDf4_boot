/*!
    \file    main.c
    \brief   APP entry - Serial-based MQTT publisher for OneNET

    \version 2026-03-02, V2.2.0

    Serial Commands:
      TEMP <value>     - Set temperature (e.g: TEMP 25.5)
      POWER <0|1>      - Set power state (e.g: POWER 1)
      SEND             - Send data to OneNET
      HELP             - Show help
*/

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sensor simulation variables - OneNET properties */
static float sensor_temperature = 25.0f;
static int power_state = 1;  /* 1 = ON, 0 = OFF */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
  return Get_Tick();
}

/**
 * @brief Build OneNET property JSON (OneJSON format, no time field)
 * Format: {
 *   "id": "123",
 *   "version": "1.0",
 *   "params": {
 *     "Switch": { "value": true },
 *     "temperature": { "value": 25.1 }
 *   }
 * }
 * Note: Switch is bool (true/false), temperature is float
 * time field omitted - platform uses server receive time
 */
static int build_property_json(char *buf, uint16_t buf_size, uint32_t msg_id,
                               float temp, int power) {
  return snprintf(buf, buf_size,
                  "{\"id\":\"%lu\",\"version\":\"1.0\","
                  "\"params\":{"
                  "\"Switch\":{\"value\":%s},"
                  "\"temperature\":{\"value\":%.1f}"
                  "}}",
                  (unsigned long)msg_id, 
                  power ? "true" : "false",
                  temp);
}

/**
 * @brief Parse integer from JSON (simple parser)
 */
static int json_get_int(const char *json, uint16_t json_len, const char *key, int *out) {
  char pattern[48];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char *p = strstr(json, pattern);
  if (!p) return -1;
  p += strlen(pattern);
  while (*p == ' ') p++;
  *out = atoi(p);
  return 0;
  (void)json_len;
}

/**
 * @brief Handle property post reply - confirms successful push to OneNET
 */
static void on_property_post_reply(const char *topic, const char *payload, uint16_t len) {
  int code = -1;
  json_get_int(payload, len, "code", &code);
  u0_printf("[REPLY] Property post reply: code=%d\r\n", code);
  if (code != 200) {
    u0_printf("[REPLY] WARNING: Platform rejected property\r\n");
  }
}

/**
 * @brief Handle property set from platform - reply with set_reply
 * Platform sends: {"id":"123","version":"1.0","params":{"Switch":true}}
 * Device must reply: {"id":"123","code":200,"msg":"success"}
 */
static void on_property_set(const char *topic, const char *payload, uint16_t len) {
  u0_printf("[SET] Platform set: %s\r\n", payload);
  
  /* Extract id from payload */
  char reply[128];
  const char *id_start = strstr(payload, "\"id\":\"");
  char id_buf[16] = "0";
  if (id_start) {
    id_start += 6; /* skip "id":" (6 chars) to point at the value */
    const char *id_end = strchr(id_start, '"');
    if (id_end && (id_end - id_start) < (int)sizeof(id_buf)) {
      memcpy(id_buf, id_start, id_end - id_start);
      id_buf[id_end - id_start] = '\0';
    }
  }
  
  /* Parse Switch value if present */
  if (strstr(payload, "\"Switch\"")) {
    if (strstr(payload, "true")) {
      power_state = 1;
      u0_printf("[SET] Switch -> ON\r\n");
    } else {
      power_state = 0;
      u0_printf("[SET] Switch -> OFF\r\n");
    }
  }
  
  /* Reply on set_reply topic */
  snprintf(reply, sizeof(reply),
           "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id_buf);
  mqtt_publish_json(ONENET_TOPIC_PROP_SET_REPLY, reply, 0, 0);

  /* Report updated state back to platform */
  {
    char json_buf[256];
    build_property_json(json_buf, sizeof(json_buf), mqtt.MessageID++,
                        sensor_temperature, power_state);
    mqtt_publish_json(ONENET_TOPIC_PROP_POST, json_buf, 0, 0);
    u0_printf("[SET] Reported updated state: Switch=%d\r\n", power_state);
  }
}

/**
 * @brief Default callback for unhandled topics
 */
static void on_default_msg(const char *topic, const char *payload, uint16_t len) {
  u0_printf("[MQTT] Unhandled topic: %s\r\n", topic);
}

int main(void) {
  /* Relocate VTOR to APP address */
  SCB->VTOR = APP_ADDR;
  __DSB();
  __ISB();

  /* Enable interrupts (disabled by Bootloader) */
  __enable_irq();

  /* Initialize hardware */
  systick_config();
  delay_init();
  usart0_init(921600);
  iic_init();

  u0_printf("\r\n[APP] Running from 0x%08X\r\n", APP_ADDR);

  /* Read network configuration from EEPROM */
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

  /* Initialize ESP8266 UART and external Flash */
  usart1_init(115200);
  gd25q40e_init();

  /* Override MQTT host/port with hardcoded OneNET */
  strncpy(net_cfg.mqtt_host, IPADDR, sizeof(net_cfg.mqtt_host) - 1);
  net_cfg.mqtt_port = atoi(PORTNUMBER);

  /* Generate OneNET authentication token */
  u0_printf("\r\n========== Calling password_Init() ==========\r\n");
  password_Init();
  u0_printf("========== password_Init() completed ==========\r\n\r\n");

  /* Verify MQTT credentials */
  u0_printf("[MAIN] MQTT Credentials:\r\n");
  u0_printf("[MAIN] client_id = %s\r\n", mqtt.client_id);
  u0_printf("[MAIN] username = %s\r\n", mqtt.user_name);
  u0_printf("[MAIN] password length = %u\r\n", (unsigned)strlen(mqtt.pass_word));

  /* Initialize ESP8266 and connect to MQTT broker */
  esp8266_err_t err = ESP8266_Init(&net_cfg);
  if (err != ESP_OK) {
    u0_printf("[APP] ESP8266 init failed (err=%d), MQTT disabled\r\n", err);
    while (1) {
      delay_ms(1000);
    }
  }

  /* Initialize MQTT module and subscribe to topics */
  mqtt_init();
  
  /* 等待MQTT连接稳定 */
  delay_ms(1000);
  
  /* 必须订阅这些topic才能正常通信 */
  u0_printf("[MQTT] Subscribing topics...\r\n");
  
  if (mqtt_subscribe(ONENET_TOPIC_PROP_SET, 0, on_property_set) != 0) {
    u0_printf("[MQTT] Subscribe property/set FAILED!\r\n");
  }
  delay_ms(500);
  if (mqtt_subscribe(ONENET_TOPIC_PROP_REPLY, 0, on_property_post_reply) != 0) {
    u0_printf("[MQTT] Subscribe post_reply FAILED!\r\n");
  }
  
  mqtt_set_default_callback(on_default_msg);

  u0_printf("[APP] MQTT ready, subscriptions active\r\n");
  u0_printf("[APP] Type HELP for commands\r\n");

  /* Main loop */
  while (1) {
    /* Poll MQTT messages */
    mqtt_poll();

    /* Process serial commands */
    while (u0_ucb.out != u0_ucb.in) {
      uint16_t flen = u0_ucb.out->ed - u0_ucb.out->st + 1;
      char cmd_line[128];
      uint16_t copy = (flen < sizeof(cmd_line) - 1) ? flen : (uint16_t)(sizeof(cmd_line) - 1);
      memcpy(cmd_line, u0_ucb.out->st, copy);
      cmd_line[copy] = '\0';

      u0_ucb.out++;
      if (u0_ucb.out > u0_ucb.end) {
        u0_ucb.out = &u0_ucb.uart_infro_buf[0];
      }

      /* TEMP <value> - Set temperature */
      if (strncmp(cmd_line, "TEMP ", 5) == 0) {
        sensor_temperature = (float)atof(cmd_line + 5);
        u0_printf("[CMD] Temperature set to %.1f\r\n", sensor_temperature);
      }
      /* POWER <0|1> - Set power state */
      else if (strncmp(cmd_line, "POWER ", 6) == 0) {
        power_state = atoi(cmd_line + 6) ? 1 : 0;
        u0_printf("[CMD] Power set to %d\r\n", power_state);
      }
      /* SEND - Publish data to OneNET */
      else if (strncmp(cmd_line, "SEND", 4) == 0) {
        char json_buf[256];
        build_property_json(json_buf, sizeof(json_buf), mqtt.MessageID++,
                            sensor_temperature, power_state);
        int ret = mqtt_publish_json(ONENET_TOPIC_PROP_POST, json_buf, 0, 0);
        if (ret == 0) {
          u0_printf("[SEND] Data published OK\r\n");
          u0_printf("  Power=%d, Temp=%.1f\r\n",
                    power_state, sensor_temperature);
        } else {
          u0_printf("[SEND] Publish failed (ret=%d)\r\n", ret);
        }
      }
      /* HELP - Show command list */
      else if (strncmp(cmd_line, "HELP", 4) == 0) {
        u0_printf("\r\n===== [APP] Serial Commands =====\r\n");
        u0_printf("  TEMP <value>  - Set temperature (e.g: TEMP 25.5)\r\n");
        u0_printf("  POWER <0|1>   - Set power state (e.g: POWER 1)\r\n");
        u0_printf("  SEND          - Publish data to OneNET\r\n");
        u0_printf("  HELP          - Show this help\r\n");
        u0_printf("==================================\r\n\r\n");
      }
    }

    delay_ms(5);
  }
}
