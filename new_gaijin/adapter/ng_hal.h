/**
 * @file    ng_hal.h
 * @brief   硬件抽象层（HAL）接口定义
 * @note    通过函数指针结构体封装底层硬件驱动，
 *          上层业务模块通过函数指针调用硬件，不直接依赖具体驱动函数。
 *          这样当硬件平台变更时，只需替换 ng_hal_stm32.c 的实现即可。
 *
 * 对应原项目驱动函数映射：
 *   - uart2_send_str    -> Uart2_SendStr()       (usart.c)
 *   - uart2_send_data   -> USART2_Send()         (usart.c)
 *   - uart2_send_byte   -> UART2_send_byte()     (usart.c)
 *   - uart2_get_buf     -> buf_uart2.buf          (usart.c)
 *   - uart2_clear_buf   -> Clear_Buffer()         (4G.c)
 *   - feed_watchdog     -> IWDG_Feed()            (wdg.c)
 *   - delay_ms          -> delay_ms()             (delay.c)
 *   - get_millis        -> ng_timer_millis()      (ng_timer.c)
 *   - led_on/off/toggle -> LED1=1/0/取反          (led.h)
 *   - mqtt_connect      -> CSTX_4G_RegALIYUNIOT() (4G.c/bc20_improved.c)
 *   - mqtt_disconnect   -> QMTDISC+QMTCLOSE序列     (4G.c)
 *   - mqtt_reconnect    -> mqtt_disconnect + mqtt_connect
 *   - mqtt_send         -> CSTX_4G_ALYIOTSenddata()(4G.c/bc20_improved.c)
 *   - mqtt_is_connected -> 静态状态变量跟踪
 *   - module_init       -> CSTX_4GCTR_Init() + CSTX_4G_Init() (led.c + 4G.c)
 *   - get_imei          -> Get_4GIMEI_NUM()                  (4G.c)
 *   - dht11_init        -> DHT11_Init()                      (dht11.c)
 *   - dht11_read        -> DHT11_Read_TempAndHumidity()      (dht11.c)
 *   - dht11_get_temp    -> DHT11_Data.temp_int               (dht11.c)
 *   - dht11_get_humi    -> DHT11_Data.humi_int               (dht11.c)
 */

#ifndef __NG_HAL_H
#define __NG_HAL_H

#include <stdint.h>

/**
 * @brief  硬件驱动函数指针结构体
 * @note   通过此结构体封装底层驱动，新模块通过函数指针调用硬件
 *         所有函数指针在 ng_hal_init() 中由 ng_hal_stm32.c 填充
 */
typedef struct {
    /* ====== UART操作（对应原项目 usart.c 中的函数） ====== */

    /** 发送字符串（对应 Uart2_SendStr） */
    void (*uart2_send_str)(const char *str);

    /** 发送指定长度数据（对应 USART2_Send） */
    void (*uart2_send_data)(const char *data, uint16_t len);

    /** 发送单字节（对应 UART2_send_byte） */
    void (*uart2_send_byte)(uint8_t data);

    /** 获取UART2接收缓冲区指针（对应 buf_uart2.buf） */
    char* (*uart2_get_buf)(void);

    /** 清空UART2接收缓冲区（对应 Clear_Buffer） */
    void (*uart2_clear_buf)(void);

    /* ====== 系统操作 ====== */

    /** 喂狗（对应 IWDG_Feed） */
    void (*feed_watchdog)(void);

    /** 阻塞式毫秒延时（对应 delay_ms） */
    void (*delay_ms)(uint32_t ms);

    /** 获取系统毫秒计数（对应 ng_timer_millis） */
    uint32_t (*get_millis)(void);

    /* ====== LED操作（对应 led.h 中 LED1 宏） ====== */

    /** LED点亮（对应 LED1=1 或 LED_Init后控制） */
    void (*led_on)(void);

    /** LED熄灭 */
    void (*led_off)(void);

    /** LED翻转 */
    void (*led_toggle)(void);

    /* ====== 4G模块生命周期 ====== */

    /** 4G模块初始化（对应 CSTX_4GCTR_Init() + CSTX_4G_Init()） */
    void (*module_init)(void);

    /** 获取IMEI号（对应 Get_4GIMEI_NUM()） */
    void (*get_imei)(void);

    /* ====== 4G模块MQTT操作（对应 4G.c / bc20_improved.c） ====== */

    /** MQTT连接OneNET（对应 CSTX_4G_RegALIYUNIOT） */
    uint8_t (*mqtt_connect)(void);

    /**
     * MQTT发送数据（对应 CSTX_4G_ALYIOTSenddata）
     * @param temp 温度, humi 湿度
     * @param lat/lon/alt 纬度/经度/海拔
     * @param speed 速度, dir 方向角
     * @return 0=成功, 1=失败
     */
    uint8_t (*mqtt_send)(uint8_t temp, uint8_t humi,
                         double lat, double lon, double alt,
                         double speed, double dir);

    /** 检查MQTT是否已连接（基于内部状态变量） */
    uint8_t (*mqtt_is_connected)(void);

    /** MQTT断开连接（发送QMTDISC+QMTCLOSE指令序列） */
    void (*mqtt_disconnect)(void);

    /** MQTT重连（先断开再连接） */
    uint8_t (*mqtt_reconnect)(void);

    /* ====== DHT11温湿度传感器（对应 dht11.c） ====== */

    /** DHT11初始化（对应 DHT11_Init()） */
    void (*dht11_init)(void);

    /** DHT11读取温湿度（对应 DHT11_Read_TempAndHumidity()） */
    void (*dht11_read)(void);

    /** 获取温度整数值（返回 DHT11_Data.temp_int） */
    uint8_t (*dht11_get_temp)(void);

    /** 获取湿度整数值（返回 DHT11_Data.humi_int） */
    uint8_t (*dht11_get_humi)(void);
} ng_hal_driver_t;

/**
 * @brief  初始化HAL适配器（使用STM32实际驱动函数填充函数指针）
 * @param  drv 驱动函数指针结构体，由调用者分配内存
 * @note   内部将各个STM32驱动函数地址填入drv结构体
 */
void ng_hal_init(ng_hal_driver_t *drv);

/**
 * @brief  获取HAL驱动实例（单例）
 * @return 指向已初始化的驱动结构体指针
 * @note   内部维护一个静态实例，首次调用时自动初始化
 */
ng_hal_driver_t* ng_hal_get_driver(void);

#endif /* __NG_HAL_H */
