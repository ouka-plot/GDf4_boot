#include "delay.h"
#include "core_cm4.h"
#include "systick.h"

/*============================================================================
 *  使用 DWT (Data Watchpoint & Trace) 周期计数器实现微秒延时
 *  完全不占用 SysTick，与 SysTick 中断互不干扰
 *===========================================================================*/

/* DWT 寄存器地址（Cortex-M4 标准） */
#define DWT_CYCCNT   (*(volatile uint32_t*)0xE0001004)
#define DWT_CONTROL  (*(volatile uint32_t*)0xE0001000)
#define DWT_LAR      (*(volatile uint32_t*)0xE0001FB0)
#define SCB_DEMCR    (*(volatile uint32_t*)0xE000EDFC)

static uint32_t us_ticks;  /* 每微秒对应的 CPU 周期数 */

void delay_init(void){
    /* 1. 使能 DWT 模块 */
    SCB_DEMCR |= (1UL << 24);   /* TRCENA bit */

    /* 2. 解锁 DWT（某些芯片需要） */
    DWT_LAR = 0xC5ACCE55;

    /* 3. 清零并启动周期计数器 */
    DWT_CYCCNT = 0;
    DWT_CONTROL |= 1;           /* CYCCNTENA bit */

    /* 4. 缓存每 us 的 tick 数（240MHz → 240） */
    us_ticks = SystemCoreClock / 1000000UL;
}

/* 微秒级延时 —— 基于 DWT 周期计数器，不碰 SysTick */
void delay_us(uint32_t num){
    uint32_t start = DWT_CYCCNT;
    uint32_t target = num * us_ticks;
    while((DWT_CYCCNT - start) < target);
}

/* 毫秒级延时 —— 直接复用 SysTick 中断版本 */
void delay_ms(uint32_t num){
    delay_1ms(num);

}
