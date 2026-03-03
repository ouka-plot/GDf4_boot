/**
 * @file   onenet_http_ota.c
 * @brief  OneNET HTTP OTA 升级实现
 *
 * 基于 ESP8266 AT 指令的 HTTP 客户端功能实现 OTA 升级
 *
 * @version 2026-03-02, V1.0.0
 */

#include "onenet_http_ota.h"
#include "esp8266.h"
#include "usart.h"
#include "gd25q40e.h"
#include "AT24c256.h"
#include "ota_types.h"
#include "systick.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ===================== 私有变量 ===================== */
static char http_response_buf[HTTP_OTA_RESPONSE_BUF];
static uint32_t http_resp_len = 0;

/* ===================== Flash 驱动适配器 ===================== */

/**
 * @brief  擦除外部 Flash 的一个扇区（64KB）
 * @param  sector_num  扇区号 (0 = 0x0, 1 = 0x10000, ...)
 * @retval 0 = 成功
 */
static uint8_t GD25Q40E_Erase_Sector(uint32_t sector_num)
{
    gd25_erase64k((uint8_t)sector_num);
    return 0;
}

/**
 * @brief  写入数据到外部 Flash 的任意地址
 * @param  buf     数据缓冲区
 * @param  addr    目标地址 (字节)
 * @param  len     写入长度 (字节)
 * @retval 0 = 成功, 非0 = 失败
 * 
 * 注：gd25_pagewrite() 要求每页 256 字节，所以这里分页写入
 */
static uint8_t GD25Q40E_Write(uint8_t *buf, uint32_t addr, uint32_t len)
{
    uint32_t pagenum = addr / 256;     /* 计算起始页号 */
    uint32_t offset = addr % 256;      /* 页内偏移 */
    uint32_t remain = len;
    uint8_t *src = buf;

    while (remain > 0) {
        uint32_t write_len = (remain > (256 - offset)) ? (256 - offset) : remain;

        /* 简化：直接写入，不读后改写（因为已经擦除过）*/
        if (offset == 0 && write_len == 256) {
            /* 整页写入 */
            gd25_pagewrite(src, (uint16_t)pagenum);
        } else {
            /* 部分页写入：需要读后改写 */
            uint8_t page_buf[256];
            gd25_read(page_buf, pagenum * 256, 256);
            memcpy(page_buf + offset, src, write_len);
            gd25_pagewrite(page_buf, (uint16_t)pagenum);
        }

        src += write_len;
        remain -= write_len;
        pagenum++;
        offset = 0;  /* 后续页统一从页首开始 */
    }

    return 0;
}

/* ===================== 内部辅助函数 ===================== */

/**
 * @brief  等待 ESP8266 空闲（处理 busy p... 状态）
 * @retval 1=空闲  0=仍然忙
 */
static uint8_t wait_esp8266_ready(void)
{
    for (uint8_t i = 0; i < 10; i++) {
        if (ESP8266_SendCmd("AT\r\n", "OK", 1500)) {
            return 1;
        }
        delay_ms(500);
    }
    u0_printf("[HTTP] ESP8266 still busy after 15s\r\n");
    return 0;
}

/**
 * @brief  强制等待 ESP8266 完全空闲（用于 CIPSTART 之后）
 *         ESP8266 的 busy p... 需要等待内部操作完成
 */
static void force_wait_idle(void)
{
    for (uint8_t i = 0; i < 20; i++) {
        if (ESP8266_SendCmd("AT\r\n", "OK", 2000)) {
            return;
        }
        delay_ms(1000);
    }
}

/**
 * @brief  网络连通性诊断
 *         测试网络可达性
 */
static void http_diagnose_network(const char *host)
{
    char cmd[128];

    u0_printf("\r\n===== Network Diagnostics =====\r\n");

    /* 1. 检查 WiFi 状态 */
    ESP8266_SendCmd("AT+CWJAP?\r\n", "OK", 3000);
    delay_ms(200);

    /* 2. 查看当前 IP 地址 */
    ESP8266_SendCmd("AT+CIFSR\r\n", "OK", 3000);
    delay_ms(200);

    /* 3. Ping 测试 */
    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"\r\n", host);
    if (ESP8266_SendCmd(cmd, "+", 5000)) {
        u0_printf("[DIAG] Ping %s OK\r\n", host);
    } else {
        u0_printf("[DIAG] Ping %s FAIL\r\n", host);
    }
    delay_ms(500);
    wait_esp8266_ready();

    /* 4. 测试到公网的基本连通性 (baidu.com:80) */
    u0_printf("[DIAG] Testing basic TCP to baidu.com:80...\r\n");
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
    delay_ms(200);
    if (ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"www.baidu.com\",80\r\n", "CONNECT", 15000)) {
        u0_printf("[DIAG] Basic TCP OK - internet is reachable\r\n");
        ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    } else {
        u0_printf("[DIAG] Basic TCP FAIL - possible network/firewall issue\r\n");
        force_wait_idle();
        ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    }
    delay_ms(500);
    wait_esp8266_ready();

    u0_printf("===== Diagnostics End =====\r\n\r\n");
}

