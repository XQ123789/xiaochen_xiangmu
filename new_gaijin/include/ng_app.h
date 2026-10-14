/**
 * @file    ng_app.h
 * @brief   应用层集成模块 - 顶层状态机，整合所有子模块
 * @note    通过状态机协调GPS解析、坐标转换、数据上传、LED指示、错误恢复等功能
 *          在main.c的while(1)中调用 ng_app_loop_step() 替代原有业务逻辑
 */

#ifndef __NG_APP_H
#define __NG_APP_H

#include <stdint.h>

/* 应用状态枚举 */
typedef enum {
    NG_APP_STATE_INIT = 0,        /* 初始化 */
    NG_APP_STATE_MODULE_INIT,     /* 模块初始化（4G模块/DHT11/IMEI） */
    NG_APP_STATE_GPS_READ,        /* 读取GPS */
    NG_APP_STATE_GPS_PROCESS,     /* 处理GPS数据（坐标转换） */
    NG_APP_STATE_DATA_UPLOAD,     /* 上传数据 */
    NG_APP_STATE_IDLE,            /* 空闲等待 */
    NG_APP_STATE_ERROR_RECOVERY   /* 错误恢复 */
} ng_app_state_t;

/* 应用统计信息 */
typedef struct {
    uint32_t loop_count;           /* 主循环总次数 */
    uint32_t gps_success_count;    /* GPS成功次数 */
    uint32_t gps_fail_count;       /* GPS失败次数 */
    uint32_t upload_success_count; /* 上传成功次数 */
    uint32_t upload_fail_count;    /* 上传失败次数 */
    uint32_t recovery_count;       /* 恢复触发次数 */
} ng_app_stats_t;

/**
 * @brief  初始化应用层（初始化所有子模块）
 * @note   需要先调用 ng_hal_init() 设置HAL驱动
 */
void ng_app_init(void);

/**
 * @brief  执行一次主循环步骤
 * @note   在main.c的while(1)中调用此函数替代原有逻辑
 */
void ng_app_loop_step(void);

/**
 * @brief  获取当前应用状态
 * @return 当前状态枚举值
 */
ng_app_state_t ng_app_get_state(void);

/**
 * @brief  获取应用统计信息
 * @return 统计信息结构体指针
 */
const ng_app_stats_t* ng_app_get_stats(void);

/**
 * @brief  获取状态名称字符串（用于调试打印）
 * @param  state 状态枚举值
 * @return 状态名称字符串
 */
const char* ng_app_state_name(ng_app_state_t state);

#endif /* __NG_APP_H */
