/**
 * @file    ng_hal_stm32.c
 * @brief   硬件抽象层 - STM32F103具体实现
 * @note    本文件将原项目中的底层驱动函数包装为统一的函数指针接口，
 *          填充到 ng_hal_driver_t 结构体中，供上层业务模块调用。
 *
 * 原项目驱动函数来源：
 *   - usart.c:  Uart2_SendStr, USART2_Send, UART2_send_byte, buf_uart2
 *   - 4G.c:     Clear_Buffer, CSTX_4G_Init, CSTX_4G_RegALIYUNIOT,
 *               CSTX_4G_ALYIOTSenddata, Get_4GIMEI_NUM
 *   - wdg.c:    IWDG_Feed
 *   - delay.c:  delay_ms
 *   - led.h:    LED1 (PBout(5)), CSTX_4GCTR_Init
 *   - dht11.h:  DHT11_Init, DHT11_Read_TempAndHumidity, DHT11_Data
 *   - ng_timer: ng_timer_millis
 *
 * 注意事项：
 *   - UART2_send_byte 参数类型为 char，本文件包装为 uint8_t
 *   - delay_ms 原参数为 u16，本文件包装为 uint32_t 并分段调用
 *   - LED操作使用 bit-band 宏 PBout(5)，即 LED1
 *   - CSTX_4G_ALYIOTSenddata 原函数返回void，此处包装为uint8_t（检查缓冲区确认）
 *   - MQTT连接状态通过静态变量跟踪，避免缓冲区被清空后丢失状态
 */

#include "ng_hal.h"
#include "ng_config.h"     /* NG_USE_DHT11 编译开关 */

/* 标准C库 */
#include <string.h>        /* strstr, strncpy, strlen */

/* STM32标准外设库 */
#include "stm32f10x.h"

/* 原项目驱动头文件 */
#include "usart.h"    /* Uart2_SendStr, USART2_Send, UART2_send_byte, buf_uart2 */
#include "wdg.h"      /* IWDG_Feed */
#include "delay.h"    /* delay_ms */
#include "led.h"      /* LED1 宏定义, CSTX_4GCTR_Init */
#include "4G.h"       /* Clear_Buffer, CSTX_4G_Init, CSTX_4G_RegALIYUNIOT,
                        * CSTX_4G_ALYIOTSenddata, Get_4GIMEI_NUM */

/* 新版计时模块 */
#include "ng_timer.h"

/* DHT11温湿度传感器（条件编译） */
#if NG_USE_DHT11
#include "dht11.h"    /* DHT11_Init, DHT11_Read_TempAndHumidity, DHT11_Data */
#endif

/* ====== 静态变量 ====== */

/* 驱动单例 */
static ng_hal_driver_t hal_driver;
static uint8_t hal_initialized = 0;

/* MQTT连接状态跟踪（避免依赖会被清空的buf_uart2.buf） */
static uint8_t mqtt_connected_state = 0;

/* IMEI号静态缓存 */
static char imei_buffer[20] = {0};

/* ====== UART包装函数 ====== */

/**
 * @brief  UART2发送字符串 - 包装 Uart2_SendStr()
 * @param  str 以'\0'结尾的字符串
 * @note   原函数: void Uart2_SendStr(char*SendBuf) 位于 usart.c
 */
static void hal_uart2_send_str(const char *str)
{
    Uart2_SendStr((char *)str);
}

/**
 * @brief  UART2发送指定长度数据 - 包装 USART2_Send()
 * @param  data 数据指针
 * @param  len  数据长度（字节）
 * @note   原函数: void USART2_Send(char *Data, uint16_t Len) 位于 usart.c
 */
static void hal_uart2_send_data(const char *data, uint16_t len)
{
    USART2_Send((char *)data, len);
}

/**
 * @brief  UART2发送单字节 - 包装 UART2_send_byte()
 * @param  data 要发送的字节
 * @note   原函数: void UART2_send_byte(char data) 位于 usart.c
 *         注意原参数类型为char，此处从uint8_t隐式转换
 */
static void hal_uart2_send_byte(uint8_t data)
{
    UART2_send_byte((char)data);
}

/**
 * @brief  获取UART2接收缓冲区指针 - 包装 buf_uart2.buf
 * @return 接收缓冲区首地址
 * @note   原变量: UART_BUF buf_uart2 位于 usart.c
 *         buf_uart2.buf 为 char[BUFLEN+1] 数组
 */
static char* hal_uart2_get_buf(void)
{
    return buf_uart2.buf;
}

/**
 * @brief  清空UART2接收缓冲区 - 包装 Clear_Buffer()
 * @note   原函数: void Clear_Buffer(void) 位于 4G.c
 *         该函数会打印缓冲区内容、清零index、memset缓冲区、喂狗
 */
static void hal_uart2_clear_buf(void)
{
    Clear_Buffer();
}

/* ====== 系统操作包装函数 ====== */

/**
 * @brief  喂狗 - 包装 IWDG_Feed()
 * @note   原函数: void IWDG_Feed(void) 位于 wdg.c
 */