/**
 * @brief  建立 TCP 连接到 OneNET HTTP 服务器
 * @param  host  服务器地址
 * @param  port  端口号
 * @retval 1=成功  0=失败
 */
/**
 * @brief  连接到 HTTP 服务器
 * @param  host  主机名或 IP
 * @param  port  端口号
 * @param  skip_ready_check  是否跳过ready检查（已经验证过ESP8266稳定时使用）
 * @retval 1=成功  0=失败
 */
static uint8_t http_connect_ex(const char *host, uint16_t port, uint8_t skip_ready_check)
{
    char cmd[256];

    if (!skip_ready_check) {
        wait_esp8266_ready();
    }

    /* 设置单连接模式 - 增加超时和重试 */
    u0_printf("[HTTP] Setting single connection mode...\r\n");
    for (uint8_t i = 0; i < 3; i++) {
        if (ESP8266_SendCmd("AT+CIPMUX=0\r\n", "OK", 3000)) {
            u0_printf("[HTTP] CIPMUX=0 OK\r\n");
            break;
        }
        u0_printf("[HTTP] CIPMUX=0 retry %u/3...\r\n", i + 1);
        delay_ms(1000);
        if (i == 2) {
            u0_printf("[HTTP] CIPMUX=0 failed after 3 attempts\r\n");
            return 0;
        }
    }
    delay_ms(500);

    /* 关闭可能存在的连接 */
    u0_printf("[HTTP] Closing any existing connection...\r\n");
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    delay_ms(500);

    if (!skip_ready_check) {
        wait_esp8266_ready();
    }

    /* 建立 TCP 连接 (带重试) */
    for (uint8_t retry = 0; retry < 3; retry++) {
        if (!skip_ready_check) {
            wait_esp8266_ready();
        }

        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                 host, port);
        u0_printf("[HTTP] Attempt %u: %s", retry + 1, cmd);

        if (ESP8266_SendCmd(cmd, "CONNECT", 30000)) {
            u0_printf("[HTTP] Connected to %s:%u\r\n", host, port);
            return 1;
        }

        /* 等待 ESP8266 处理完 CIPSTART（可能仍在内部重试 TCP）*/
        force_wait_idle();

        /* 检查是否虽然超时但实际已连接 */
        if (ESP8266_SendCmd("AT+CIPSTATUS\r\n", "STATUS:3", 2000)) {
            u0_printf("[HTTP] Connected (late) to %s:%u\r\n", host, port);
            return 1;
        }

        u0_printf("[HTTP] Attempt %u failed\r\n", retry + 1);

        /* 强制关闭并等待 */
        ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
        delay_ms(2000);
    }

    /* 所有重试失败，运行诊断 */
    http_diagnose_network(host);

    return 0;
}

/* 兼容旧代码的包装函数 */
static uint8_t http_connect(const char *host, uint16_t port)
{
    return http_connect_ex(host, port, 0);
}

/**
 * @brief  发送 HTTP 请求并接收响应
 * @param  request  HTTP 请求字符串（完整的 HTTP 报文）
 * @param  timeout  超时时间（毫秒）
 * @retval 1=成功  0=失败
 */
