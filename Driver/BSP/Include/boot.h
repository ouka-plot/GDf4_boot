#ifndef __BOOT_H__
#define __BOOT_H__      

#include "main.h"



#define BOOT_FLAG_SET       0x12345678
#define OTA_HEADER_MAGIC    0xAA55AA55   // 外部 Flash Header 魔数

#define OTA_SEG_MAX         8            // 最多支持 8 个段（扇区 4-11）

/*---------------------------------------------------------------------------
 * OTA_Segment: 描述一个搬运段（存放在外部 Flash Header 中）
 *   target_addr : 目标片上 Flash 地址（如 0x08010000）
 *   data_len    : 该段有效数据长度（字节）
 *   ext_offset  : 该段数据在外部 Flash 中的偏移（相对 Header 之后/绝对均可）
 *   crc32       : 该段数据的 CRC32 校验值
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t target_addr;   // 写入内部 Flash 的起始地址
    uint32_t data_len;      // 有效数据长度（字节）
    uint32_t ext_offset;    // 数据在外部 Flash 的偏移（字节）
    uint32_t crc32;         // 该段 CRC32
} OTA_Segment;              // 16 B

/*---------------------------------------------------------------------------
 * OTA_Header: 放在外部 Flash 头部（地址 0）
 *   Bootloader 只需读取这一块就知道要搬运多少段、搬到哪里
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t    magic;               // 魔数 OTA_HEADER_MAGIC，用于校验 Header 有效性
    uint32_t    seg_count;           // 本次升级包含的段数（1 ~ OTA_SEG_MAX）
    uint32_t    total_len;           // 所有段数据的总长度（字节），用于快速校验
    uint32_t    header_crc32;        // Header 自身的 CRC32（不含此字段）
    OTA_Segment segs[OTA_SEG_MAX];   // 各段描述
} OTA_Header;                        // 4*4 + 16*8 = 144 B

#define OTA_HEADER_SIZE sizeof(OTA_Header)

/*---------------------------------------------------------------------------
 * OTA_InfoCB: 存放在 EEPROM（AT24C256 地址 0）的精简信息
 *   只负责告诉 Bootloader "有没有升级" 和 "Header 在外部 Flash 的什么位置"
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t boot_flag;      // 0x12345678 表示需要升级
    uint32_t header_addr;    // OTA_Header 在外部 Flash 的起始地址（通常 0）
} OTA_InfoCB;                // 8 B

#define OTA_INFO_SIZE sizeof(OTA_InfoCB)

extern OTA_InfoCB  ota_info;
extern OTA_Header  ota_header;

typedef void (*load_a)(void);


void bootloader_branch(void);
void jump2app(uint32_t app_addr);
void bootloader_clear(void);

// 从外部 Flash 读取 OTA_Header 并按段搬运到片上 Flash
// 返回 0 成功，非 0 失败码
uint8_t boot_apply_update(void);
_Bool bootloader_cli(uint32_t timeout_ms);
void bootloader_cli_help(void);
void bootloader_cli_event(uint8_t *data, uint16_t len);


#endif



