/**
 * @file    ng_timer.h
 * @brief   计时服务模块 - 提供毫秒级计时和延时功能
 * @note    使用TIM2实现毫秒计数器，避免与原delay.c的SysTick冲突
 *          需要在TIM2中断中调用ng_timer_tick()
 */

#ifndef __NG_TIMER_H
#define __NG_TIMER_H

#include <stdint.h>

/**
 * @brief  初始化计时器（TIM2毫秒计数器）
 * @note   使用TIM2产生1ms中断，与原delay.c的SysTick互不冲突
 *         原delay.c使用SysTick轮询方式实现延时，此处使用TIM2独立计时
 */
void ng_timer_init(void);

/**
 * @brief  TIM2中断中调用的递增函数
 * @note   在stm32f10x_it.c的TIM2_IRQHandler中调用此函数
 */
void ng_timer_tick(void);

/**
 * @brief  获取当前毫秒计数值
 * @return 自初始化以来的毫秒数
 */
uint32_t ng_timer_millis(void);

/**
 * @brief  检查是否已过指定时间（非阻塞）
 * @param  start       起始时间（由ng_timer_millis获取）
 * @param  interval_ms 间隔时间（毫秒）
 * @return 1=已过期, 0=未过期
 */
int ng_timer_expired(uint32_t start, uint32_t interval_ms);

/**
 * @brief  延时指定毫秒（阻塞式，兼容原有delay_ms）
 * @param  ms 延时毫秒数
 * @note   内部调用原delay_ms实现，保持兼容性
 */
void ng_timer_delay_ms(uint32_t ms);

#endif /* __NG_TIMER_H */