static uint8_t http_send_request(const char *request, uint32_t timeout)
{
    char cmd[64];
    uint16_t req_len = strlen(request);

    /* ===== 步骤1：发送 AT+CIPSEND 之前，完全清空接收缓存 ===== */
    u1_ucb.out = u1_ucb.in;
    delay_ms(50);

    /* AT+CIPSEND=<length> - 使用 strlen 计算的实际长度 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", req_len);
    u0_printf("[HTTP] Sending: %s", cmd);

    /* 发送 CIPSEND 命令 */
    u1_send_string(cmd);

    /* ===== 步骤2：等待 > 提示符（自己实现，不用 ESP8266_SendCmd）===== */
    uint32_t start_tick = Get_Tick();
    uint8_t got_prompt = 0;
    char temp_buf[128];
    uint16_t temp_len = 0;

    while ((Get_Tick() - start_tick) < 5000) {
        if (u1_ucb.out != u1_ucb.in) {
            uint16_t frame_len = u1_ucb.out->ed - u1_ucb.out->st + 1;
            if (temp_len + frame_len < sizeof(temp_buf) - 1) {
                memcpy(&temp_buf[temp_len], u1_ucb.out->st, frame_len);
                temp_len += frame_len;
                temp_buf[temp_len] = '\0';
            }
            u1_ucb.out++;
            if (u1_ucb.out > u1_ucb.end) {
                u1_ucb.out = &u1_ucb.uart_infro_buf[0];
            }
            /* 检查是否收到 > */
            if (strchr(temp_buf, '>') != NULL) {
                got_prompt = 1;
                break;
            }
        }
        delay_ms(10);
    }

    if (!got_prompt) {
        u0_printf("[HTTP] CIPSEND failed (no > prompt)\r\n");
        return 0;
    }

    u0_printf("[HTTP] Got >, sending %u bytes data...\r\n", req_len);

    /* ===== 步骤3：清空缓存，准备接收 SEND OK ===== */
    u1_ucb.out = u1_ucb.in;
    delay_ms(100);

    /* ===== 步骤4：发送 HTTP 请求数据 ===== */
    u1_send_string((char *)request);

    /* ===== 步骤5：等待 SEND OK（累积多帧）===== */
    start_tick = Get_Tick();
    temp_len = 0;
    memset(temp_buf, 0, sizeof(temp_buf));

    while ((Get_Tick() - start_tick) < 10000) {
        if (u1_ucb.out != u1_ucb.in) {
            uint16_t frame_len = u1_ucb.out->ed - u1_ucb.out->st + 1;
            if (temp_len + frame_len < sizeof(temp_buf) - 1) {
                memcpy(&temp_buf[temp_len], u1_ucb.out->st, frame_len);
                temp_len += frame_len;
                temp_buf[temp_len] = '\0';
            }
            u1_ucb.out++;
            if (u1_ucb.out > u1_ucb.end) {
                u1_ucb.out = &u1_ucb.uart_infro_buf[0];
            }
            /* 检查是否收到 SEND OK */
            if (strstr(temp_buf, "SEND OK") != NULL) {
                u0_printf("[HTTP] SEND OK received\r\n");
                break;
            }
            /* 检查是否出错 */
            if (strstr(temp_buf, "ERROR") != NULL) {
                u0_printf("[HTTP] Send ERROR\r\n");
                return 0;
            }
        }
        delay_ms(10);
    }

    if (strstr(temp_buf, "SEND OK") == NULL) {
        u0_printf("[HTTP] Send failed (no SEND OK, got: %s)\r\n", temp_buf);
        return 0;
    }

    /* ===== 步骤6：接收 HTTP 响应数据 ===== */
    http_resp_len = 0;
    memset(http_response_buf, 0, sizeof(http_response_buf));

    start_tick = Get_Tick();
    uint8_t receiving = 0;
    uint32_t last_recv_tick = 0;

    while ((Get_Tick() - start_tick) < timeout) {
        if (u1_ucb.out != u1_ucb.in) {
            receiving = 1;
            last_recv_tick = Get_Tick();

            uint16_t frame_len = u1_ucb.out->ed - u1_ucb.out->st + 1;

            if (http_resp_len + frame_len < sizeof(http_response_buf) - 1) {
                memcpy(&http_response_buf[http_resp_len], u1_ucb.out->st, frame_len);
                http_resp_len += frame_len;
            }

            u1_ucb.out++;
            if (u1_ucb.out > u1_ucb.end) {
                u1_ucb.out = &u1_ucb.uart_infro_buf[0];
            }

            /* 检查是否接收完成（连接关闭或收到完整响应）*/
            if (strstr(http_response_buf, "CLOSED") != NULL) {
                break;
            }
            /* Keep-Alive 模式：检查是否收到了完整的 HTTP 响应（有 \r\n\r\n）*/
            if (strstr(http_response_buf, "\r\n\r\n") != NULL) {
                /* 简单判断：如果收到了 HTTP 头结束标记，再等 200ms 收剩余数据 */
                delay_ms(200);
                /* 继续收剩余数据 */
                while (u1_ucb.out != u1_ucb.in) {
                    uint16_t fl = u1_ucb.out->ed - u1_ucb.out->st + 1;
                    if (http_resp_len + fl < sizeof(http_response_buf) - 1) {
                        memcpy(&http_response_buf[http_resp_len], u1_ucb.out->st, fl);
                        http_resp_len += fl;
                    }
                    u1_ucb.out++;
                    if (u1_ucb.out > u1_ucb.end) u1_ucb.out = &u1_ucb.uart_infro_buf[0];
                }
                break;
            }
        } else if (receiving && (Get_Tick() - last_recv_tick) > 300) {
            /* 已经开始接收但 300ms 没有新数据，认为接收完成 */
            break;
        }
        delay_ms(5);
    }

    if (http_resp_len == 0) {
        u0_printf("[HTTP] No response received\r\n");
        return 0;
    }

    u0_printf("[HTTP] Response: %u bytes\r\n", http_resp_len);
    return 1;
}

/**
 * @brief  从 HTTP 响应中查找关键字
 * @param  keyword  要查找的关键字
 * @retval 指向关键字的指针，NULL=未找到
 */
static const char* http_find_keyword(const char *keyword)
{
    return strstr(http_response_buf, keyword);
}

/**
 * @brief  从 JSON 响应中提取字符串字段值
 * @param  json      JSON 字符串
 * @param  key       字段名（例如 "target"）
 * @param  out_buf   输出缓冲区
 * @param  buf_size  缓冲区大小
 * @retval 1=成功  0=失败
 */
static uint8_t json_extract_string(const char *json, const char *key,
                                    char *out_buf, uint16_t buf_size)
{
    /* 查找 "key":" */
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"", key);

    const char *start = strstr(json, search_pattern);
    if (start == NULL) {
        return 0;
    }

    start += strlen(search_pattern);  /* 跳过 "key":" */

    /* 查找结束引号 */
    const char *end = strchr(start, '\"');
    if (end == NULL) {
        return 0;
    }

    uint16_t len = (uint16_t)(end - start);
    if (len >= buf_size) {
        len = buf_size - 1;
    }

    memcpy(out_buf, start, len);
    out_buf[len] = '\0';

    return 1;
}

