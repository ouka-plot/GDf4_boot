#include "boot.h"
#include "string.h"
#include <stdbool.h>  // 确保编译器认识 _Bool
#include "usart.h"
#include "gd32f4xx_fmc.h"

_Bool bootloader_cli(uint32_t timeout_ms);
void bootloader_cli_help(void);
static void dump_app_header(void);
static void flash_cache_reset(void);

/*============================================================================
 *                     CLI命令处理函数声明
 *===========================================================================*/
static void cmd_erase_app_flash(void);
static void cmd_uart_iap_download(void);
static void cmd_set_ota_version(uint8_t *data, uint16_t len);
static void cmd_get_ota_version(void);
static void cmd_backup_to_external(void);
static void cmd_restore_from_external(void);

/*============================================================================
 *                     Ymodem协议相关定义
 *===========================================================================*/
#define YMODEM_SOH      0x01    /* 128字节数据包头 */
#define YMODEM_STX      0x02    /* 1024字节数据包头 */
#define YMODEM_EOT      0x04    /* 传输结束 */
#define YMODEM_ACK      0x06    /* 确认 */
#define YMODEM_NAK      0x15    /* 否认 */
#define YMODEM_CAN      0x18    /* 取消传输 */
#define YMODEM_C        0x43    /* 'C' 请求CRC模式 */

#define YMODEM_PACKET_128   128
#define YMODEM_PACKET_1K    1024
#define YMODEM_MAX_RETRY    30           /* 增加重试次数，给用户更多时间操作 */
#define YMODEM_TIMEOUT_MS   3000         /* 数据包超时 */
#define YMODEM_STARTUP_TIMEOUT_MS   5000 /* 启动等待延长到5秒 */

/*============================================================================
 *                     CRC16-CCITT 查表法实现
 *  多项式: 0x1021, 初始值: 0x0000
 *  这是Ymodem/Xmodem标准使用的CRC算法
 *===========================================================================*/
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

/**
 * @brief  CRC16-CCITT计算
 * @param  data  数据指针
 * @param  len   数据长度
 * @return uint16_t CRC值
 */
static uint16_t crc16_ccitt(const uint8_t *data, uint32_t len){
    uint16_t crc = 0x0000;
    
    while(len--){
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *data++];
    }
    
    return crc;
}

/*============================================================================
 *                     串口发送函数 (Ymodem专用)
 *===========================================================================*/
/**
 * @brief  发送单字节
 * @param  c  字节数据
 */
static void uart_putc(uint8_t c){
    while(usart_flag_get(USART0, USART_FLAG_TBE) != SET);
    usart_data_transmit(USART0, c);
}

/*============================================================================
 *                     基于DMA+空闲中断的Ymodem协议实现
 *===========================================================================*/
/* Ymodem接收状态 */
typedef enum {
    YMODEM_STATE_IDLE = 0,
    YMODEM_STATE_WAIT_HEADER,
    YMODEM_STATE_RECEIVE_DATA,
    YMODEM_STATE_WAIT_EOT,
    YMODEM_STATE_COMPLETE,
    YMODEM_STATE_ERROR
} ymodem_state_t;

/* Ymodem接收控制块 */
typedef struct {
    ymodem_state_t state;
    char           filename[64];
    uint32_t       filesize;
    uint32_t       received;
    uint8_t        packet_num;
    uint32_t       flash_addr;
    uint8_t        errors;
    /* DMA累积缓冲区 */
    uint8_t        acc_buf[YMODEM_PACKET_1K + 5];  /* 累积缓冲区 */
    uint16_t       acc_len;                         /* 累积长度 */
    /* 数据输出缓冲区(避免memmove后数据被覆盖) */
    uint8_t        data_buf[YMODEM_PACKET_1K];     /* 数据输出缓冲区 */
} ymodem_cb_t;

static ymodem_cb_t g_ymodem;

/**
 * @brief  等待DMA数据到达(带超时)
 * @param  timeout_ms  超时时间(毫秒)
 * @return int8_t      0:有数据, -1:超时
 */
static int8_t ymodem_wait_data(uint32_t timeout_ms){
    uint32_t elapsed = 0;
    
    while(u0_ucb.out == u0_ucb.in){
        delay_1ms(1);
        elapsed++;
        if(elapsed >= timeout_ms){
            return -1;  /* 超时 */
        }
    }
    return 0;
}

/**
 * @brief  从DMA缓冲区获取一帧数据
 * @param  data  数据输出指针
 * @param  len   数据长度输出指针
 * @return int8_t 0:成功, -1:无数据
 */
static int8_t ymodem_get_frame(uint8_t **data, uint16_t *len){
    if(u0_ucb.out == u0_ucb.in){
        return -1;  /* 无数据 */
    }
    
    *data = u0_ucb.out->st;
    *len = u0_ucb.out->ed - u0_ucb.out->st + 1;
    
    /* 移动out指针 */
    u0_ucb.out++;
    if(u0_ucb.out > u0_ucb.end){
        u0_ucb.out = &u0_ucb.uart_infro_buf[0];
    }
    
    return 0;
}

/**
 * @brief  解析Ymodem第0包(文件信息包)
 * @param  data  数据指针(从第3字节开始，跳过SOH+SEQ+~SEQ)
 * @param  len   数据长度
 * @return int8_t 0:成功, -1:失败
 */
