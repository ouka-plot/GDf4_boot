/**
 * @file   onenet_http_ota.h
 * @brief  OneNET HTTP OTA 升级接口
 *
 * 实现基于 HTTP 协议的 OneNET 平台 OTA 升级流程：
 *   步骤一：上报设备当前版本号 (POST /fuse-ota/{产品ID}/{设备名}/version)
 *   步骤二：查询是否有 OTA 任务 (GET /fuse-ota/{产品ID}/{设备名}/check)
 *   步骤三：分片下载固件数据 (GET /fuse-ota/{产品ID}/{设备名}/{tid}/download)
 *
 * @version 2026-03-02, V1.0.0
 */
#ifndef __ONENET_HTTP_OTA_H__
#define __ONENET_HTTP_OTA_H__

#include <stdint.h>
#include "gd32f4xx.h"

/* ===================== 配置参数 ===================== */
#define HTTP_OTA_CHUNK_SIZE     256     /* 每次下载的字节数 */
#define HTTP_OTA_RESPONSE_BUF   1024    /* HTTP 响应缓冲区大小 */
#define HTTP_OTA_VERSION_LEN    16      /* 版本号字符串长度 */
#define HTTP_OTA_TID_LEN        32      /* 任务 ID 字符串长度 */

/* ===================== 错误码 ===================== */
typedef enum {
    HTTP_OTA_OK = 0,              /* 成功 */
    HTTP_OTA_ERR_CONNECT,         /* TCP 连接失败 */
    HTTP_OTA_ERR_SEND,            /* 发送 HTTP 请求失败 */
    HTTP_OTA_ERR_TIMEOUT,         /* 接收超时 */
    HTTP_OTA_ERR_PARSE,           /* 解析响应失败 */
    HTTP_OTA_ERR_NO_TASK,         /* 无 OTA 任务 */
    HTTP_OTA_ERR_FLASH_WRITE,     /* Flash 写入失败 */
    HTTP_OTA_ERR_INVALID_PARAM,   /* 参数错误 */
} http_ota_err_t;

/* ===================== OTA 任务信息 ===================== */
typedef struct {
    char     target_version[HTTP_OTA_VERSION_LEN];  /* 目标版本号 */
    char     task_id[HTTP_OTA_TID_LEN];             /* 任务 ID (tid) */
    uint32_t firmware_size;                          /* 固件大小（字节）*/
    char     md5[33];                                /* MD5 校验值 */
    uint8_t  valid;                                  /* 任务信息是否有效 */
} http_ota_task_t;

/* ===================== API 函数 ===================== */

/**
 * @brief  步骤一：上报设备当前版本号
 * @param  product_id   产品 ID (例如 "Qp2jg6z96u")
 * @param  device_name  设备名称 (例如 "D001")
 * @param  s_version    MCU 固件版本 (例如 "V1.0")
 * @param  f_version    模块固件版本 (例如 "V1.0")
 * @param  auth_token   鉴权 token (Authorization header)
 * @retval HTTP_OTA_OK 成功，其他值表示错误
 */
http_ota_err_t http_ota_report_version(const char *product_id,
                                        const char *device_name,
                                        const char *s_version,
                                        const char *f_version,
                                        const char *auth_token);

/**
 * @brief  步骤二：查询是否有 OTA 升级任务
 * @param  product_id   产品 ID
 * @param  device_name  设备名称
 * @param  current_ver  当前版本号 (用于查询参数)
 * @param  auth_token   鉴权 token
 * @param  task_info    [out] 输出任务信息（如果有任务）
 * @retval HTTP_OTA_OK 有任务，HTTP_OTA_ERR_NO_TASK 无任务，其他值表示错误
 */
http_ota_err_t http_ota_check_update(const char *product_id,
                                      const char *device_name,
                                      const char *current_ver,
                                      const char *auth_token,
                                      http_ota_task_t *task_info);

/**
 * @brief  步骤三：分片下载固件数据
 * @param  product_id   产品 ID
 * @param  device_name  设备名称
 * @param  task_id      任务 ID (从步骤二获取)
 * @param  auth_token   鉴权 token
 * @param  total_size   固件总大小（字节）
 * @retval HTTP_OTA_OK 成功，其他值表示错误
 *
 * @note   此函数会循环下载所有分片，并写入外部 Flash
 *         下载完成后会设置 OTA 标志并重启设备
 */
http_ota_err_t http_ota_download_firmware(const char *product_id,
                                           const char *device_name,
                                           const char *task_id,
                                           const char *auth_token,
                                           uint32_t total_size);

/**
 * @brief  完整的 HTTP OTA 流程（三步合一）
 * @param  product_id   产品 ID
 * @param  device_name  设备名称
 * @param  current_ver  当前版本号
 * @param  auth_token   鉴权 token
 * @retval HTTP_OTA_OK 成功，其他值表示错误
 */
http_ota_err_t http_ota_full_process(const char *product_id,
                                      const char *device_name,
                                      const char *current_ver,
                                      const char *auth_token);

#endif /* __ONENET_HTTP_OTA_H__ */