static void hal_feed_watchdog(void)
{
    IWDG_Feed();
}

/**
 * @brief  阻塞式毫秒延时 - 包装 delay_ms()
 * @param  ms 延时毫秒数
 * @note   原函数: void delay_ms(u16 nms) 位于 delay.c
 *         原参数为u16且受SysTick 24位限制，最大约1864ms
 *         此处包装为uint32_t，超过1000ms时分段调用
 */
static void hal_delay_ms(uint32_t ms)
{
    while (ms > 1000) {
        delay_ms(1000);
        ms -= 1000;
    }
    if (ms > 0) {
        delay_ms((u16)ms);
    }
}

/**
 * @brief  获取系统毫秒计数 - 包装 ng_timer_millis()
 * @return 自初始化以来的毫秒数
 * @note   调用新版计时模块 ng_timer.c 中的函数
 */
static uint32_t hal_get_millis(void)
{
    return ng_timer_millis();
}

/* ====== LED包装函数 ====== */
/* 原项目 led.h 定义: #define LED1  PBout(5)
 * Pbout() 是位带操作宏，直接操作GPIO输出寄存器
 * LED1=1 点亮, LED1=0 熄灭
 */

/** @brief  LED点亮 - 设置 PB5 输出高电平 */
static void hal_led_on(void)  { LED1 = 1; }

/** @brief  LED熄灭 - 设置 PB5 输出低电平 */
static void hal_led_off(void) { LED1 = 0; }

/** @brief  LED状态翻转 */
static void hal_led_toggle(void) { LED1 = !LED1; }

/* ====== 4G模块生命周期包装函数 ====== */

/**
 * @brief  4G模块初始化 - 包装 CSTX_4GCTR_Init() + CSTX_4G_Init()
 * @note   对应原项目 main.c 中的初始化顺序：
 *         CSTX_4GCTR_Init() -> 配置PA12 4G模块电源控制GPIO，拉低复位
 *         CSTX_4G_Init()    -> AT指令初始化4G模块（回显、SIM卡、网络注册、信号、GPS）
 */
static void hal_module_init(void)
{
    CSTX_4GCTR_Init();   /* 4G模块电源引脚初始化（led.c） */
    CSTX_4G_Init();      /* 4G模块AT指令初始化（4G.c） */
}

/**
 * @brief  获取IMEI号 - 包装 Get_4GIMEI_NUM()
 * @note   原函数返回char*，此处将结果拷贝到内部静态缓冲区
 */
static void hal_get_imei(void)
{
    char *ret = Get_4GIMEI_NUM();
    if (ret != NULL) {
        strncpy(imei_buffer, ret, sizeof(imei_buffer) - 1);
        imei_buffer[sizeof(imei_buffer) - 1] = '\0';
    }
}

/* ====== 4G模块MQTT包装函数 ====== */

/**
 * @brief  MQTT连接OneNET - 包装 CSTX_4G_RegALIYUNIOT()
 * @return 0=成功, 1=失败
 * @note   原函数: void CSTX_4G_RegALIYUNIOT(void) 位于 4G.c
 *         原函数内部等待 "+QMTCONN: 0,0,0" 响应，在 Clear_Buffer() 之前
 *         缓冲区中会保留连接结果，此处检查缓冲区判断连接是否成功
 */
static uint8_t hal_mqtt_connect(void)
{
    CSTX_4G_RegALIYUNIOT();

    /* 检查UART缓冲区中的连接结果（在Clear_Buffer之前） */
    char *buf = buf_uart2.buf;
    if (strstr(buf, "+QMTCONN: 0,0,0") != NULL) {
        /* 连接成功 */
        mqtt_connected_state = 1;
        Clear_Buffer();
        return 0;
    } else {
        /* 连接失败 */
        mqtt_connected_state = 0;
        Clear_Buffer();
        return 1;
    }
}

/**
 * @brief  MQTT发送数据 - 包装 CSTX_4G_ALYIOTSenddata()
 * @param  temp 温度值, humi 湿度值
 * @param  lat/lon/alt 纬度/经度/海拔（double类型，与4G.c定义一致）
 * @param  speed 速度, dir 方向角
 * @return 0=成功, 1=失败
 * @note   原函数: void CSTX_4G_ALYIOTSenddata(...) 位于 4G.c
 *         原函数返回void，参数顺序为 (speed, direction) 匹配4G.c定义
 *         包装为uint8_t：调用后检查UART缓冲区确认发送结果
 */
static uint8_t hal_mqtt_send(uint8_t temp, uint8_t humi,
                              double lat, double lon, double alt,
                              double speed, double dir)
{
    CSTX_4G_ALYIOTSenddata(temp, humi, lat, lon, alt, speed, dir);

    /* 检查UART缓冲区确认发送结果 */
    char *buf = buf_uart2.buf;
    if (strstr(buf, "+QMTPUBEX: 0,0,0") != NULL ||
        strstr(buf, "SEND OK") != NULL ||
        strstr(buf, "+QMTSUB: 0,0,0") != NULL) {
        Clear_Buffer();
        return 0;  /* 成功 */
    }

    Clear_Buffer();
    return 1;  /* 失败 */
}

