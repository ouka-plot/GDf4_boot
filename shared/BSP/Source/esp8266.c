#include "esp8266.h"
#include "AT24c256.h"
#include "mqtt.h"
#include "net_config.h"
#include "ota_types.h"
#include "usart.h"

// 全局变量定义
ota_mqtt_state_t ota_state = OTA_IDLE;
uint32_t ota_total_size = 0;
uint32_t ota_received = 0;
uint32_t ota_ext_offset = 256;
uint16_t ota_page_num = 1;


esp8266_err_t ESP8266_Init(const NetConfig *cfg) {
  char cmd_buf[512];

    /* 1. AT 指令测试 */
  uint8_t at_ok = 0;
  for (uint8_t i = 0; i < 3; i++) {
    if (ESP8266_SendCmd("AT\r\n", "OK", 1000)) {
      at_ok = 1;
      break;
    }
    delay_ms(500);
  }
  if (!at_ok) {
    u0_printf("[ESP] AT no response\r\n");
    return ESP_ERR_AT;
  }
  u0_printf("[ESP] ESP8266 Ready\r\n");

  ESP8266_SendCmd("ATE0\r\n", "OK", 500);           
  ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 500);    

    /* 2. WiFi 连接 */
  snprintf(cmd_buf, sizeof(cmd_buf), "AT+CWJAP=\"%s\",\"%s\"\r\n",
           cfg->wifi_ssid, cfg->wifi_pass);
  if (!ESP8266_SendCmd(cmd_buf, "WIFI GOT IP", 15000)) {
    u0_printf("[ESP] WiFi connect failed\r\n");
    return ESP_ERR_WIFI;
  }
  u0_printf("[ESP] WiFi Connected\r\n");

    /* 3. MQTT 配置 */
  ESP8266_SendCmd("AT+MQTTCLEAN=0\r\n", "OK", 500);     
  delay_ms(500);


  int use_ssl = 0;

  if (cfg->mqtt_host && strstr(cfg->mqtt_host, "mqtts.heclouds.com") != NULL) {
    use_ssl = 0;    
    u0_printf("[ESP] OneNET detected, using TCP (not TLS)\r\n");
  } else if (cfg->mqtt_host && strstr(cfg->mqtt_host, "mqtts.") != NULL) {
    use_ssl = 1;
  }

  /**
   * 1=MQTT over TCP, 2=MQTT over TLS (no cert verify),
   * 3=MQTT over TLS (verify server cert), 4=MQTT over TLS (provide client cert) */
  int scheme = use_ssl ? 2 : 1; 

  /* Build MQTTUSERCFG with client_id, username, and password */
  snprintf(cmd_buf, sizeof(cmd_buf),
           "AT+MQTTUSERCFG=0,%d,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n", 
           scheme, mqtt.client_id, mqtt.user_name, mqtt.pass_word);
  
  u0_printf("[ESP] MQTT config: client_id=%s, username=%s, password_len=%u\r\n",
            mqtt.client_id, mqtt.user_name, (unsigned)strlen(mqtt.pass_word));
  
  if (!ESP8266_SendCmd(cmd_buf, "OK", 5000)) {
    u0_printf("[ESP] MQTT config failed\r\n");
    return ESP_ERR_MQTT_CFG;
  }
  delay_ms(200);
/**
  * Set MQTT keepalive = 60 seconds 
  * if (!ESP8266_SendCmd("AT+MQTTCONNCFG=0,60\r\n", "OK", 5000)) {
  * u0_printf("[ESP] MQTT keepalive config failed, continue anyway\r\n");
  * }
  * delay_ms(200);  */
  {
    const char *host = cfg->mqtt_host;
    uint16_t port = cfg->mqtt_port;
    if (port == 0) {
      port = 1883;  /* 默认端口 */
    }
    delay_ms(500);
    u0_printf("[ESP] Connecting to %s:%u (ssl=%d) ...\r\n", host, port,
              use_ssl);

    /* reconnect=1: auto-reconnect on disconnection */
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+MQTTCONN=0,\"%s\",%u,1\r\n",
             host, port);
  }
  {
    uint8_t conn_ok = 0;
    for (uint8_t retry = 0; retry <3 ; retry++) {
      if (retry > 0) {
        u0_printf("[ESP] MQTT connect retry %u/3...\r\n", retry + 1);
        delay_ms(5000);
      }
    
      if (ESP8266_SendCmd(cmd_buf, "OK", 30000)) {
        conn_ok = 1;
        break;
      }
    }
    if (!conn_ok) {
      u0_printf("[ESP] MQTT connect failed after 3 tries\r\n");
      return ESP_ERR_MQTT_CONN;
    }
  }
  u0_printf("[ESP] MQTT Connected, ready\r\n");
  return ESP_OK;
}


static uint8_t hex_char_to_nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 0;
}
static uint16_t hex_decode(const char *hex, uint16_t hex_len, uint8_t *out) {
  uint16_t bin_len = hex_len / 2;
  for (uint16_t i = 0; i < bin_len; i++) {
    out[i] = (hex_char_to_nibble(hex[2 * i]) << 4) |
             hex_char_to_nibble(hex[2 * i + 1]);
  }
  return bin_len;
}

/**

 */
