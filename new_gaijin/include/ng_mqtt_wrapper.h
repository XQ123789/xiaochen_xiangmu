/**
 * @file    ng_mqtt_wrapper.h
 * @brief   MQTT管理模块 - 连接管理、数据上传、自动重连、统计信息
 * @note    依赖关系：
 *          - ng_hal.h:       通过HAL层调用 mqtt_connect/mqtt_send/mqtt_is_connected
 *          - ng_config.h:    读取 NG_MQTT_MAX_FAIL 等配置参数
 *          - ng_gps_parser.h: 使用 ng_gps_data_t 数据类型
 *
 * 设计说明：
 *   本模块不依赖 ng_json_builder，直接通过 HAL 层的 mqtt_send 函数指针
 *   发送数据，JSON构建由HAL层内部完成（与原项目 4G.c 保持一致）。
 */

#ifndef __NG_MQTT_WRAPPER_H
#define __NG_MQTT_WRAPPER_H

#include "ng_config.h"
#include "ng_gps_parser.h"
#include <stdint.h>

/* MQTT状态 */
typedef enum {
    NG_MQTT_DISCONNECTED = 0,  /* 未连接 */
    NG_MQTT_CONNECTING,        /* 连接中 */
    NG_MQTT_CONNECTED,         /* 已连接 */
    NG_MQTT_ERROR               /* 错误状态 */
} ng_mqtt_state_t;

/* MQTT统计信息 */
typedef struct {
    uint32_t send_success_count;   /* 成功发送次数 */
    uint32_t send_fail_count;      /* 发送失败次数 */
    uint32_t reconnect_count;      /* 重连次数 */
    uint8_t  consecutive_fails;    /* 连续失败次数 */
} ng_mqtt_stats_t;

/**
 * @brief  初始化MQTT管理器
 * @note   需要先通过ng_hal_init()设置好HAL驱动
 */
void ng_mqtt_wrapper_init(void);

/**
 * @brief  连接MQTT服务器
 * @return 0=成功, 1=失败
 */
int ng_mqtt_connect(void);

/**
 * @brief  检查MQTT是否已连接
 * @return 1=已连接, 0=未连接
 */
int ng_mqtt_is_connected(void);

/**
 * @brief  获取当前MQTT状态
 * @return 状态枚举值
 */
ng_mqtt_state_t ng_mqtt_get_state(void);

/**
 * @brief  上传GPS+传感器数据到OneNET
 * @param  temp      温度
 * @param  humi      湿度
 * @param  gps_data  GPS数据（已解析和转换）
 * @return 0=成功, 1=失败
 * @note   内部直接调用HAL层 mqtt_send，不经过 ng_json_builder
 */
int ng_mqtt_send_gps_data(uint8_t temp, uint8_t humi, const ng_gps_data_t *gps_data);

/**
 * @brief  尝试自动重连（如果已断开）
 * @return 0=已连接（或重连成功）, 1=重连失败
 */
int ng_mqtt_ensure_connected(void);

/**
 * @brief  获取MQTT统计信息
 * @return 统计信息结构体指针
 */
const ng_mqtt_stats_t* ng_mqtt_get_stats(void);

/**
 * @brief  重置MQTT统计信息
 */
void ng_mqtt_reset_stats(void);

#endif /* __NG_MQTT_WRAPPER_H */
