/**
 * @file    ng_led_status.c
 * @brief   LED状态指示模块实现 - 非阻塞闪烁模式控制
 * @note    依赖关系：
 *          - ng_timer.h: 提供 ng_timer_millis() 非阻塞计时
 *          - ng_hal.h:   提供 led_on/led_off/led_toggle 硬件操作
 *
 * 工作原理：
 *   每种LED模式定义一个闪烁序列（时间数组），序列中每个元素
 *   包含（LED状态, 持续时间）。ng_led_update() 在主循环中被周期调用，
 *   根据当前时间和序列配置自动切换LED状态，不阻塞主循环。
 */

#include "ng_led_status.h"
#include "ng_hal.h"
#include "ng_timer.h"
#include "ng_config.h"

/* ====== 闪烁序列定义 ====== */

/* 单个闪烁步骤：LED状态 + 持续时间(毫秒) */
typedef struct {
    uint8_t  on;         /* 1=亮, 0=灭 */
    uint16_t duration;   /* 持续时间(毫秒) */
} ng_led_step_t;

/*
 * 各模式的闪烁序列
 * 序列最后一个元素的 duration=0 表示结束标记，循环回起点
 */

/* INIT: 快闪 200ms亮/200ms灭 */
static const ng_led_step_t seq_init[] = {
    {1, 200},   /* 亮 200ms */
    {0, 200},   /* 灭 200ms */
};

/* READY: 慢闪 1000ms亮/1000ms灭 */
static const ng_led_step_t seq_ready[] = {
    {1, 1000},  /* 亮 1000ms */
    {0, 1000},  /* 灭 1000ms */
};

/* GPS_OK: 常亮（只有一个亮步骤，持续0作为结束标记，实际由特殊逻辑处理） */
static const ng_led_step_t seq_gps_ok[] = {
    {1, 0},     /* 常亮，duration=0 表示特殊模式 */
};

/* GPS_FAIL: 双闪 - 亮200ms 灭200ms 亮200ms 灭800ms 循环 */
static const ng_led_step_t seq_gps_fail[] = {
    {1, 200},   /* 第一闪：亮 */
    {0, 200},   /* 灭 */
    {1, 200},   /* 第二闪：亮 */
    {0, 800},   /* 长灭 */
};

/* UPLOADING: 快闪 100ms亮/100ms灭 */
static const ng_led_step_t seq_uploading[] = {
    {1, 100},   /* 亮 100ms */
    {0, 100},   /* 灭 100ms */
};

/* ERROR: 三闪 - 亮100ms 灭100ms 亮100ms 灭100ms 亮100ms 灭500ms 循环 */
static const ng_led_step_t seq_error[] = {
    {1, 100},   /* 第一闪：亮 */
    {0, 100},   /* 灭 */
    {1, 100},   /* 第二闪：亮 */
    {0, 100},   /* 灭 */
    {1, 100},   /* 第三闪：亮 */
    {0, 500},   /* 长灭 */
};

/* RECOVERY: 交替快慢闪 - 快闪3次后慢闪1次 循环 */
static const ng_led_step_t seq_recovery[] = {
    {1, 100},   /* 快闪1：亮 */
    {0, 100},   /* 灭 */
    {1, 100},   /* 快闪2：亮 */
    {0, 100},   /* 灭 */
    {1, 100},   /* 快闪3：亮 */
    {0, 100},   /* 灭 */
    {1, 500},   /* 慢闪：亮500ms */
    {0, 500},   /* 灭500ms */
};

/* ====== 模式到序列的映射 ====== */

/* 每个模式对应的序列指针和长度 */
typedef struct {
    const ng_led_step_t *seq;    /* 序列数组指针 */
    uint8_t              count;  /* 序列元素个数 */
} ng_led_pattern_t;

/*
 * 模式映射表，下标对应 ng_led_mode_t 枚举值
 * NG_LED_OFF 和 NG_LED_GPS_OK 做特殊处理，此处用NULL标记
 */
static const ng_led_pattern_t pattern_table[] = {
    [NG_LED_OFF]       = {NULL, 0},                    /* 特殊：直接关闭 */
    [NG_LED_INIT]      = {seq_init,      sizeof(seq_init)      / sizeof(seq_init[0])},
    [NG_LED_READY]     = {seq_ready,     sizeof(seq_ready)     / sizeof(seq_ready[0])},
    [NG_LED_GPS_OK]    = {seq_gps_ok,    sizeof(seq_gps_ok)    / sizeof(seq_gps_ok[0])},
    [NG_LED_GPS_FAIL]  = {seq_gps_fail,  sizeof(seq_gps_fail)  / sizeof(seq_gps_fail[0])},
    [NG_LED_UPLOADING] = {seq_uploading, sizeof(seq_uploading) / sizeof(seq_uploading[0])},
    [NG_LED_ERROR]     = {seq_error,     sizeof(seq_error)     / sizeof(seq_error[0])},
    [NG_LED_RECOVERY]  = {seq_recovery,  sizeof(seq_recovery)  / sizeof(seq_recovery[0])},
};

