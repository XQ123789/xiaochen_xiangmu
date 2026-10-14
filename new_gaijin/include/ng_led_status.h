/**
 * @file    ng_led_status.h
 * @brief   LED状态指示模块 - 通过不同闪烁模式反映系统运行状态
 * @note    依赖 ng_timer 模块实现非阻塞定时，依赖 HAL 层驱动LED硬件
 *          需要在主循环中周期调用 ng_led_update() 以更新LED状态
 */

#ifndef __NG_LED_STATUS_H
#define __NG_LED_STATUS_H

#include <stdint.h>

/* LED状态模式 */
typedef enum {
    NG_LED_OFF = 0,       /* 关闭 */
    NG_LED_INIT,          /* 初始化：快闪(200ms) */
    NG_LED_READY,         /* 就绪：慢闪(1000ms) */
    NG_LED_GPS_OK,        /* GPS定位成功：常亮 */
    NG_LED_GPS_FAIL,      /* GPS定位失败：双闪(200ms闪两次，停800ms) */
    NG_LED_UPLOADING,     /* 上传中：快闪(100ms) */
    NG_LED_ERROR,         /* 错误：三闪(100ms闪三次，停500ms) */
    NG_LED_RECOVERY       /* 恢复中：呼吸效果(如果硬件支持)或交替快慢闪 */
} ng_led_mode_t;

/**
 * @brief  初始化LED状态指示模块
 */
void ng_led_status_init(void);

/**
 * @brief  设置LED显示模式
 * @param  mode 目标模式
 */
void ng_led_set_mode(ng_led_mode_t mode);

/**
 * @brief  获取当前LED模式
 * @return 当前模式
 */
ng_led_mode_t ng_led_get_mode(void);

/**
 * @brief  LED状态更新（非阻塞，需在主循环中周期调用）
 * @note   依赖ng_timer_millis()进行非阻塞定时
 */
void ng_led_update(void);

#endif /* __NG_LED_STATUS_H */