/**
 * @brief  从 JSON 响应中提取整数字段值
 * @param  json  JSON 字符串
 * @param  key   字段名（例如 "size"）
 * @param  value [out] 输出值
 * @retval 1=成功  0=失败
 */
static uint8_t json_extract_int(const char *json, const char *key, uint32_t *value)
{
    /* 查找 "key": */
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);

    u0_printf("[JSON] Searching for: %s\r\n", search_pattern);

    const char *start = strstr(json, search_pattern);
    if (start == NULL) {
        u0_printf("[JSON] Pattern not found in buffer!\r\n");
        /* 打印缓冲区前 200 字节用于调试 */
        u0_printf("[JSON] Buffer content (first 200 chars):\r\n");
        for (int i = 0; i < 200 && json[i] != '\0'; i++) {
            u0_printf("%c", json[i]);
        }
        u0_printf("\r\n");
        return 0;
    }

    start += strlen(search_pattern);

    /* 跳过空格 */
    while (*start == ' ') start++;

    /* 打印找到的位置周围内容 */
    u0_printf("[JSON] Found at position, next 20 chars: ");
    for (int i = 0; i < 20 && start[i] != '\0'; i++) {
        u0_printf("%c", start[i]);
    }
    u0_printf("\r\n");

    /* 解析数字 */
    *value = (uint32_t)atoi(start);
    u0_printf("[JSON] Parsed value: %u\r\n", (unsigned int)*value);

    return 1;
}

/* ===================== 公开 API 实现 ===================== */

/**
 * @brief  步骤一：上报设备当前版本号
 */
http_ota_err_t http_ota_report_version(const char *product_id,
                                        const char *device_name,
                                        const char *s_version,
                                        const char *f_version,
                                        const char *auth_token)
{
    if (!product_id || !device_name || !s_version || !f_version || !auth_token) {
        return HTTP_OTA_ERR_INVALID_PARAM;
    }

    /* 连接到 OneNET OTA 服务器 */
    if (!http_connect("iot-api.heclouds.com", 80)) {
        return HTTP_OTA_ERR_CONNECT;
    }

    /* 构造 JSON body */
    char json_body[128];
    snprintf(json_body, sizeof(json_body),
             "{\"s_version\":\"%s\", \"f_version\":\"%s\"}",
             s_version, f_version);

    uint16_t body_len = strlen(json_body);

    /* 构造 HTTP POST 请求 */
    char http_request[1024];
    snprintf(http_request, sizeof(http_request),
             "POST /fuse-ota/%s/%s/version HTTP/1.1\r\n"
             "Host: iot-api.heclouds.com\r\n"
             "Authorization: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %u\r\n"
             "\r\n"
             "%s",
             product_id, device_name, auth_token, body_len, json_body);

    u0_printf("[HTTP OTA] Reporting version: %s\r\n", s_version);

    /* 发送请求并接收响应 */
    if (!http_send_request(http_request, 10000)) {
        return HTTP_OTA_ERR_SEND;
    }

    /* 打印响应内容用于调试 */
    http_response_buf[http_resp_len < sizeof(http_response_buf) - 1 ? http_resp_len : sizeof(http_response_buf) - 1] = '\0';
    u0_printf("[HTTP OTA] Response body:\r\n%s\r\n", http_response_buf);

    /* 检查响应是否包含 "msg":"succ" */
    if (http_find_keyword("\"msg\":\"succ\"") != NULL) {
        u0_printf("[HTTP OTA] Version reported successfully\r\n");

        /* ===== 关键修复：关闭TCP连接 ===== */
        u0_printf("[HTTP OTA] Closing connection after report...\r\n");
        ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
        delay_ms(500);

        return HTTP_OTA_OK;
    }

    u0_printf("[HTTP OTA] Version report failed\r\n");

    /* ===== 失败时也要关闭连接 ===== */
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    delay_ms(500);

    return HTTP_OTA_ERR_PARSE;
}

/**
 * @brief  步骤二：查询是否有 OTA 升级任务
 */