static int8_t ymodem_parse_header(uint8_t *data, uint16_t len){
    /* 第0包格式: 文件名 + '\0' + 文件大小(ASCII) + '\0' + ... */
    
    /* 提取文件名 */
    uint16_t i = 0;
    while(i < len && i < sizeof(g_ymodem.filename) - 1 && data[i] != '\0'){
        g_ymodem.filename[i] = data[i];
        i++;
    }
    g_ymodem.filename[i] = '\0';
    
    if(i == 0){
        /* 空文件名表示传输结束 */
        return -1;
    }
    
    /* 跳过文件名结束符 */
    i++;
    
    /* 提取文件大小(ASCII字符串转数字) */
    g_ymodem.filesize = 0;
    while(i < len && data[i] >= '0' && data[i] <= '9'){
        g_ymodem.filesize = g_ymodem.filesize * 10 + (data[i] - '0');
        i++;
    }
    
    u0_printf("File: %s, Size: %lu bytes\r\n", g_ymodem.filename, g_ymodem.filesize);
    
    return 0;
}

/**
 * @brief  处理累积缓冲区中的Ymodem数据包
 * @param  packet_data  数据输出指针(指向数据部分)
 * @param  packet_len   数据长度输出指针
 * @return int8_t       0:成功, 1:EOT, -1:数据不足, -2:CRC错误, -3:序号错误, -4:取消
 */
static int8_t ymodem_parse_packet(uint8_t **packet_data, uint16_t *packet_len){
    if(g_ymodem.acc_len == 0){
        return -1;
    }
    
    uint8_t header = g_ymodem.acc_buf[0];
    uint16_t data_len;
    uint16_t total_len;
    
    /* 处理特殊字符 */
    if(header == YMODEM_EOT){
        /* 移除EOT */
        g_ymodem.acc_len--;
        memmove(g_ymodem.acc_buf, &g_ymodem.acc_buf[1], g_ymodem.acc_len);
        return 1;   /* 传输结束 */
    }
    
    if(header == YMODEM_CAN){
        if(g_ymodem.acc_len >= 2 && g_ymodem.acc_buf[1] == YMODEM_CAN){
            return -4;  /* 发送方取消 */
        }
        /* 只有一个CAN，可能是数据不完整 */
        if(g_ymodem.acc_len < 2){
            return -1;
        }
    }
    
    /* 确定包长度 */
    if(header == YMODEM_SOH){
        data_len = YMODEM_PACKET_128;
    }else if(header == YMODEM_STX){
        data_len = YMODEM_PACKET_1K;
    }else{
        /* 无效包头，丢弃一个字节 */
        g_ymodem.acc_len--;
        memmove(g_ymodem.acc_buf, &g_ymodem.acc_buf[1], g_ymodem.acc_len);
        return -1;
    }
    
    /* 包结构: SOH/STX(1) + SEQ(1) + ~SEQ(1) + DATA(128/1024) + CRC_H(1) + CRC_L(1) */
    total_len = 1 + 1 + 1 + data_len + 2;
    
    if(g_ymodem.acc_len < total_len){
        return -1;  /* 数据不足，继续等待 */
    }
    
    /* 校验序号 */
    uint8_t seq = g_ymodem.acc_buf[1];
    uint8_t seq_inv = g_ymodem.acc_buf[2];
    if((seq ^ seq_inv) != 0xFF){
        /* 序号校验失败，丢弃包头重新同步 */
        g_ymodem.acc_len--;
        memmove(g_ymodem.acc_buf, &g_ymodem.acc_buf[1], g_ymodem.acc_len);
        return -3;
    }
    
    /* CRC校验 */
    uint8_t *data_start = &g_ymodem.acc_buf[3];
    uint16_t crc_recv = ((uint16_t)g_ymodem.acc_buf[3 + data_len] << 8) | 
                         g_ymodem.acc_buf[3 + data_len + 1];
    uint16_t crc_calc = crc16_ccitt(data_start, data_len);
    
    if(crc_recv != crc_calc){
        /* CRC错误，丢弃这个包 */
        g_ymodem.acc_len -= total_len;
        memmove(g_ymodem.acc_buf, &g_ymodem.acc_buf[total_len], g_ymodem.acc_len);
        return -2;
    }
    
    /* 包解析成功 - 先复制数据到独立缓冲区再移除 */
    g_ymodem.packet_num = seq;
    memcpy(g_ymodem.data_buf, data_start, data_len);  /* 先复制数据 */
    *packet_data = g_ymodem.data_buf;                 /* 返回独立缓冲区 */
    *packet_len = data_len;
    
    /* 从累积缓冲区移除已处理的包 */
    g_ymodem.acc_len -= total_len;
    memmove(g_ymodem.acc_buf, &g_ymodem.acc_buf[total_len], g_ymodem.acc_len);
    
    return 0;
}

/**
 * @brief  累积DMA数据到缓冲区
 */
static void ymodem_accumulate_data(void){
    uint8_t *frame_data;
    uint16_t frame_len;
    
    while(ymodem_get_frame(&frame_data, &frame_len) == 0){
        /* 防止累积缓冲区溢出 */
        if(g_ymodem.acc_len + frame_len > sizeof(g_ymodem.acc_buf)){
            /* 缓冲区溢出，丢弃旧数据 */
            g_ymodem.acc_len = 0;
        }
        
        memcpy(&g_ymodem.acc_buf[g_ymodem.acc_len], frame_data, frame_len);
        g_ymodem.acc_len += frame_len;
    }
}

