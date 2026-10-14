/**
 * @file    config.h
 * @brief   系统配置集中管理头文件
 * @note    改进版 - 将原项目中分散的魔数和硬编码配置统一提取到此处
 *          原项目问题：服务器地址、端口、超时时间等散落在各源文件中，修改困难
 *
 * 使用方法：在需要配置的源文件中 #include "config.h" 即可
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* ============ 通信配置 ============ */
/* OneNET MQTT 服务器地址和端口 */
#define SERVER_IP           "mqtts.heclouds.com"
#define SERVER_PORT         1883

/*
 * OneNET 平台配置
 * 注意：实际使用时必须填写自己的产品密钥、设备名称和设备鉴权信息
 * 原项目中这些信息硬编码在 4G.c 的宏定义中，现统一在此管理
 */
#define PRODUCT_KEY         "your_product_key"
#define DEVICE_NAME         "your_device_name"
#define DEVICE_SECRET       "your_device_secret"

/* APN配置 - 中国移动默认 cmnet */
#define APN_NAME            "cmnet"

/* ============ UART配置 ============ */
#define UART_DEBUG_BAUD     115200    /* UART1 - 调试串口 */
#define UART_4G_BAUD        115200    /* UART2 - 4G模块(BC20/EC200U-CN) */
#define UART_PI_BAUD        115200    /* UART3 - 树莓派通信 */

/* ============ 缓冲区配置 ============ */
#define UART_BUF_SIZE       256       /* UART接收缓冲区大小 */
#define AT_CMD_BUF_SIZE     256       /* AT指令缓冲区 */
#define JSON_BUF_SIZE       256       /* JSON数据缓冲区 */
#define GPS_PARSE_BUF_SIZE  128       /* GPS解析缓冲区 */

/* ============ 超时与重试配置 ============ */
#define AT_CMD_TIMEOUT_MS   3000      /* AT指令超时时间(ms) */
#define AT_CMD_MAX_RETRY    3         /* AT指令最大重试次数 */
#define GPS_WAIT_MS         500       /* GPS数据等待时间(ms) */
#define LOOP_INTERVAL_MS    1000      /* 主循环空闲间隔时间(ms) */
#define MQTT_CONNECT_TIMEOUT 10000    /* MQTT连接超时(ms) */

/* ============ 传感器配置 ============ */
#define DHT11_READ_INTERVAL 5000      /* DHT11读取间隔(ms)，DHT11最高采样频率1Hz */
#define DHT11_PIN           GPIO_Pin_11  /* DHT11数据引脚 */

/* ============ 看门狗配置 ============ */
/*
 * 独立看门狗(IWDG)配置
 * 预分频=6, 重装值=500
 * 超时时间 ≈ (预分频 * 重装值) / LSI频率 ≈ (6 * 500) / 40000 ≈ 0.075s
 * 实际需根据LSI频率(约30-60kHz，典型40kHz)计算
 * 如需约4秒超时，可调整预分频和重装值
 */
#define WDG_PRESCALE        6         /* 看门狗预分频 */
#define WDG_RELOAD          500       /* 看门狗重装值 */

/* ============ LED状态指示 ============ */
/* LED闪烁模式定义 - 用于指示系统当前工作状态 */
#define LED_MODE_INIT       0   /* 初始化中：快闪(200ms) */
#define LED_MODE_READY      1   /* 就绪：慢闪(1000ms) */
#define LED_MODE_GPS_OK     2   /* GPS定位成功：常亮 */
#define LED_MODE_GPS_FAIL   3   /* GPS定位失败：双闪 */
#define LED_MODE_UPLOADING  4   /* 数据上传中：快闪(100ms) */
#define LED_MODE_ERROR      5   /* 错误：三闪 */

#endif /* __CONFIG_H */
