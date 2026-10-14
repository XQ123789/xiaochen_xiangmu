/**
 * @file    ng_error.h
 * @brief   错误处理模块 - 错误计数、阈值回调、故障统计
 * @note    纯逻辑模块，不依赖STM32头文件，可在任何平台使用
 *          用于记录系统运行中各类错误的发生次数，支持阈值回调
 */

#ifndef __NG_ERROR_H
#define __NG_ERROR_H

#include <stdint.h>

/* 错误ID定义 */
#define NG_ERR_GPS_PARSE      0   /* GPS解析失败 */
#define NG_ERR_GPS_TIMEOUT    1   /* GPS读取超时 */
#define NG_ERR_MQTT_SEND      2   /* MQTT发送失败 */
#define NG_ERR_MQTT_CONNECT   3   /* MQTT连接失败 */
#define NG_ERR_DHT11_READ     4   /* DHT11读取失败 */
#define NG_ERR_UART_OVERFLOW  5   /* UART缓冲区溢出 */
#define NG_ERR_MAX_ID         8   /* 最大错误ID（错误槽位数） */

/* 恢复回调函数类型：当错误计数达到阈值时调用 */
typedef void (*ng_error_callback_t)(void);

/**
 * @brief  初始化错误处理器
 * @note   清零所有错误计数和回调设置
 */
void ng_error_init(void);

/**
 * @brief  记录一次错误
 * @param  error_id 错误ID（0 ~ NG_ERR_MAX_ID-1）
 * @note   递增对应错误ID的计数，达到阈值时自动触发回调
 */
void ng_error_record(uint8_t error_id);

/**
 * @brief  获取指定错误的累计次数
 * @param  error_id 错误ID
 * @return 累计错误次数（0~255）
 */
uint8_t ng_error_count(uint8_t error_id);

/**
 * @brief  设置错误阈值回调
 * @param  error_id  错误ID
 * @param  threshold 触发阈值（计数达到此值时调用回调）
 * @param  cb        回调函数指针（达到阈值时调用，传NULL取消回调）
 * @note   例如：设置MQTT发送失败阈值为3次，达到后自动触发重连
 */
void ng_error_set_callback(uint8_t error_id, uint8_t threshold, ng_error_callback_t cb);

/**
 * @brief  重置指定错误的计数
 * @param  error_id 错误ID
 */
void ng_error_reset(uint8_t error_id);

/**
 * @brief  重置所有错误计数
 * @note   同时清零所有阈值和回调设置
 */
void ng_error_reset_all(void);

#endif /* __NG_ERROR_H */