/**
 * @brief  等待并接收Ymodem数据包
 * @param  packet_data  数据输出指针
 * @param  packet_len   数据长度输出指针
 * @param  timeout_ms   超时时间
 * @return int8_t       0:成功, 1:EOT, -1:超时, -2:CRC错误, -3:序号错误, -4:取消
 */
static int8_t ymodem_receive_packet(uint8_t **packet_data, uint16_t *packet_len, uint32_t timeout_ms){
    uint32_t elapsed = 0;
    int8_t ret;
    
    while(1){
        /* 累积新数据 */
        ymodem_accumulate_data();
        
        /* 尝试解析包 */
        ret = ymodem_parse_packet(packet_data, packet_len);
        
        if(ret != -1){
            return ret;  /* 解析成功或出错 */
        }
        
        /* 数据不足，等待1ms并检查超时 */
        delay_1ms(1);
        elapsed++;
        if(elapsed >= timeout_ms){
            return -1;  /* 超时 */
        }
    }
}

/**
 * @brief  Ymodem接收主函数 (基于DMA+空闲中断)
 * @return int8_t  0:成功, 非0:失败
 */
static int8_t ymodem_receive(void){
    uint8_t *data;
    uint16_t len;
    int8_t ret;
    uint8_t retry;
    uint8_t expected_seq = 0;
    
    /* 初始化控制块 */
    memset(&g_ymodem, 0, sizeof(g_ymodem));
    g_ymodem.flash_addr = APP_ADDR;
    g_ymodem.state = YMODEM_STATE_WAIT_HEADER;
    
    u0_printf("Waiting for Ymodem transfer...\r\n");
    u0_printf("Please start sending file with Ymodem protocol.\r\n");
    
    /* 发送'C'请求CRC模式，等待第0包 */
    for(retry = 0; retry < YMODEM_MAX_RETRY; retry++){
        uart_putc(YMODEM_C);
        
        ret = ymodem_receive_packet(&data, &len, YMODEM_STARTUP_TIMEOUT_MS);
        if(ret == 0){
            break;  /* 收到包 */
        }
        u0_printf("Retry %d...\r\n", retry + 1);
    }
    
    if(retry >= YMODEM_MAX_RETRY){
        u0_printf("No response from sender.\r\n");
        /* 清空DMA缓冲区，避免残留数据被主循环误处理 */
        u0_ucb.out = u0_ucb.in;
        g_ymodem.acc_len = 0;
        return -1;
    }
    
    /* 解析文件头 */
    if(ymodem_parse_header(data, len) != 0){
        u0_printf("Invalid header packet.\r\n");
        uart_putc(YMODEM_CAN);
        uart_putc(YMODEM_CAN);
        return -2;
    }
    
    /* 检查文件大小 */
    uint32_t app_max_size = 0x10000 + 0x20000 * 7;  /* 960KB */
    if(g_ymodem.filesize > app_max_size){
        u0_printf("File too large! Max: %lu bytes\r\n", app_max_size);
        uart_putc(YMODEM_CAN);
        uart_putc(YMODEM_CAN);
        return -3;
    }
    
    /* 擦除APP Flash */
    u0_printf("Erasing flash...\r\n");
    cmd_erase_app_flash();
    
    /* ACK第0包并请求数据 */
    uart_putc(YMODEM_ACK);
    uart_putc(YMODEM_C);
    
    expected_seq = 1;
    g_ymodem.state = YMODEM_STATE_RECEIVE_DATA;
    
    uint32_t loop_count = 0;
    
    /* 接收数据包循环 */
    while(g_ymodem.state == YMODEM_STATE_RECEIVE_DATA){
        ret = ymodem_receive_packet(&data, &len, YMODEM_TIMEOUT_MS);
        loop_count++;
        
        if(ret == 1){
            /* EOT - 传输结束 */
            u0_printf("\r\n[DBG] EOT received, loop=%lu\r\n", loop_count);
            uart_putc(YMODEM_NAK);  /* 第一次NAK */
            
            /* 等待第二个EOT */
            ret = ymodem_receive_packet(&data, &len, YMODEM_TIMEOUT_MS);
            if(ret == 1){
                uart_putc(YMODEM_ACK);
                uart_putc(YMODEM_C);  /* 请求结束包 */
                
                /* 接收空文件名包(结束标志) */
                ret = ymodem_receive_packet(&data, &len, YMODEM_TIMEOUT_MS);
                if(ret == 0){
                    uart_putc(YMODEM_ACK);
                }
            }
            
            g_ymodem.state = YMODEM_STATE_COMPLETE;
            break;
        }
        
        if(ret < 0){
            /* 超时或错误 - 打印第一次和每100次 */
            if(loop_count == 1 || loop_count % 100 == 0){
                u0_printf("\r\n[DBG] loop=%lu ret=%d err=%d\r\n", loop_count, ret, g_ymodem.errors);
            }
            g_ymodem.errors++;
            if(g_ymodem.errors > YMODEM_MAX_RETRY){
                u0_printf("\r\nToo many errors, aborting.\r\n");
                uart_putc(YMODEM_CAN);
                uart_putc(YMODEM_CAN);
                g_ymodem.state = YMODEM_STATE_ERROR;
                break;
            }
            uart_putc(YMODEM_NAK);
            continue;
        }
        
        /* 检查序号 */
        if(g_ymodem.packet_num != expected_seq){
            /* 可能是重传的包，检查是否是上一个序号 */
            if(g_ymodem.packet_num == (uint8_t)(expected_seq - 1)){
                uart_putc(YMODEM_ACK);  /* 已处理，直接ACK */
                continue;
            }
            u0_printf("\r\nSequence error: expected %d, got %d\r\n", 
                      expected_seq, g_ymodem.packet_num);
            g_ymodem.errors++;
            uart_putc(YMODEM_NAK);
            continue;
        }
        
        /* 计算实际写入长度 */
        uint32_t write_len = len;
        if(g_ymodem.received + write_len > g_ymodem.filesize){
            write_len = g_ymodem.filesize - g_ymodem.received;
        }
        
        /* 写入Flash */
        if(write_len > 0){
            uint32_t words = (write_len + 3) / 4;
            gd32_writeflash(g_ymodem.flash_addr, (uint32_t*)data, words);
            g_ymodem.flash_addr += write_len;
            g_ymodem.received += write_len;
        }
        
        /* 显示进度 */
        u0_printf("\rProgress: %lu / %lu bytes (%lu%%)", 
                  g_ymodem.received, g_ymodem.filesize,
                  g_ymodem.received * 100 / g_ymodem.filesize);
        
        /* ACK并准备下一包 */
        uart_putc(YMODEM_ACK);
        expected_seq++;
        g_ymodem.errors = 0;
    }
    
    if(g_ymodem.state == YMODEM_STATE_COMPLETE){
        u0_printf("\r\nTransfer complete! Received %lu / %lu bytes.\r\n", 
                  g_ymodem.received, g_ymodem.filesize);
        u0_printf("[DBG] flash_addr end = 0x%08lX (expected 0x%08lX)\r\n",
                  g_ymodem.flash_addr, APP_ADDR + g_ymodem.received);
        
        /* 立即验证Flash写入 */
        u0_printf("[DBG] Flash@0x%08lX = 0x%08lX\r\n", 
                  (uint32_t)APP_ADDR, *(volatile uint32_t*)APP_ADDR);
        return 0;
    }
    
    /* 失败时清空缓冲区 */
    u0_ucb.out = u0_ucb.in;
    g_ymodem.acc_len = 0;
    return -4;
}

