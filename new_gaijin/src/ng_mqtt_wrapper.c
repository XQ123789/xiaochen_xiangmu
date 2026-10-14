/**
 * @file    ng_mqtt_wrapper.c
 * @brief   MQTT管理模块实现 - 连接管理、数据上传、自动重连、统计追踪
 * @note    依赖关系：
 *          - ng_hal.h:       通过 ng_hal_get_driver() 获取HAL驱动函数指针
 *                            使用 mqtt_connect / mqtt_send / mqtt_is_connected
 *          - ng_config.h:    读取 NG_MQTT_MAX_FAIL 重连阈值配置
 *          - ng_gps_parser.h: 使用 ng_gps_data_t 数据类型
 *          - ng_error.h:     记录MQTT相关错误（连接失败、发送失败）
 *
 * 设计说明：
 *   本模块不依赖 ng_json_builder，JSON构建由HAL层内部的 mqtt_send 完成
 *   （对应原项目 4G.c 中 CSTX_4G_ALYIOTSenddata 内部调用 MakeOnenetPayload）。
 *   本模块只负责连接管理、发送调度、失败重试和统计。
 *
 * 工作流程：
 *   1. 初始化时调用 ng_mqtt_wrapper_init() 清零统计、设置初始状态
 *   2. 发送数据前调用 ng_mqtt_ensure_connected() 确保连接
 *   3. 调用 ng_mqtt_send_gps_data() 发送数据
 *   4. 发送失败时递增连续失败计数，达到 NG_MQTT_MAX_FAIL 次触发重连
 *   5. 重连成功后重置连续失败计数
 */

#include "ng_mqtt_wrapper.h"
#include "ng_hal.h"
#include "ng_error.h"
#include <string.h>

/* ====== 模块内部变量 ====== */

/* 当前MQTT连接状态 */
static ng_mqtt_state_t mqtt_state = NG_MQTT_DISCONNECTED;

/* MQTT统计信息 */
static ng_mqtt_stats_t mqtt_stats;

/* ====== 内部辅助函数 ====== */

/**
 * @brief  通过HAL层检查MQTT实际连接状态
 * @return 1=HAL层确认已连接, 0=未连接或HAL不可用
 * @note   调用HAL层的 mqtt_is_connected 函数指针，
 *         对应原项目中检查 buf_uart2.buf 中 "+QMTCONN: 0,0,0" 的逻辑
 */
static int hal_check_connected(void)
{
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv == NULL || drv->mqtt_is_connected == NULL) {
        return 0;
    }
    return (drv->mqtt_is_connected() != 0) ? 1 : 0;
}

/* ====== 公开接口实现 ====== */

/**
 * @brief  初始化MQTT管理器
 * @note   清零统计信息，设置初始状态为 DISCONNECTED
 *         同时初始化错误模块中MQTT相关的错误计数
 */
void ng_mqtt_wrapper_init(void)
{
    /* 清零统计信息 */
    memset(&mqtt_stats, 0, sizeof(mqtt_stats));

    /* 设置初始状态 */
    mqtt_state = NG_MQTT_DISCONNECTED;
}

/**
 * @brief  连接MQTT服务器
 * @return 0=成功, 1=失败
 * @note   流程：
 *         1. 如果已处于 CONNECTED 状态，直接返回成功（避免重复连接）
 *         2. 设置状态为 CONNECTING
 *         3. 通过HAL层调用 mqtt_connect（对应原项目 CSTX_4G_RegALIYUNIOT）
 *         4. 连接后通过HAL层检查实际连接状态
 *         5. 成功则更新状态并递增重连计数；失败则记录错误
 */
int ng_mqtt_connect(void)
{
    /* 已连接则跳过 */
    if (mqtt_state == NG_MQTT_CONNECTED && hal_check_connected()) {
        return 0;
    }

    /* 标记为连接中 */
    mqtt_state = NG_MQTT_CONNECTING;

    /* 获取HAL驱动 */
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv == NULL || drv->mqtt_connect == NULL) {
        mqtt_state = NG_MQTT_ERROR;
        ng_error_record(NG_ERR_MQTT_CONNECT);
        return 1;
    }

    /* 调用HAL层连接函数（对应原项目 CSTX_4G_RegALIYUNIOT） */
    uint8_t ret = drv->mqtt_connect();

    /* 检查连接结果 */
    if (ret == 0) {
        /* HAL层返回成功，再通过 mqtt_is_connected 二次确认 */
        if (hal_check_connected()) {
            mqtt_state = NG_MQTT_CONNECTED;
            mqtt_stats.reconnect_count++;
            /* 重连成功，重置连续失败计数 */
            mqtt_stats.consecutive_fails = 0;
            return 0;
        }
    }

    /* 连接失败 */
    mqtt_state = NG_MQTT_ERROR;
    ng_error_record(NG_ERR_MQTT_CONNECT);
    return 1;
}