void ota_handle_start(const char *payload, uint16_t len) {

  const char *p = payload + 10;
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

  
  uint8_t blocks = (ota_total_size + 256 + 65535) / 65536; /* +256 for header */
  if (blocks > 8)
    blocks = 8;
  u0_printf("[OTA] Start, size=%lu, erasing %d blocks...\r\n", ota_total_size,
            blocks);
  for (uint8_t b = 0; b < blocks; b++) {
    gd25_erase64k(b);
  }

  ota_received = 0;
  ota_ext_offset = 256; 
  ota_page_num = 1;
  ota_state = OTA_RECEIVING;
  u0_printf("[OTA] Flash erased, ready to receive\r\n");
}


void ota_handle_data(const char *payload, uint16_t len) {
  static uint8_t page_buf[256];
  static uint16_t buf_pos = 0;


  uint8_t tmp[512];
  uint16_t bin_len = hex_decode(payload, len, tmp);

  for (uint16_t i = 0; i < bin_len; i++) {
    page_buf[buf_pos++] = tmp[i];
    ota_received++;


    if (buf_pos >= 256) {
      gd25_pagewrite(page_buf, ota_page_num);
      ota_page_num++;
      ota_ext_offset += 256;
      buf_pos = 0;
      memset(page_buf, 0xFF, 256);
    }
  }

 
  if (ota_received >= ota_total_size && buf_pos > 0) {
    memset(&page_buf[buf_pos], 0xFF, 256 - buf_pos);
    gd25_pagewrite(page_buf, ota_page_num);
    buf_pos = 0;
  }

  u0_printf("[OTA] %lu / %lu bytes\r\n", ota_received, ota_total_size);
}

/**
 * 
 */
void ota_handle_end(void) {
  u0_printf("[OTA] Transfer complete, writing header...\r\n");

  
  OTA_Header hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.magic = OTA_HEADER_MAGIC; /* 0xAA55AA55 */
  hdr.seg_count = 1;
  hdr.total_len = ota_received;
  hdr.header_crc32 = 0;             
  hdr.segs[0].target_addr = APP_ADDR; 
  hdr.segs[0].data_len = ota_received;
  hdr.segs[0].ext_offset = 256; 
  hdr.segs[0].crc32 = 0;       


  uint8_t hdr_buf[256];
  memset(hdr_buf, 0xFF, 256);
  memcpy(hdr_buf, &hdr, OTA_HEADER_SIZE);
  gd25_pagewrite(hdr_buf, 0);


  ota_info.boot_flag = BOOT_FLAG_SET; /* 0x12345678 */
  ota_info.header_addr = 0;
  AT24_WriteOTAInfo();

  u0_printf("[OTA] Done! Rebooting...\r\n");
  delay_ms(200);
  NVIC_SystemReset();
}

void mqtt_ota_callback(const char *topic, const char *payload,
                       uint16_t pay_len) {
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


/**
 * @brief  等待ESP8266响应（不发送命令）
 */
uint8_t ESP8266_WaitResp(char *expect_reply, uint32_t timeout) {
  uint32_t start = Get_Tick();
  while ((Get_Tick() - start) < timeout) {
    if (u1_ucb.out != u1_ucb.in) {
      uint16_t len = u1_ucb.out->ed - u1_ucb.out->st + 1;
      char tmp[256];
      uint16_t copy_len = (len < sizeof(tmp) - 1) ? len : (sizeof(tmp) - 1);
      memcpy(tmp, u1_ucb.out->st, copy_len);
      tmp[copy_len] = '\0';
      u0_printf("[RECV] %s\r\n", tmp);
      if (strstr(tmp, expect_reply) != NULL) {
        u1_ucb.out++;
        if (u1_ucb.out > u1_ucb.end) {
          u1_ucb.out = &u1_ucb.uart_infro_buf[0];
        }
        return 1;
      }
      u1_ucb.out++;
      if (u1_ucb.out > u1_ucb.end) {
        u1_ucb.out = &u1_ucb.uart_infro_buf[0];
      }
    }
  }
  return 0;
}

uint8_t ESP8266_SendCmd(char *cmd, char *expect_reply, uint32_t timeout) {

  u1_ucb.out = (uart_rxbuff_ptr *)u1_ucb.in;

  // 2. ����ָ��
  u0_printf("[CMD] %s", cmd); 
  u1_send_string(cmd);        


  uint32_t start = Get_Tick();
  while ((Get_Tick() - start) < timeout) {
    if (u1_ucb.out != u1_ucb.in) {
 
      uint16_t len = u1_ucb.out->ed - u1_ucb.out->st + 1;

    
      char tmp[256];
      uint16_t copy_len = (len < sizeof(tmp) - 1) ? len : (sizeof(tmp) - 1);
      memcpy(tmp, u1_ucb.out->st, copy_len);
      tmp[copy_len] = '\0';

      u0_printf("[RECV] %s\r\n", tmp);

   
      if (strstr(tmp, expect_reply) != NULL) {
        return 1; 
      }
      if (strstr(tmp, "ERROR") != NULL &&
          strcmp(cmd, "AT+MQTTCLEAN=0\r\n") == 0) {
        return 2;
      }

   
      u1_ucb.out++;
      if (u1_ucb.out > u1_ucb.end) {
        u1_ucb.out = &u1_ucb.uart_infro_buf[0];
      }
    }
  }
  u0_printf("[ERR] Timeout!\r\n");
  return 0;
}