OTA_InfoCB  ota_info   = {0};
OTA_Header  ota_header = {0};

load_a load_app;

// 片上 Flash 扇区基址与大小（Bank0），用于地址 → 扇区号反查
static const uint32_t flash_sector_base[12] = {
    0x08000000, //0 16KB
    0x08004000, //1 16KB
    0x08008000, //2 16KB
    0x0800C000, //3 16KB
    0x08010000, //4 64KB
    0x08020000, //5 128KB
    0x08040000, //6 128KB
    0x08060000, //7 128KB
    0x08080000, //8 128KB
    0x080A0000, //9 128KB
    0x080C0000, //10 128KB
    0x080E0000  //11 128KB
};

static const uint32_t flash_sector_size[12] = {
    0x4000, //0 16KB
    0x4000, //1 16KB
    0x4000, //2 16KB
    0x4000, //3 16KB
    0x10000, //4 64KB
    0x20000, //5 128KB
    0x20000, //6 128KB
    0x20000, //7 128KB
    0x20000, //8 128KB
    0x20000, //9 128KB
    0x20000, //10 128KB
    0x20000  //11 128KB
};

/*--- 辅助：根据内部 Flash 地址找到扇区号，找不到返回 0xFF ---*/
static uint8_t addr_to_sector(uint32_t addr){
    for(uint8_t i = 0; i < 12; i++){
        if(addr >= flash_sector_base[i] &&
           addr <  flash_sector_base[i] + flash_sector_size[i]){
            return i;
        }
    }
    return 0xFF;
}

void bootloader_branch(void){
    _Bool cli_result = bootloader_cli(2000);  // 只调用一次，缓存结果

    if(cli_result == 0){
        //进入命令行模式
        bootloader_cli_help();
    }else{
        if(ota_info.boot_flag == BOOT_FLAG_SET){
            u0_printf("Enter bootloader\r\n");

            uint8_t ret = boot_apply_update();
            if(ret == 0){
                u0_printf("OTA update OK\r\n");
                // 清除升级标志，回写 EEPROM
                ota_info.boot_flag = 0;
                AT24_write_page(0, (uint8_t*)&ota_info, OTA_INFO_SIZE);
                jump2app(APP_ADDR);
            }else{
                u0_printf("OTA update FAIL, code %d\r\n", ret);
                // 失败不清标志，下次重启仍可重试
            }
        }else{
            u0_printf("Enter app\r\n");
            dump_app_header();
            jump2app(APP_ADDR);
        }
    }
}