/**
 * @brief  检查MQTT是否已连接
 * @return 1=已连接, 0=未连接
 * @note   同时检查内部状态和HAL层实际连接状态
 */
int ng_mqtt_is_connected(void)
{
    if (mqtt_state != NG_MQTT_CONNECTED) {
        return 0;
    }
    return hal_check_connected();
}

/**
 * @brief  获取当前MQTT状态
 * @return 状态枚举值
 */
ng_mqtt_state_t ng_mqtt_get_state(void)
{
    return mqtt_state;
}

/**
 * @brief  上传GPS+传感器数据到OneNET
 * @param  temp      温度值
 * @param  humi      湿度值
 * @param  gps_data  已解析和坐标转换后的GPS数据
 * @return 0=成功, 1=失败
 * @note   直接调用HAL层的 mqtt_send 函数指针发送数据，
 *         不经过 ng_json_builder（JSON构建由HAL层内部完成）。
 *         对应原项目调用链：
 *         CSTX_4G_ALYIOTSenddata() -> MakeOnenetPayload() -> USART2_Send()
 */
int ng_mqtt_send_gps_data(uint8_t temp, uint8_t humi, const ng_gps_data_t *gps_data)
{
    /* 参数检查 */
    if (gps_data == NULL) {
        return 1;
    }

    /* 发送前确保连接 */
    if (ng_mqtt_ensure_connected() != 0) {
        /* 无法连接，记录失败 */
        mqtt_stats.send_fail_count++;
        ng_error_record(NG_ERR_MQTT_SEND);
        return 1;
    }

    /* 获取HAL驱动 */
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv == NULL || drv->mqtt_send == NULL) {
        mqtt_stats.send_fail_count++;
        ng_error_record(NG_ERR_MQTT_SEND);
        return 1;
    }

    /*
     * 直接调用HAL层 mqtt_send 发送数据
     * 对应原项目 CSTX_4G_ALYIOTSenddata()，内部会：
     *   1. 调用 MakeOnenetPayload() 构建JSON
     *   2. 通过 AT+QMTPUBEX 发布到OneNET
     *   3. 等待确认响应
     * 返回 0=成功, 1=失败
     */
    uint8_t ret = drv->mqtt_send(temp, humi,
                                  gps_data->lat,
                                  gps_data->lon,
                                  gps_data->alt,
                                  gps_data->speed,
                                  gps_data->direction);

    if (ret == 0) {
        /* 发送成功 */
        mqtt_stats.send_success_count++;
        /* 成功时重置连续失败计数 */
        mqtt_stats.consecutive_fails = 0;
        return 0;
    }

    /* 发送失败 */
    mqtt_stats.send_fail_count++;
    mqtt_stats.consecutive_fails++;
    ng_error_record(NG_ERR_MQTT_SEND);

    /*
     * 连续失败达到阈值时触发重连
     * 阈值由 ng_config.h 中的 NG_MQTT_MAX_FAIL 配置（默认3次）
     */
    if (mqtt_stats.consecutive_fails >= NG_MQTT_MAX_FAIL) {
        /* 标记为断开，下次 ensure_connected 会触发重连 */
        mqtt_state = NG_MQTT_DISCONNECTED;
    }

    return 1;
}

/**
 * @brief  确保MQTT已连接，未连接时自动重连
 * @return 0=已连接（或重连成功）, 1=重连失败
 * @note   发送数据前调用此函数，保证连接可用。
 *         参考 esp8266_improved.ino 中的 checkWiFiConnection() 模式：
 *         先检查状态，断开时尝试重连，避免每次发送都盲目连接。
 */
int ng_mqtt_ensure_connected(void)
{
    /* 先通过HAL层确认实际连接状态 */
    if (mqtt_state == NG_MQTT_CONNECTED && hal_check_connected()) {
        return 0;  /* 已连接，无需操作 */
    }

    /* 状态不一致：HAL层已断开但内部状态仍为CONNECTED */
    if (mqtt_state == NG_MQTT_CONNECTED && !hal_check_connected()) {
        mqtt_state = NG_MQTT_DISCONNECTED;
    }

    /* 尝试重连 */
    return ng_mqtt_connect();
}

/**
 * @brief  获取MQTT统计信息（只读）
 * @return 统计信息结构体常量指针
 * @note   返回内部静态变量的指针，调用者不可修改
 */
const ng_mqtt_stats_t* ng_mqtt_get_stats(void)
{
    return &mqtt_stats;
}

/**
 * @brief  重置MQTT统计信息
 * @note   清零所有计数器，但不改变当前连接状态
 */
void ng_mqtt_reset_stats(void)
{
    memset(&mqtt_stats, 0, sizeof(mqtt_stats));
}