/* ====== 模块内部变量 ====== */

static ng_led_mode_t current_mode = NG_LED_OFF;   /* 当前LED模式 */
static uint8_t       step_index   = 0;             /* 当前闪烁序列步骤索引 */
static uint32_t      step_start   = 0;             /* 当前步骤开始时间(ms) */

/* ====== 内部辅助函数 ====== */

/**
 * @brief  根据模式执行LED动作（非序列模式特殊处理）
 * @param  mode LED模式
 */
static void ng_led_apply_mode(ng_led_mode_t mode)
{
    ng_hal_driver_t *drv = ng_hal_get_driver();

    if (drv == NULL) return;

    switch (mode) {
    case NG_LED_OFF:
        /* 直接关闭LED */
        if (drv->led_off) drv->led_off();
        break;

    case NG_LED_GPS_OK:
        /* 常亮 */
        if (drv->led_on) drv->led_on();
        break;

    default:
        /* 其他模式由序列驱动，此处不处理 */
        break;
    }
}

/* ====== 公开接口实现 ====== */

/**
 * @brief  初始化LED状态指示模块
 * @note   设置初始模式为OFF，清零序列索引和时间戳
 */
void ng_led_status_init(void)
{
    current_mode = NG_LED_OFF;
    step_index   = 0;
    step_start   = 0;

    /* 确保LED初始为关闭状态 */
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv != NULL && drv->led_off != NULL) {
        drv->led_off();
    }
}

/**
 * @brief  设置LED显示模式
 * @param  mode 目标模式
 * @note   切换模式时重置序列索引和时间戳，立即生效
 */
void ng_led_set_mode(ng_led_mode_t mode)
{
    if (mode == current_mode) {
        return;  /* 模式未变，无需操作 */
    }

    current_mode = mode;
    step_index   = 0;
    step_start   = ng_timer_millis();

    /* 对非序列模式（OFF/GPS_OK），立即执行 */
    ng_led_apply_mode(mode);
}

/**
 * @brief  获取当前LED模式
 * @return 当前模式枚举值
 */
ng_led_mode_t ng_led_get_mode(void)
{
    return current_mode;
}

/**
 * @brief  LED状态更新（非阻塞）
 * @note   在主循环中周期调用此函数，内部根据 ng_timer_millis() 判断
 *         是否需要切换到下一个闪烁步骤。
 *
 * 处理流程：
 *   1. OFF / GPS_OK 模式：无需更新（已在 set_mode 中处理）
 *   2. 其他模式：查表获取当前序列，检查当前步骤是否到期
 *   3. 到期则切换到下一步，设置LED状态，重置计时
 *   4. 序列结束则循环回起点
 */
void ng_led_update(void)
{
    /* OFF和GPS_OK为非序列模式，不需要更新 */
    if (current_mode == NG_LED_OFF || current_mode == NG_LED_GPS_OK) {
        return;
    }

    /* 获取HAL驱动 */
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv == NULL) return;

    /* 获取当前模式对应的闪烁序列 */
    const ng_led_pattern_t *pattern = &pattern_table[current_mode];

    /* 安全检查：序列为空则返回 */
    if (pattern->seq == NULL || pattern->count == 0) {
        return;
    }

    /* 检查当前步骤是否已到期 */
    uint32_t now = ng_timer_millis();
    const ng_led_step_t *cur_step = &pattern->seq[step_index];

    /*
     * duration=0 的特殊处理：
     * 对于 GPS_OK 已在 set_mode 中处理，此处不会到达
     * 其他模式 duration 均 > 0
     */
    if (cur_step->duration == 0) {
        return;
    }

    if ((now - step_start) >= cur_step->duration) {
        /* 当前步骤到期，切换到下一步 */
        step_index++;

        /* 序列循环：到达末尾则回到起点 */
        if (step_index >= pattern->count) {
            step_index = 0;
        }

        /* 更新步骤起始时间 */
        step_start = now;

        /* 根据新步骤设置LED状态 */
        const ng_led_step_t *new_step = &pattern->seq[step_index];
        if (new_step->on) {
            if (drv->led_on) drv->led_on();
        } else {
            if (drv->led_off) drv->led_off();
        }
    }
}
