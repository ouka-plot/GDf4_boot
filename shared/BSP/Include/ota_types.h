/**
 * @file   ota_types.h
 * @brief  OTA 数据结构定义 (Boot 和 APP 共用)
 *
 * 定义了外部 Flash 上的 OTA_Header / OTA_Segment、
 * 以及 EEPROM 中的 OTA_InfoCB，供 Boot 搬运和 APP 写入共同使用。
 */
#ifndef __OTA_TYPES_H__
#define __OTA_TYPES_H__

#include <stdint.h>

#define BOOT_FLAG_SET       0x12345678
#define OTA_HEADER_MAGIC    0xAA55AA55   /* 外部 Flash Header 魔数 */

#define OTA_SEG_MAX         8            /* 最多支持 8 个段（扇区 4-11）*/

/*---------------------------------------------------------------------------
 * OTA_Segment: 描述一个搬运段（存放在外部 Flash Header 中）
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t target_addr;   /* 写入内部 Flash 的起始地址 */
    uint32_t data_len;      /* 有效数据长度（字节）*/
    uint32_t ext_offset;    /* 数据在外部 Flash 的偏移（字节）*/
    uint32_t crc32;         /* 该段 CRC32 */
} OTA_Segment;              /* 16 B */

/*---------------------------------------------------------------------------
 * OTA_Header: 放在外部 Flash 头部（地址 0）
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t    magic;               /* 魔数 OTA_HEADER_MAGIC */
    uint32_t    seg_count;           /* 段数（1 ~ OTA_SEG_MAX）*/
    uint32_t    total_len;           /* 所有段数据的总长度（字节）*/
    uint32_t    header_crc32;        /* Header 自身的 CRC32 */
    OTA_Segment segs[OTA_SEG_MAX];   /* 各段描述 */
} OTA_Header;                        /* 4*4 + 16*8 = 144 B */

#define OTA_HEADER_SIZE sizeof(OTA_Header)

/*---------------------------------------------------------------------------
 * OTA_InfoCB: 存放在 EEPROM（AT24C256 地址 0）
 *---------------------------------------------------------------------------*/
typedef struct {
    uint32_t boot_flag;      /* 0x12345678 表示需要升级 */
    uint32_t header_addr;    /* OTA_Header 在外部 Flash 的起始地址 */
} OTA_InfoCB;                /* 8 B */

#define OTA_INFO_SIZE sizeof(OTA_InfoCB)

extern OTA_InfoCB  ota_info;
extern OTA_Header  ota_header;

#endif /* __OTA_TYPES_H__ */