http_ota_err_t http_ota_check_update(const char *product_id,
                                      const char *device_name,
                                      const char *current_ver,
                                      const char *auth_token,
                                      http_ota_task_t *task_info)
{
    if (!product_id || !device_name || !current_ver || !auth_token || !task_info) {
        return HTTP_OTA_ERR_INVALID_PARAM;
    }

    /* 初始化任务信息 */
    memset(task_info, 0, sizeof(http_ota_task_t));

    /* 连接到 OneNET OTA 服务器 */
    if (!http_connect("iot-api.heclouds.com", 80)) {
        return HTTP_OTA_ERR_CONNECT;
    }

    /* 构造 HTTP GET 请求 
     * type=1: 差分升级, type=2: MCU整包升级, type=3: 模组固件
     * 根据平台配置的升级包类型选择 */
    char http_request[1024];
    snprintf(http_request, sizeof(http_request),
             "GET /fuse-ota/%s/%s/check?type=2&version=%s HTTP/1.1\r\n"
             "Host: iot-api.heclouds.com\r\n"
             "Authorization: %s\r\n"
             "\r\n",
             product_id, device_name, current_ver, auth_token);

    u0_printf("[HTTP OTA] Checking for updates (current: %s)...\r\n", current_ver);

    /* 打印完整的 HTTP 请求用于调试 */
    u0_printf("[HTTP OTA] Request (len=%u):\r\n%s\r\n", (unsigned int)strlen(http_request), http_request);

    /* 发送请求并接收响应 */
    if (!http_send_request(http_request, 10000)) {
        return HTTP_OTA_ERR_SEND;
    }

    /* 打印响应内容用于调试 */
    http_response_buf[http_resp_len < sizeof(http_response_buf) - 1 ? http_resp_len : sizeof(http_response_buf) - 1] = '\0';
    u0_printf("[HTTP OTA] Check response:\r\n%s\r\n", http_response_buf);

    /* 情况 1：无任务 - 检查 "msg":"not exist" */
    if (http_find_keyword("\"msg\":\"not exist\"") != NULL) {
        u0_printf("[HTTP OTA] No OTA task available\r\n");
        return HTTP_OTA_ERR_NO_TASK;
    }

    /* 情况 2：任务已过期 - 检查 "task expire" */
    if (http_find_keyword("task expire") != NULL) {
        u0_printf("[HTTP OTA] OTA task expired, please create new task on OneNET\r\n");
        return HTTP_OTA_ERR_NO_TASK;
    }

    /* 情况 3：有任务 - 检查 "msg":"succ" */
    if (http_find_keyword("\"msg\":\"succ\"") == NULL) {
        u0_printf("[HTTP OTA] Check update failed (unexpected response)\r\n");
        return HTTP_OTA_ERR_PARSE;
    }

    /* 提取任务信息 */
    if (!json_extract_string(http_response_buf, "target",
                             task_info->target_version, HTTP_OTA_VERSION_LEN)) {
        u0_printf("[HTTP OTA] Failed to extract target version\r\n");
        return HTTP_OTA_ERR_PARSE;
    }

    /* tid 是整数字段，需要先提取为 uint32_t 再转换为字符串 */
    uint32_t tid_value = 0;
    if (!json_extract_int(http_response_buf, "tid", &tid_value)) {
        u0_printf("[HTTP OTA] Failed to extract task ID\r\n");
        return HTTP_OTA_ERR_PARSE;
    }
    snprintf(task_info->task_id, HTTP_OTA_TID_LEN, "%u", (unsigned int)tid_value);

    if (!json_extract_int(http_response_buf, "size", &task_info->firmware_size)) {
        u0_printf("[HTTP OTA] Failed to extract firmware size\r\n");
        return HTTP_OTA_ERR_PARSE;
    }

    /* MD5 是可选的 */
    json_extract_string(http_response_buf, "md5", task_info->md5, 33);

    task_info->valid = 1;

    u0_printf("[HTTP OTA] New version available: %s (tid=%s, size=%u)\r\n",
              task_info->target_version, task_info->task_id, task_info->firmware_size);

    /* ===== 关键修复：关闭TCP连接，防止下一个函数卡死 ===== */
    u0_printf("[HTTP OTA] Closing connection after check...\r\n");
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
    delay_ms(500);

    return HTTP_OTA_OK;
}

/**
 * @brief  步骤三：分片下载固件数据
 */
