/**
 * @file    ng_error.c
 * @brief   错误处理模块实现 - 错误计数、阈值检测、回调触发
 * @note    纯逻辑模块，不依赖任何STM32头文件或硬件
 *          使用数组存储每个错误ID的计数、阈值和回调函数
 *
 * 工作流程：
 *   1. 系统初始化时调用 ng_error_init() 清零所有计数
 *   2. 各模块检测到错误时调用 ng_error_record(error_id) 记录
 *   3. record内部自动递增计数，若达到阈值则触发回调
 *   4. 主循环可调用 ng_error_count() 查询错误统计
 */

#include "ng_error.h"
#include <string.h>

/* ====== 模块内部数据结构 ====== */

/* 单个错误通道的完整配置 */
typedef struct {
    uint8_t count;              /* 当前错误计数（0~255） */
    uint8_t threshold;          /* 触发阈值（0表示不启用回调） */
    ng_error_callback_t cb;     /* 回调函数指针 */
} ng_error_slot_t;

/* 错误槽位数组，每个错误ID对应一个槽位 */
static ng_error_slot_t error_slots[NG_ERR_MAX_ID];

/**
 * @brief  初始化错误处理器，清零所有槽位
 * @note   系统启动时调用一次即可
 */
void ng_error_init(void)
{
    memset(error_slots, 0, sizeof(error_slots));
}

/**
 * @brief  记录一次错误并检查阈值
 * @param  error_id 错误ID（0 ~ NG_ERR_MAX_ID-1）
 * @note   内部逻辑：
 *         1. 参数越界检查：error_id >= NG_ERR_MAX_ID 时直接返回
 *         2. 计数器递增（饱和在255，不会溢出回绕）
 *         3. 若启用了阈值回调且计数恰好等于阈值，触发回调
 *            使用"恰好等于"而非">="，避免回调被重复触发
 */
void ng_error_record(uint8_t error_id)
{
    /* 参数越界保护 */
    if (error_id >= NG_ERR_MAX_ID) {
        return;
    }

    ng_error_slot_t *slot = &error_slots[error_id];

    /* 计数器递增，饱和在255防止溢出回绕 */
    if (slot->count < 255) {
        slot->count++;
    }

    /* 检查是否达到阈值，触发回调 */
    /* 使用 threshold > 0 判断是否启用回调，避免 threshold=0 时每次record都触发 */
    if (slot->threshold > 0 && slot->count == slot->threshold) {
        if (slot->cb != NULL) {
            slot->cb();
        }
    }
}

/**
 * @brief  获取指定错误的累计次数
 * @param  error_id 错误ID
 * @return 累计错误次数，参数越界时返回0
 */
uint8_t ng_error_count(uint8_t error_id)
{
    if (error_id >= NG_ERR_MAX_ID) {
        return 0;
    }
    return error_slots[error_id].count;
}

/**
 * @brief  设置错误阈值回调
 * @param  error_id  错误ID
 * @param  threshold 触发阈值（计数达到此值时调用回调，0=禁用回调）
 * @param  cb        回调函数指针（threshold>0 且 cb!=NULL 时生效）
 * @note   典型用法：
 *         ng_error_set_callback(NG_ERR_MQTT_SEND, 3, mqtt_reconnect_cb);
 *         // MQTT发送连续失败3次后自动调用 mqtt_reconnect_cb()
 */
void ng_error_set_callback(uint8_t error_id, uint8_t threshold, ng_error_callback_t cb)
{
    if (error_id >= NG_ERR_MAX_ID) {
        return;
    }
    error_slots[error_id].threshold = threshold;
    error_slots[error_id].cb = cb;
}

/**
 * @brief  重置指定错误的计数（保留阈值和回调设置）
 * @param  error_id 错误ID
 * @note   用于错误恢复后清零计数，阈值回调设置不变
 */
void ng_error_reset(uint8_t error_id)
{
    if (error_id >= NG_ERR_MAX_ID) {
        return;
    }
    error_slots[error_id].count = 0;
}

/**
 * @brief  重置所有错误计数和配置
 * @note   完全清零所有槽位（计数、阈值、回调全部清除）
 *         如需保留配置只清零计数，请逐个调用 ng_error_reset()
 */
void ng_error_reset_all(void)
{
    memset(error_slots, 0, sizeof(error_slots));
}