//实现进入串口命令行
_Bool bootloader_cli(uint32_t timeout_ms){
//等待2s看是否有串口输入'w'，决定要不要进入命令行
    u0_printf("Press 'w' within %d ms to enter CLI\r\n", timeout_ms);

    delay_1ms(timeout_ms);

    // 直接扫 buffer 找 'w'
    _Bool found = 0;
    for(uint32_t i = 0; i < rx_max; i++){
        if(uart_rxbuf[i] == 'w'){
            found = 1;
            uart_rxbuf[i] = 0;  // 清除已处理的'w'
            break;
        }
    }

    if(found){
        u0_printf("Enter CLI\r\n");
        // 清空接收缓冲区，防止'w'被当作命令处理
        u0_ucb.out = u0_ucb.in;
        return 0;
    }else{
        u0_printf("Enter bootloader or app\r\n");
        return 1;
    }
}


//命令行指令显示
void bootloader_cli_help(void){
    u0_printf("Bootloader CLI commands:\r\n");
    u0_printf(" 输入[1],擦除A区 \r\n");
    u0_printf(" 输入[2],串口IAP下载A区程序 \r\n");
    u0_printf(" 输入[3],设置OTA版本号 \r\n");
    u0_printf(" 输入[4],查询OTA版本号 \r\n");
    u0_printf(" 输入[5],从内部flash向外部flash搬运程序<存储> \r\n");
    u0_printf(" 输入[6],从外部flash向内部flash搬运程序<升级> \r\n");
    u0_printf(" 输入[7],软件重启 \r\n");
}
/*---------------------------------------------------------------------------
 * boot_apply_update
 *   1) 从外部 Flash 读取 OTA_Header
 *   2) 校验 magic、seg_count
 *   3) 遍历每个 OTA_Segment：查扇区号 → 擦除 → 分块搬运写入
 *   返回 0 成功，非 0 失败
 *---------------------------------------------------------------------------*/
uint8_t boot_apply_update(void){

    /* ---------- 1. 读 Header ---------- */
    gd25_read((uint8_t*)&ota_header, ota_info.header_addr, OTA_HEADER_SIZE);

    if(ota_header.magic != OTA_HEADER_MAGIC){
        u0_printf("OTA header magic error\r\n");
        return 1;
    }
    if(ota_header.seg_count == 0 || ota_header.seg_count > OTA_SEG_MAX){
        u0_printf("OTA seg_count invalid: %lu\r\n", ota_header.seg_count);
        return 2;
    }

    /* ---------- 2. 逐段搬运 ---------- */
    uint8_t buffer[1024];

    for(uint32_t i = 0; i < ota_header.seg_count; i++){
        OTA_Segment *seg = &ota_header.segs[i];

        if(seg->data_len == 0) continue;

        /* 2a. 根据 target_addr 反查扇区号 */
        uint8_t sec = addr_to_sector(seg->target_addr);
        if(sec == 0xFF || sec < 4){
            u0_printf("OTA seg %lu target 0x%08lX invalid\r\n", i, seg->target_addr);
            return 3;
        }

        /* 2b. 长度不能超过该扇区容量 */
        if(seg->data_len > flash_sector_size[sec]){
            u0_printf("OTA seg %lu len overflow\r\n", i);
            return 4;
        }

        /* 2c. 擦除目标扇区 */
        gd32_eraseflash(sec, 1);

        /* 2d. 分块搬运：外部 Flash → 内部 Flash */
        uint32_t src    = seg->ext_offset;     // 外部 Flash 偏移
        uint32_t dst    = seg->target_addr;    // 内部 Flash 地址
        uint32_t remain = seg->data_len;

        while(remain){
            uint32_t chunk       = (remain > sizeof(buffer)) ? sizeof(buffer) : remain;
            uint32_t chunk_words = (chunk + 3) / 4;

            memset(buffer, 0xFF, chunk_words * 4);     // 填 0xFF 处理尾部对齐
            gd25_read(buffer, src, chunk);              // 从外部 Flash 读
            gd32_writeflash(dst, (uint32_t*)buffer, chunk_words); // 写内部 Flash

            src    += chunk;
            dst    += chunk;
            remain -= chunk;
        }

        u0_printf("OTA seg %lu done (%lu B -> 0x%08lX)\r\n",
                   i, seg->data_len, seg->target_addr);
    }

    return 0;
}