http_ota_err_t http_ota_download_firmware(const char *product_id,
                                           const char *device_name,
                                           const char *task_id,
                                           const char *auth_token,
                                           uint32_t total_size)
{
    if (!product_id || !device_name || !task_id || !auth_token || total_size == 0) {
        return HTTP_OTA_ERR_INVALID_PARAM;
    }

    u0_printf("[HTTP OTA] Starting firmware download (size=%u bytes)...\r\n", total_size);

    /* 计算需要擦除的扇区数（每个扇区 64KB）*/
    uint32_t total_flash_size = total_size + 256;  /* 包含 OTA Header */
    uint32_t sectors_needed = (total_flash_size + 65535) / 65536;  /* 向上取整 */

    /* ===== 关键修复：擦除前断开WiFi，防止电流干扰 ===== */
    u0_printf("[HTTP OTA] Disconnecting WiFi before Flash erase...\r\n");

    /* 先检查连接状态，避免对已关闭的连接执行CIPCLOSE */
    u0_printf("[HTTP OTA] Checking connection status...\r\n");
    if (ESP8266_SendCmd("AT+CIPSTATUS\r\n", "STATUS:3", 1000)) {
        /* 连接存在，需要关闭 */
        u0_printf("[HTTP OTA] Connection active, closing...\r\n");
        ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
        delay_ms(500);
    } else {
        /* 连接已关闭或不存在 */
        u0_printf("[HTTP OTA] No active connection\r\n");
        delay_ms(200);
    }

    /* 断开WiFi连接（而不是sleep）*/
    u0_printf("[HTTP OTA] Disconnecting from WiFi...\r\n");
    ESP8266_SendCmd("AT+CWQAP\r\n", "OK", 3000);
    delay_ms(1000);

    /* 擦除所需的扇区 */
    u0_printf("[HTTP OTA] Erasing %u sectors (WiFi disconnected)...\r\n", sectors_needed);
    for (uint32_t i = 0; i < sectors_needed; i++) {
        u0_printf("[HTTP OTA] Erasing sector %u...\r\n", i);
        GD25Q40E_Erase_Sector(i);
        delay_ms(1000);  /* 增加延时，确保擦除完成且电流稳定 */
    }
    u0_printf("[HTTP OTA] Flash erase complete\r\n");

    /* ===== 擦除完成后重新连接WiFi ===== */
    u0_printf("[HTTP OTA] Reconnecting to WiFi...\r\n");

    /* 读取WiFi配置 */
    extern NetConfig net_cfg;
    char wifi_cmd[256];
    snprintf(wifi_cmd, sizeof(wifi_cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
             net_cfg.wifi_ssid, net_cfg.wifi_pass);

    if (!ESP8266_SendCmd(wifi_cmd, "OK", 15000)) {
        u0_printf("[HTTP OTA] WiFi reconnect failed\r\n");
        return HTTP_OTA_ERR_CONNECT;
    }

    u0_printf("[HTTP OTA] WiFi reconnected\r\n");
    delay_ms(3000);  /* 等待获取IP */

    /* 验证ESP8266是否正常 */
    u0_printf("[HTTP OTA] Verifying ESP8266 status...\r\n");
    if (!ESP8266_SendCmd("AT\r\n", "OK", 2000)) {
        u0_printf("[HTTP OTA] ESP8266 not responding after erase\r\n");
        return HTTP_OTA_ERR_CONNECT;
    }
    u0_printf("[HTTP OTA] ESP8266 ready after erase\r\n");

    /* WiFi重连后ESP8266已经完全正常，简化验证 */
    u0_printf("[HTTP OTA] Waiting for network to stabilize...\r\n");
    delay_ms(2000);

    uint32_t downloaded = 0;
    uint32_t ext_flash_offset = 256;  /* 数据从 256 字节后开始写入 */
    uint8_t chunk_buf[HTTP_OTA_CHUNK_SIZE];

    /* ===== 建立一次 TCP 连接，使用 Keep-Alive ===== */
    u0_printf("[HTTP OTA] Establishing TCP connection for download...\r\n");
    if (!http_connect_ex("iot-api.heclouds.com", 80, 1)) {
        u0_printf("[HTTP OTA] Initial connection failed\r\n");
        return HTTP_OTA_ERR_CONNECT;
    }

    uint8_t connection_alive = 1;
    uint8_t retry_count = 0;
    const uint8_t max_retries = 3;

    while (downloaded < total_size) {
        uint32_t chunk_size = HTTP_OTA_CHUNK_SIZE;
        if (downloaded + chunk_size > total_size) {
            chunk_size = total_size - downloaded;
        }

        /* 如果连接断开，重新建立 */
        if (!connection_alive) {
            u0_printf("[HTTP OTA] Reconnecting (retry %u/%u)...\r\n", retry_count + 1, max_retries);
            delay_ms(1000);
            if (!http_connect_ex("iot-api.heclouds.com", 80, 0)) {
                retry_count++;
                if (retry_count >= max_retries) {
                    u0_printf("[HTTP OTA] Max retries reached\r\n");
                    return HTTP_OTA_ERR_CONNECT;
                }
                continue;
            }
            connection_alive = 1;
            retry_count = 0;
        }

        /* 构造带 Range 和 Keep-Alive 的 HTTP GET 请求 */
        char http_request[1024];
        snprintf(http_request, sizeof(http_request),
                 "GET /fuse-ota/%s/%s/%s/download HTTP/1.1\r\n"
                 "Host: iot-api.heclouds.com\r\n"
                 "Authorization: %s\r\n"
                 "Range: bytes=%u-%u\r\n"
                 "Connection: keep-alive\r\n"
                 "\r\n",
                 product_id, device_name, task_id, auth_token,
                 downloaded, downloaded + chunk_size - 1);

        /* 打印进度 */
        uint32_t chunk_num = downloaded / HTTP_OTA_CHUNK_SIZE + 1;
        uint32_t total_chunks = (total_size + HTTP_OTA_CHUNK_SIZE - 1) / HTTP_OTA_CHUNK_SIZE;
        u0_printf("[HTTP OTA] Chunk %u/%u (offset=%u)\r\n", chunk_num, total_chunks, downloaded);

        /* 发送请求并接收响应 */
        if (!http_send_request(http_request, 15000)) {
            u0_printf("[HTTP OTA] Download chunk failed at offset %u\r\n", downloaded);
            return HTTP_OTA_ERR_SEND;
        }

        /* 步骤一：屏蔽打印 - 只打印接收长度 */
        u0_printf("[HTTP OTA] Received length: %u\r\n", http_resp_len);

        /* 步骤二：检查 HTTP 状态码是否为 206 Partial Content */
        if (strstr(http_response_buf, "HTTP/1.1 206") == NULL &&
            strstr(http_response_buf, "HTTP/1.0 206") == NULL) {
            u0_printf("[HTTP OTA] Error: Expected HTTP 206, got different status\r\n");
            return HTTP_OTA_ERR_PARSE;
        }

        /* 步骤三：提取 Content-Length（应该等于 chunk_size = 256）*/
        const char *content_len_str = strstr(http_response_buf, "Content-Length:");
        uint32_t content_length = 0;
        if (content_len_str != NULL) {
            content_length = (uint32_t)atoi(content_len_str + 15);  /* 跳过 "Content-Length:" */
            u0_printf("[HTTP OTA] Content-Length: %u\r\n", content_length);
        } else {
            u0_printf("[HTTP OTA] Warning: No Content-Length header found\r\n");
            content_length = chunk_size;  /* 默认使用请求的大小 */
        }

        /* 步骤四：查找 HTTP 头结束标志 \r\n\r\n */
        const char *body_start = strstr(http_response_buf, "\r\n\r\n");
        if (body_start == NULL) {
            u0_printf("[HTTP OTA] Invalid HTTP response (no \\r\\n\\r\\n marker)\r\n");
            return HTTP_OTA_ERR_PARSE;
        }

        body_start += 4;  /* 跳过 \r\n\r\n */

        /* 计算 HTTP 头长度 */
        uint32_t header_len = (uint32_t)(body_start - http_response_buf);
        u0_printf("[HTTP OTA] HTTP header: %u bytes\r\n", header_len);

        /* 步骤五：精确拷贝 Content-Length 指定的字节数（通常是 256）*/
        uint32_t actual_data_len = http_resp_len - header_len;

        /* 使用 Content-Length 作为实际要写入的长度 */
        uint32_t write_len = content_length;

        /* 安全检查：确保不超过实际接收的数据 */
        if (write_len > actual_data_len) {
            u0_printf("[HTTP OTA] Warning: Content-Length (%u) > actual data (%u), using actual\r\n",
                      write_len, actual_data_len);
            write_len = actual_data_len;
        }

        /* 安全检查：确保不超过 chunk_size */
        if (write_len > chunk_size) {
            u0_printf("[HTTP OTA] Warning: write_len (%u) > chunk_size (%u), truncating\r\n",
                      write_len, chunk_size);
            write_len = chunk_size;
        }

        if (write_len == 0) {
            u0_printf("[HTTP OTA] Error: no firmware data in response\r\n");
            return HTTP_OTA_ERR_PARSE;
        }

        /* 精确拷贝固件数据到缓冲区 */
        memcpy(chunk_buf, body_start, write_len);

        u0_printf("[HTTP OTA] Writing %u bytes to Flash at offset %u\r\n",
                  write_len, ext_flash_offset);

        /* 写入外部 Flash */
        u0_printf("[HTTP OTA] Calling GD25Q40E_Write...\r\n");
        if (GD25Q40E_Write(chunk_buf, ext_flash_offset, write_len) != 0) {
            u0_printf("[HTTP OTA] Flash write failed at offset %u\r\n", ext_flash_offset);
            return HTTP_OTA_ERR_FLASH_WRITE;
        }
        u0_printf("[HTTP OTA] Flash write OK\r\n");

        downloaded += write_len;
        ext_flash_offset += write_len;

        /* 避免浮点运算，使用整数百分比 */
        uint32_t progress_percent = (downloaded * 100) / total_size;
        u0_printf("[HTTP OTA] Progress: %u / %u (%u%%)\r\n",
                  downloaded, total_size, progress_percent);

        /* ===== Keep-Alive: 检测连接是否被服务器关闭 ===== */
        if (strstr(http_response_buf, "CLOSED") != NULL ||
            strstr(http_response_buf, "Connection: close") != NULL) {
            u0_printf("[HTTP OTA] Server closed connection, will reconnect\r\n");
            connection_alive = 0;
        }

        /* 短暂延时，让 ESP8266 处理 */
        delay_ms(50);
    }

    /* 下载完成，关闭连接 */
    u0_printf("[HTTP OTA] Download complete, closing connection...\r\n");
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
    delay_ms(200);

    u0_printf("[HTTP OTA] Firmware download complete!\r\n");

    /* 写入 OTA Header 到外部 Flash 地址 0 */
    u0_printf("[HTTP OTA] Preparing OTA Header...\r\n");
    OTA_Header header;
    memset(&header, 0, sizeof(header));

    header.magic = OTA_HEADER_MAGIC;
    header.seg_count = 1;
    header.total_len = total_size;
    header.segs[0].target_addr = 0x08010000;  /* APP 起始地址 */
    header.segs[0].data_len = total_size;
    header.segs[0].ext_offset = 256;
    header.segs[0].crc32 = 0;  /* CRC32 暂时不计算 */
    header.header_crc32 = 0;

    u0_printf("[HTTP OTA] Writing OTA Header to Flash address 0...\r\n");
    if (GD25Q40E_Write((uint8_t *)&header, 0, sizeof(header)) != 0) {
        u0_printf("[HTTP OTA] Write OTA header failed\r\n");
        return HTTP_OTA_ERR_FLASH_WRITE;
    }
    u0_printf("[HTTP OTA] OTA Header written successfully\r\n");

    /* 验证 Magic Number - 读回并打印 */
    u0_printf("[HTTP OTA] Verifying OTA Header...\r\n");

    /* 方法 1：读取前 16 字节并逐字节打印 */
    uint8_t verify_buf[16];
    gd25_read(verify_buf, 0, 16);
    u0_printf("[HTTP OTA] Read back from Flash addr 0 (hex): ");
    for (int i = 0; i < 16; i++) {
        u0_printf("%02X ", verify_buf[i]);
    }
    u0_printf("\r\n");

    /* 方法 2：读取完整的 OTA_Header 结构体 */
    OTA_Header verify_header;
    gd25_read((uint8_t*)&verify_header, 0, sizeof(OTA_Header));
    u0_printf("[HTTP OTA] Read back Magic: 0x%08X\r\n", verify_header.magic);
    u0_printf("[HTTP OTA] Read back seg_count: %u\r\n", verify_header.seg_count);
    u0_printf("[HTTP OTA] Read back total_len: %u\r\n", verify_header.total_len);
    u0_printf("[HTTP OTA] Read back target_addr: 0x%08X\r\n", verify_header.segs[0].target_addr);
    u0_printf("[HTTP OTA] Read back data_len: %u\r\n", verify_header.segs[0].data_len);
    u0_printf("[HTTP OTA] Read back ext_offset: %u\r\n", verify_header.segs[0].ext_offset);

    /* 方法 3：直接读取 Magic Number（前 4 字节）*/
    uint32_t read_magic = 0;
    gd25_read((uint8_t*)&read_magic, 0, 4);
    u0_printf("[HTTP OTA] Read back Magic (direct): 0x%08X\r\n", read_magic);
    u0_printf("[HTTP OTA] Expected Magic: 0x%08X\r\n", OTA_HEADER_MAGIC);

    /* 验证 Magic Number 是否匹配 */
    if (read_magic == OTA_HEADER_MAGIC) {
        u0_printf("[HTTP OTA] *** Magic Number VERIFIED! Flash Write SUCCESS! ***\r\n");
    } else {
        u0_printf("[HTTP OTA] *** ERROR: Magic Number MISMATCH! Flash Write FAILED! ***\r\n");
        u0_printf("[HTTP OTA] This will cause Bootloader to fail!\r\n");
        return HTTP_OTA_ERR_FLASH_WRITE;
    }

    /* 设置 EEPROM 中的 OTA 标志 */
    u0_printf("[HTTP OTA] Setting OTA flag in EEPROM...\r\n");
    OTA_InfoCB ota_info;
    ota_info.boot_flag = BOOT_FLAG_SET;
    ota_info.header_addr = 0;

    AT24_write_page(0, (uint8_t *)&ota_info, sizeof(ota_info));
    delay_ms(10);
    u0_printf("[HTTP OTA] OTA flag set\r\n");

    u0_printf("[HTTP OTA] OTA upgrade ready, rebooting in 3 seconds...\r\n");
    delay_ms(3000);

    /* 重启设备 */
    NVIC_SystemReset();

    return HTTP_OTA_OK;
}

/**
 * @brief  完整的 HTTP OTA 流程（三步合一）
 */
http_ota_err_t http_ota_full_process(const char *product_id,
                                      const char *device_name,
                                      const char *current_ver,
                                      const char *auth_token)
{
    http_ota_err_t ret;

    /* 步骤一：上报版本号 */
    ret = http_ota_report_version(product_id, device_name,
                                   current_ver, current_ver, auth_token);
    if (ret != HTTP_OTA_OK) {
        u0_printf("[HTTP OTA] Step 1 failed: %d\r\n", ret);
        return ret;
    }

    delay_ms(500);

    /* 步骤二：查询 OTA 任务 */
    http_ota_task_t task_info;
    ret = http_ota_check_update(product_id, device_name,
                                 current_ver, auth_token, &task_info);

    if (ret == HTTP_OTA_ERR_NO_TASK) {
        u0_printf("[HTTP OTA] No update available\r\n");
        return ret;
    }

    if (ret != HTTP_OTA_OK) {
        u0_printf("[HTTP OTA] Step 2 failed: %d\r\n", ret);
        return ret;
    }

    delay_ms(500);

    /* 步骤三：下载固件 */
    ret = http_ota_download_firmware(product_id, device_name,
                                      task_info.task_id, auth_token,
                                      task_info.firmware_size);

    if (ret != HTTP_OTA_OK) {
        u0_printf("[HTTP OTA] Step 3 failed: %d\r\n", ret);
        return ret;
    }

    return HTTP_OTA_OK;
}

