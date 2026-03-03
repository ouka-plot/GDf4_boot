/**
 * @file   net_config.c
 * @brief  
 */
#include "net_config.h"
#include "AT24c256.h"
#include "usart.h"
#ifdef USE_MQTT_TOKEN
#include "utils_hmac.h"
#endif
#include <string.h>
#include <stdio.h>


NetConfig net_cfg;
MQTT_CB mqtt;
TOKEN_CB token;

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

void URL_Encode(char *data, int data_len, char *outdata) {
    int i, j = 0;
    for (i = 0; i < data_len; i++) {
        unsigned char c = data[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            outdata[j++] = c;
        } else {
            sprintf(&outdata[j], "%%%02X", c);
            j += 3;
        }
    }
    outdata[j] = '\0';
}

#ifdef USE_MQTT_TOKEN
void password_Init(void) {
    memset(&token, 0, sizeof(token));
    memset(&mqtt, 0, sizeof(mqtt));

    u0_printf("[DEBUG] Step 1: Structures cleared\r\n");

    /* Always use hardcoded DeviceName */
    strncpy(mqtt.client_id, DeviceName, sizeof(mqtt.client_id) - 1);
    u0_printf("[DEBUG] Step 2: client_id = %s\r\n", mqtt.client_id);

    /* Always use hardcoded ProductID */
    strncpy(mqtt.user_name, ProductID, sizeof(mqtt.user_name) - 1);
    u0_printf("[DEBUG] Step 3: user_name = %s\r\n", mqtt.user_name);

    /* Always generate OneNET token */
    u0_printf("[DEBUG] Step 4: Generating OneNET token...\r\n");

        /* Decode device key */
        int decode_len = base64_decode(DeviceKey, token.decodekey);
        u0_printf("[DEBUG] Step 5: base64_decode returned %d bytes\r\n", decode_len);

        /* Build res string */
        snprintf(token.res, sizeof(token.res), "products/%s/devices/%s", ProductID, DeviceName);
        u0_printf("[DEBUG] Step 6: token.res = %s\r\n", token.res);

        /* Build StringForSignature: et\nmethod\nres\nversion */
        snprintf(token.StringForSignature, sizeof(token.StringForSignature),
                 "%s\nsha1\n%s\n2018-10-31", UNIX, token.res);
        u0_printf("[DEBUG] Step 7: StringForSignature length = %u\r\n", (unsigned)strlen(token.StringForSignature));

        /* Calculate HMAC-SHA1 */
        utils_hmac_sha1_hex(token.StringForSignature, strlen(token.StringForSignature),
                           token.signtemp, (char*)token.decodekey, decode_len);
        u0_printf("[DEBUG] Step 8: HMAC-SHA1 calculated\r\n");

        /* Base64 encode the signature */
        base64_encode((unsigned char*)token.signtemp, token.sign, 20);
        u0_printf("[DEBUG] Step 9: token.sign = %s\r\n", token.sign);

        /* URL encode res and sign */
        URL_Encode(token.res, strlen(token.res), token.resURL);
        u0_printf("[DEBUG] Step 10: token.resURL = %s\r\n", token.resURL);

        URL_Encode(token.sign, strlen(token.sign), token.signURL);
        u0_printf("[DEBUG] Step 11: token.signURL = %s\r\n", token.signURL);

        /* Build final token */
        int written = snprintf(mqtt.pass_word, sizeof(mqtt.pass_word),
                 "version=2018-10-31&res=%s&et=%s&method=sha1&sign=%s",
                 token.resURL, UNIX, token.signURL);
        u0_printf("[DEBUG] Step 12: snprintf returned %d, pass_word length = %u\r\n",
                  written, (unsigned)strlen(mqtt.pass_word));

    mqtt.MessageID = 1;

    u0_printf("[TOKEN] Client ID: %s\r\n", mqtt.client_id);
    u0_printf("[TOKEN] Username: %s\r\n", mqtt.user_name);
    u0_printf("[TOKEN] Password length: %u\r\n", (unsigned)strlen(mqtt.pass_word));
    u0_printf("[TOKEN] Password (first 100 chars): %.100s\r\n", mqtt.pass_word);
    u0_printf("[TOKEN] Password hex dump (first 32 bytes):\r\n");
    for (int i = 0; i < 32 && i < (int)strlen(mqtt.pass_word); i++) {
        u0_printf("%02X ", (unsigned char)mqtt.pass_word[i]);
        if ((i + 1) % 16 == 0) u0_printf("\r\n");
    }
    u0_printf("\r\n");

    /* 生成 HTTP OTA Token (使用与 MQTT 相同的设备级认证) */
    u0_printf("[DEBUG] Generating HTTP OTA token (device-level auth)...\r\n");

    /* HTTP OTA 使用与 MQTT 相同的 token 格式和凭据 */
    /* res = products/{ProductID}/devices/{DeviceName} */
    /* 签名密钥 = DeviceKey (已在上面解码) */

    TOKEN_CB token_ota;
    memset(&token_ota, 0, sizeof(token_ota));

    /* 复用 MQTT 的解码密钥 */
    memcpy(token_ota.decodekey, token.decodekey, decode_len);

    /* 构造 res: products/{ProductID}/devices/{DeviceName} */
    snprintf(token_ota.res, sizeof(token_ota.res),
             "products/%s/devices/%s", ProductID, DeviceName);
    u0_printf("[DEBUG] OTA res = %s\r\n", token_ota.res);

    /* 构造签名字符串: {et}\nsha1\n{res}\n2018-10-31 (与 MQTT 格式一致) */
    snprintf(token_ota.StringForSignature, sizeof(token_ota.StringForSignature),
             "%s\nsha1\n%s\n2018-10-31", UNIX, token_ota.res);
    u0_printf("[DEBUG] OTA StringForSignature length = %u\r\n",
              (unsigned)strlen(token_ota.StringForSignature));

    /* HMAC-SHA1 签名 */
    utils_hmac_sha1_hex(token_ota.StringForSignature, strlen(token_ota.StringForSignature),
                       token_ota.signtemp, (char*)token_ota.decodekey, decode_len);
    u0_printf("[DEBUG] OTA HMAC-SHA1 calculated\r\n");

    /* Base64 编码签名结果 */
    base64_encode((unsigned char*)token_ota.signtemp, token_ota.sign, 20);
    u0_printf("[DEBUG] OTA sign = %s\r\n", token_ota.sign);

    /* URL 编码 res 和 sign */
    URL_Encode(token_ota.res, strlen(token_ota.res), token_ota.resURL);
    URL_Encode(token_ota.sign, strlen(token_ota.sign), token_ota.signURL);

    /* 构造最终 Token (版本号与 MQTT 一致: 2018-10-31) */
    snprintf(mqtt.pass_word_ota, sizeof(mqtt.pass_word_ota),
             "version=2018-10-31&res=%s&et=%s&method=sha1&sign=%s",
             token_ota.resURL, UNIX, token_ota.signURL);

    u0_printf("[HTTP OTA TOKEN] Length: %u\r\n", (unsigned)strlen(mqtt.pass_word_ota));
    u0_printf("[HTTP OTA TOKEN] %.100s...\r\n", mqtt.pass_word_ota);
}
#endif /* USE_MQTT_TOKEN */