void jump2app(uint32_t app_addr){
    uint32_t app_msp = *(volatile uint32_t*)app_addr;  // 读取栈顶指针
    // 校验栈顶指针是否指向 SRAM 范围
    if(app_msp < 0x20000000 || app_msp > 0x2002FFFF){
        u0_printf("App MSP 0x%08lX invalid, abort jump\r\n", app_msp);
        return;
    }

    uint32_t jump_addr = *(volatile uint32_t*)(app_addr + 4); //获取复位地址 reset handler address
    
    u0_printf("Jumping to APP: MSP=0x%08lX, RST=0x%08lX\r\n", app_msp, jump_addr);
    /* 等待串口发送完毕，确保日志可见 */
    while(usart_flag_get(USART0, USART_FLAG_TC) != SET);

    /* ---- 清理 Bootloader 遗留状态 ---- */

    /* 1. 关闭全局中断 */
    __disable_irq();

    /* 2. 关闭 SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 3. 禁止所有 NVIC 中断 & 清除所有 Pending */
    for(uint32_t i = 0; i < 8; i++){
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 4. 复位 Flash I/D cache，防止取到旧指令 */
    flash_cache_reset();

    /* 5. 关闭 Bootloader 用到的外设（USART0 DMA, SPI1, I2C 等） */
    usart_deinit(USART0);
    dma_deinit(DMA1, DMA_CH2);
    spi_i2s_deinit(SPI1);
    rcu_periph_clock_disable(RCU_DMA1);

    /* 6. 设置向量表偏移到 APP 地址 */
    SCB->VTOR = app_addr;
    
    /* 7. 设置主栈指针 */
    __set_MSP(app_msp);
    
    /* 8. 内存屏障，确保以上操作完成 */
    __DSB();
    __ISB();
    
    /* 9. 跳转到 APP 的 Reset Handler */
    ((void(*)(void))jump_addr)();
    
    /* 不应该执行到这里 */
    while(1);
}

void bootloader_clear(void){
    usart_deinit(USART0);
    gpio_deinit(GPIOA);
    gpio_deinit(GPIOB);
}

/*============================================================================
 * GD32F4xx FMC_WS 寄存器缓存控制位（库头文件未定义，手动补充）
 * 与 STM32F4 FLASH_ACR 布局一致
 *===========================================================================*/
#define FMC_WS_ICEN   BIT(9)    /* Instruction cache enable  */
#define FMC_WS_DCEN   BIT(10)   /* Data cache enable         */
#define FMC_WS_ICRST  BIT(11)   /* Instruction cache reset   */
#define FMC_WS_DCRST  BIT(12)   /* Data cache reset          */

/* 复位 Flash I/D cache，防止编程后取到旧缓存 */
static void flash_cache_reset(void){
    /* 1. 关闭 I-Cache 和 D-Cache */
    FMC_WS &= ~(FMC_WS_ICEN | FMC_WS_DCEN);

    /* 2. 置位复位位（自动清零） */
    FMC_WS |= FMC_WS_ICRST;
    FMC_WS |= FMC_WS_DCRST;

    /* 3. 重新使能 */
    FMC_WS |= (FMC_WS_ICEN | FMC_WS_DCEN);

    /* 4. 全屏障，确保流水线刷新 */
    __DSB();
    __ISB();
}

/* 诊断：打印 APP 向量表前 32 字节 */
static void dump_app_header(void){
    uint32_t msp_val   = *(volatile uint32_t*)APP_ADDR;
    uint32_t reset_val = *(volatile uint32_t*)(APP_ADDR + 4);
    u0_printf("APP_ADDR = 0x%08lX\r\n", (uint32_t)APP_ADDR);
    u0_printf("MSP  @0x%08lX = 0x%08lX\r\n", (uint32_t)APP_ADDR, msp_val);
    u0_printf("RST  @0x%08lX = 0x%08lX\r\n", (uint32_t)(APP_ADDR + 4), reset_val);

    u0_printf("First 32 bytes of APP:\r\n");
    uint8_t *p = (uint8_t*)APP_ADDR;
    for(int i = 0; i < 32; i++){
        u0_printf("%02X ", p[i]);
        if((i + 1) % 16 == 0) u0_printf("\r\n");
    }
}

/*============================================================================
 *                     功能1: 擦除APP区Flash
 *===========================================================================*/
static void cmd_erase_app_flash(void){
    u0_printf("Erasing APP flash (sector 4-11)...\r\n");
    
    /* APP区从sector 4开始，共8个扇区 */
    for(uint8_t sec = 4; sec < 12; sec++){
        u0_printf("  Erasing sector %d...", sec);
        gd32_eraseflash(sec, 1);
        u0_printf(" OK\r\n");
    }
    
    u0_printf("APP flash erased successfully!\r\n");
}

/*============================================================================
 *                     功能2: 串口IAP下载 (Ymodem协议)
 *  使用DMA+空闲中断接收，兼容SecureCRT/Tera Term/Xshell等工具
 *===========================================================================*/
static void cmd_uart_iap_download(void){
    u0_printf("\r\n=== UART IAP Download (Ymodem) v2.5 ===\r\n");
    u0_printf("Support: SecureCRT, Tera Term, Xshell, etc.\r\n");
    u0_printf("Using DMA + IDLE interrupt reception.\r\n\r\n");
    
    /* 使用Ymodem协议接收 (基于DMA+空闲中断) */
    int8_t ret = ymodem_receive();
    
    if(ret == 0){
        u0_printf("IAP download successful!\r\n");
        
        /* 验证写入的数据 */
        u0_printf("\r\n=== Flash Verification ===\r\n");
        u0_printf("APP_ADDR = 0x%08lX\r\n", (uint32_t)APP_ADDR);
        uint32_t msp_val = *(volatile uint32_t*)APP_ADDR;
        uint32_t reset_val = *(volatile uint32_t*)(APP_ADDR + 4);
        u0_printf("MSP  @0x%08lX = 0x%08lX\r\n", (uint32_t)APP_ADDR, msp_val);
        u0_printf("RST  @0x%08lX = 0x%08lX\r\n", (uint32_t)(APP_ADDR + 4), reset_val);
        
        /* 打印前32字节 (向量表) */
        u0_printf("First 32 bytes of APP:\r\n");
        uint8_t *p = (uint8_t*)APP_ADDR;
        for(int i = 0; i < 32; i++){
            u0_printf("%02X ", p[i]);
            if((i + 1) % 16 == 0) u0_printf("\r\n");
        }
        
        u0_printf("Press '7' to reset and run new firmware.\r\n");
    }else{
        u0_printf("IAP download failed, error: %d\r\n", ret);
    }
}

/*============================================================================
 *                     功能3: 设置OTA版本号
 *  数据格式: "3 V1.2.3" 或 "3V1.2.3" 
 *  版本号存储在EEPROM的OTA_INFO之后的位置
 *===========================================================================*/
#define OTA_VERSION_ADDR    (OTA_INFO_SIZE)     /* 版本号存储地址 */
#define OTA_VERSION_MAX_LEN 16                   /* 版本号最大长度 */

static void cmd_set_ota_version(uint8_t *data, uint16_t len){
    /* 跳过命令字符和可能的空格 */
    uint8_t *version_str = data + 1;
    uint16_t version_len = len - 1;
    
    /* 跳过前导空格 */
    while(version_len > 0 && *version_str == ' '){
        version_str++;
        version_len--;
    }
    
    if(version_len == 0 || version_len > OTA_VERSION_MAX_LEN){
        u0_printf("Error: Invalid version format. Usage: 3 V1.2.3\r\n");
        return;
    }
    
    /* 准备写入数据 (添加长度前缀和结束符) */
    uint8_t ver_buf[OTA_VERSION_MAX_LEN + 2];
    memset(ver_buf, 0, sizeof(ver_buf));
    ver_buf[0] = (uint8_t)version_len;
    memcpy(&ver_buf[1], version_str, version_len);
    
    /* 写入EEPROM */
    uint8_t ret = AT24_write_page(OTA_VERSION_ADDR, ver_buf, version_len + 2);
    
    if(ret == 0){
        u0_printf("OTA version set to: %.*s\r\n", version_len, version_str);
    }else{
        u0_printf("Error: Failed to write version, code %d\r\n", ret);
    }
}

/*============================================================================
 *                     功能4: 查询OTA版本号
 *===========================================================================*/
static void cmd_get_ota_version(void){
    uint8_t ver_buf[OTA_VERSION_MAX_LEN + 2];
    memset(ver_buf, 0, sizeof(ver_buf));
    
    /* 从EEPROM读取 */
    uint8_t ret = AT24_read_page(OTA_VERSION_ADDR, ver_buf, OTA_VERSION_MAX_LEN + 2);
    
    if(ret != 0){
        u0_printf("Error: Failed to read version, code %d\r\n", ret);
        return;
    }
    
    uint8_t ver_len = ver_buf[0];
    
    if(ver_len == 0 || ver_len > OTA_VERSION_MAX_LEN || ver_len == 0xFF){
        u0_printf("OTA version: Not set\r\n");
    }else{
        u0_printf("OTA version: %.*s\r\n", ver_len, &ver_buf[1]);
    }
    
    /* 同时显示OTA标志状态 */
    u0_printf("Boot flag: 0x%08lX (%s)\r\n", 
              ota_info.boot_flag,
              (ota_info.boot_flag == BOOT_FLAG_SET) ? "Update pending" : "Normal");
}

/*============================================================================
 *                     功能5: 内部Flash → 外部Flash (备份)
 *  将APP区的程序备份到外部SPI Flash
 *===========================================================================*/
static void cmd_backup_to_external(void){
    u0_printf("=== Backup Internal Flash to External Flash ===\r\n");
    
    /* 计算APP区总大小: sector4(64KB) + sector5-11(128KB*7) = 960KB */
    const uint32_t app_total_size = 0x10000 + 0x20000 * 7;  /* 960KB */
    /* 外部 Flash 容量（GD25Q40E = 512KB）和页大小 */
    const uint32_t ext_flash_size      = 512 * 1024;
    const uint32_t ext_flash_page_size = 256;
    const uint32_t header_reserved     = (OTA_HEADER_SIZE + ext_flash_page_size - 1) / ext_flash_page_size * ext_flash_page_size;

    /* 计算实际占用长度：从 APP 末尾向前找到最后一个非 0xFFFFFFFF 字 */
    uint32_t app_start = APP_ADDR;
    uint32_t app_end   = APP_ADDR + app_total_size;
    uint32_t used_end  = app_start;  /* 默认空 */

    for(uint32_t addr = app_end; addr > app_start; addr -= 4){
        uint32_t val = gd32_readflash(addr - 4);
        if(val != 0xFFFFFFFF){
            used_end = addr;
            break;
        }
    }

    uint32_t used_size = (used_end > app_start) ? (used_end - app_start) : 0;
    if(used_size == 0){
        u0_printf("Error: APP appears empty, abort backup.\r\n");
        return;
    }

    uint32_t used_size_aligned = (used_size + ext_flash_page_size - 1) & ~(ext_flash_page_size - 1);

    /* 容量检查：只备份实际使用部分 */
    if(header_reserved + used_size_aligned > ext_flash_size){
        u0_printf("Error: Used APP (%lu B, aligned %lu B) exceeds external flash capacity (%lu B), abort backup.\r\n",
                  used_size, used_size_aligned, ext_flash_size);
        return;
    }
    
    /* 擦除外部Flash (GD25Q40E 512KB, 需要多块) */
    /* 这里假设OTA数据从Block 1开始存储，Block 0 存Header */
    u0_printf("Erasing external flash...\r\n");
    for(uint8_t blk = 0; blk < 8; blk++){  /* 8 * 64KB = 512KB */
        gd25_erase64k(blk);
        u0_printf("  Block %d erased\r\n", blk);
    }
    
    /* 准备OTA Header */
    OTA_Header backup_header;
    memset(&backup_header, 0, sizeof(backup_header));
    backup_header.magic = OTA_HEADER_MAGIC;
    backup_header.seg_count = 1;
    backup_header.total_len = used_size;

    /* 当前外部Flash写入偏移 (Header之后，按页对齐保护 Header) */
    uint32_t ext_offset = header_reserved;
    uint8_t buffer[256];

    /* 只备份实际使用的部分，作为单段 */
    backup_header.segs[0].target_addr = APP_ADDR;
    backup_header.segs[0].data_len    = used_size;
    backup_header.segs[0].ext_offset  = ext_offset;
    backup_header.segs[0].crc32       = 0;  /* TODO: 计算CRC */

    u0_printf("Backup used region: %lu bytes (aligned %lu bytes)\r\n",
              used_size, used_size_aligned);

    uint32_t src_addr  = APP_ADDR;
    uint32_t remain    = used_size_aligned;
    uint32_t page_num  = ext_offset / ext_flash_page_size;
    const uint32_t app_used_end = APP_ADDR + used_size;

    while(remain > 0){
        uint32_t chunk = (remain > ext_flash_page_size) ? ext_flash_page_size : remain;

        /* 预填 0xFF，末尾不足部分保持 0xFF */
        memset(buffer, 0xFF, ext_flash_page_size);

        /* 仅在真实数据范围内拷贝 */
        uint32_t copy_len = chunk;
        if(src_addr + copy_len > app_used_end){
            if(src_addr >= app_used_end){
                copy_len = 0;  /* 全是填充 */
            }else{
                copy_len = app_used_end - src_addr;
            }
        }

        for(uint32_t i = 0; i < copy_len; i += 4){
            uint32_t val = gd32_readflash(src_addr + i);
            memcpy(&buffer[i], &val, 4);
        }

        gd25_pagewrite(buffer, page_num);

        src_addr += chunk;
        remain   -= chunk;
        page_num++;
    }
    
    /* 写入Header到外部Flash地址0 */
    u0_printf("Writing OTA header...\r\n");
    /* Header可能超过256字节，需要分页写 */
    uint8_t *hdr_ptr = (uint8_t*)&backup_header;
    uint32_t hdr_remain = OTA_HEADER_SIZE;
    uint16_t hdr_page = 0;
    
    while(hdr_remain > 0){
        uint32_t chunk = (hdr_remain > 256) ? 256 : hdr_remain;
        memset(buffer, 0xFF, 256);
        memcpy(buffer, hdr_ptr, chunk);
        gd25_pagewrite(buffer, hdr_page);
        hdr_ptr += chunk;
        hdr_remain -= chunk;
        hdr_page++;
    }
    
    u0_printf("Backup complete! Total: %lu bytes\r\n", backup_header.total_len);
}

/*============================================================================
 *                     功能6: 外部Flash → 内部Flash (升级/恢复)
 *  调用已有的boot_apply_update函数
 *===========================================================================*/
static void cmd_restore_from_external(void){
    u0_printf("=== Restore from External Flash ===\r\n");
    
    /* 设置OTA信息 */
    ota_info.boot_flag = BOOT_FLAG_SET;
    ota_info.header_addr = 0;  /* Header在外部Flash地址0 */
    
    /* 调用升级函数 */
    uint8_t ret = boot_apply_update();
    
    if(ret == 0){
        u0_printf("Restore successful!\r\n");
        /* 清除升级标志 */
        ota_info.boot_flag = 0;
        AT24_WriteOTAInfo();
        dump_app_header();
        /* 刚写完内部 Flash，先清缓存 */
        flash_cache_reset();
    }else{
        u0_printf("Restore failed, error code: %d\r\n", ret);
    }
}

/*============================================================================
 *                     CLI命令分发函数
 *===========================================================================*/
void bootloader_cli_event(uint8_t *data, uint16_t len){
    if(len == 0) return;

    switch(data[0]){
        case '1':
            cmd_erase_app_flash();
            break;
            
        case '2':
            cmd_uart_iap_download();
            break;
            
        case '3':
            cmd_set_ota_version(data, len);
            break;
            
        case '4':
            cmd_get_ota_version();
            break;
            
        case '5':
            cmd_backup_to_external();
            break;
            
        case '6':
            cmd_restore_from_external();
            break;
            
        case '7':
            u0_printf("Software reset...\r\n");
            /* 等待串口发送完毕（不依赖 SysTick，因为 delay_us 可能已将其关闭） */
            while(usart_flag_get(USART0, USART_FLAG_TC) != SET);
            __disable_irq();
            __DSB();
            __ISB();
            NVIC_SystemReset();
            while(1);  /* 等待复位 */
            break;

        case '8':
            u0_printf("Dump APP header...\r\n");
            dump_app_header();
            break;
            
        case 'h':
        case 'H':
        case '?':
            bootloader_cli_help();
            break;
            
        default:
            u0_printf("Unknown command: %c (0x%02X)\r\n", data[0], data[0]);
            u0_printf("Press 'h' for help\r\n");
            break;
    }
}