/**
 * @brief  检查MQTT是否已连接（基于静态状态变量）
 * @return 1=已连接, 0=未连接
 * @note   使用静态变量 mqtt_connected_state 跟踪连接状态，
 *         避免依赖会被 Clear_Buffer() 清空的 buf_uart2.buf
 */
static uint8_t hal_mqtt_is_connected(void)
{
    return mqtt_connected_state;
}

/**
 * @brief  MQTT断开连接 - 发送 QMTDISC + QMTCLOSE 指令序列
 * @note   原项目没有独立的断开函数，此处实现断开逻辑
 */
static void hal_mqtt_disconnect(void)
{
    Uart2_SendStr("AT+QMTDISC=0\r\n");
    delay_ms(300);
    Clear_Buffer();

    Uart2_SendStr("AT+QMTCLOSE=0\r\n");
    delay_ms(300);
    Clear_Buffer();

    mqtt_connected_state = 0;
}

/**
 * @brief  MQTT重连 - 先断开再连接
 * @return 0=成功, 1=失败
 */
static uint8_t hal_mqtt_reconnect(void)
{
    hal_mqtt_disconnect();
    return hal_mqtt_connect();
}

/* ====== DHT11温湿度传感器包装函数（条件编译） ====== */

#if NG_USE_DHT11

/** @brief  DHT11初始化 - 包装 DHT11_Init() */
static void hal_dht11_init(void)
{
    DHT11_Init();
}

/**
 * @brief  DHT11读取温湿度 - 包装 DHT11_Read_TempAndHumidity()
 * @note   原函数返回SUCCESS/ERROR，此处包装为void
 *         读取结果存储在全局 DHT11_Data 结构体中
 */
static void hal_dht11_read(void)
{
    DHT11_Read_TempAndHumidity();
}

/** @brief  获取温度整数值 - 返回 DHT11_Data.temp_int */
static uint8_t hal_dht11_get_temp(void)
{
    return DHT11_Data.temp_int;
}

/** @brief  获取湿度整数值 - 返回 DHT11_Data.humi_int */
static uint8_t hal_dht11_get_humi(void)
{
    return DHT11_Data.humi_int;
}

#else /* NG_USE_DHT11 == 0：DHT11未启用，提供空实现 */

static void hal_dht11_init(void) {}
static void hal_dht11_read(void) {}
static uint8_t hal_dht11_get_temp(void) { return 0; }
static uint8_t hal_dht11_get_humi(void) { return 0; }

#endif /* NG_USE_DHT11 */

/* ====== HAL初始化 ====== */

/**
 * @brief  初始化HAL适配器，填充所有函数指针
 * @param  drv 驱动函数指针结构体指针
 * @note   将STM32平台的实际驱动函数地址填入结构体
 *         上层模块通过 drv->uart2_send_str() 等方式调用硬件
 */
void ng_hal_init(ng_hal_driver_t *drv)
{
    if (drv == NULL) {
        return;
    }

    /* UART操作函数 */
    drv->uart2_send_str  = hal_uart2_send_str;
    drv->uart2_send_data = hal_uart2_send_data;
    drv->uart2_send_byte = hal_uart2_send_byte;
    drv->uart2_get_buf   = hal_uart2_get_buf;
    drv->uart2_clear_buf = hal_uart2_clear_buf;

    /* 系统操作函数 */
    drv->feed_watchdog = hal_feed_watchdog;
    drv->delay_ms      = hal_delay_ms;
    drv->get_millis    = hal_get_millis;

    /* LED操作函数 */
    drv->led_on    = hal_led_on;
    drv->led_off   = hal_led_off;
    drv->led_toggle = hal_led_toggle;

    /* 4G模块生命周期 */
    drv->module_init = hal_module_init;
    drv->get_imei    = hal_get_imei;

    /* 4G模块MQTT操作函数 */
    drv->mqtt_connect      = hal_mqtt_connect;
    drv->mqtt_send         = hal_mqtt_send;
    drv->mqtt_is_connected = hal_mqtt_is_connected;
    drv->mqtt_disconnect   = hal_mqtt_disconnect;
    drv->mqtt_reconnect    = hal_mqtt_reconnect;

    /* DHT11温湿度传感器 */
    drv->dht11_init     = hal_dht11_init;
    drv->dht11_read     = hal_dht11_read;
    drv->dht11_get_temp = hal_dht11_get_temp;
    drv->dht11_get_humi = hal_dht11_get_humi;

    /* 重置MQTT连接状态 */
    mqtt_connected_state = 0;

    hal_initialized = 1;
}

/**
 * @brief  获取HAL驱动单例实例
 * @return 指向已初始化的 ng_hal_driver_t 结构体指针
 * @note   首次调用时自动初始化，之后直接返回已填充的结构体
 */
ng_hal_driver_t* ng_hal_get_driver(void)
{
    if (!hal_initialized) {
        ng_hal_init(&hal_driver);
    }
    return &hal_driver;
}
