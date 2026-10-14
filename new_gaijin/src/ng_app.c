/**
 * @file    ng_app.c
 * @brief   应用层集成模块 - 顶层状态机实现
 * @note    整合所有子模块：GPS解析、坐标转换、MQTT上传、LED指示、错误恢复
 *          通过HAL层调用硬件驱动，不直接操作寄存器
 *
 * 依赖模块：
 *   - ng_hal.h:          HAL驱动接口（UART、LED、4G模块、MQTT）
 *   - ng_timer.h:        计时服务（毫秒延时、非阻塞定时）
 *   - ng_gps_parser.h:   GPS数据解析
 *   - ng_coord.h:        WGS84→GCJ02坐标转换
 *   - ng_mqtt_wrapper.h: MQTT连接管理与数据上传
 *   - ng_error.h:        错误计数与阈值回调
 *   - ng_led_status.h:   LED状态指示
 *   - ng_config.h:       全局配置参数
 */

#include "ng_app.h"
#include "ng_config.h"
#include "ng_hal.h"
#include "ng_timer.h"
#include "ng_gps_parser.h"
#include "ng_coord.h"
#include "ng_mqtt_wrapper.h"
#include "ng_error.h"
#include "ng_led_status.h"
#include <string.h>

/* ====== 静态变量 ====== */
static ng_app_state_t current_state;     /* 当前状态机状态 */
static ng_app_stats_t app_stats;         /* 应用统计信息 */
static ng_gps_data_t  gps_data;          /* 当前GPS解析结果 */
static ng_gps_data_t  gps_data_gcj02;    /* 坐标转换后的GCJ02结果 */
static uint8_t        gps_valid;         /* GPS数据是否有效 */
static uint8_t        dht11_temp;        /* 当前温度值 */
static uint8_t        dht11_humi;        /* 当前湿度值 */
static uint32_t       last_dht11_read_time; /* DHT11上次读取时间戳 */
static uint8_t        consecutive_gps_fail;   /* GPS连续失败计数 */
static uint8_t        consecutive_upload_fail; /* 上传连续失败计数 */

/* ====== 内部辅助函数声明 ====== */
static void app_do_module_init(void);
static void app_do_gps_read(void);
static void app_do_gps_process(void);
static void app_do_data_upload(void);
static void app_do_idle(void);
static void app_do_error_recovery(void);
static void app_read_dht11_if_needed(void);

/**
 * @brief  初始化应用层（初始化所有子模块）
 * @note   调用顺序：timer → error → led → mqtt → 状态设置
 *         需要先通过 ng_hal_init() 设置好HAL驱动函数指针
 */
void ng_app_init(void)
{
    /* 获取HAL驱动实例并检查有效性 */
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv == NULL) return;
    (void)drv;  /* 初始化阶段仅做NULL检查，后续状态机中使用drv */

    /* 1. 初始化计时器（TIM2毫秒计数器） */
    ng_timer_init();

    /* 2. 初始化错误处理模块（清零所有错误计数） */
    ng_error_init();

    /* 3. 初始化LED状态指示模块 */
    ng_led_status_init();

    /* 4. 初始化MQTT管理模块 */
    ng_mqtt_wrapper_init();

    /* 5. 初始化统计信息 */
    app_stats.loop_count           = 0;
    app_stats.gps_success_count    = 0;
    app_stats.gps_fail_count       = 0;
    app_stats.upload_success_count = 0;
    app_stats.upload_fail_count    = 0;
    app_stats.recovery_count       = 0;

    /* 6. 初始化运行时变量 */
    gps_valid = 0;
    dht11_temp = 0;
    dht11_humi = 0;
    last_dht11_read_time = 0;
    consecutive_gps_fail = 0;
    consecutive_upload_fail = 0;

    /* 7. 清空GPS数据结构 */
    ng_gps_data_t empty_gps = {0};
    gps_data = empty_gps;
    gps_data_gcj02 = empty_gps;

    /* 8. 设置初始状态 */
    current_state = NG_APP_STATE_INIT;

    /* 9. LED显示初始化模式 */
    ng_led_set_mode(NG_LED_INIT);
}

/**
 * @brief  执行一次主循环步骤
 * @note   每次调用执行一个状态转换，在main.c的while(1)中循环调用
 *         状态机流程：INIT → MODULE_INIT → GPS_READ → GPS_PROCESS → DATA_UPLOAD → IDLE → GPS_READ ...
 *                    任意状态失败 → ERROR_RECOVERY → GPS_READ
 */
void ng_app_loop_step(void)
{
    /* 递增主循环计数 */
    app_stats.loop_count++;

    /* 获取HAL驱动实例并检查有效性 */
    ng_hal_driver_t *drv = ng_hal_get_driver();
    if (drv == NULL) return;

    /* 喂看门狗（通过HAL层） */
    if (drv->feed_watchdog != NULL) {
        drv->feed_watchdog();
    }

    /* 更新LED状态（非阻塞，每次循环都需调用） */
    ng_led_update();

    /* 状态机分发 */
    switch (current_state)
    {
    /* ====== 状态0：初始化 ====== */
    case NG_APP_STATE_INIT:
    {
        /* LED显示初始化模式 */
        ng_led_set_mode(NG_LED_INIT);

        /* 初始化完成，转入模块初始化状态 */
        current_state = NG_APP_STATE_MODULE_INIT;
        break;
    }

    /* ====== 状态1：模块初始化（4G模块/DHT11/IMEI） ====== */
    case NG_APP_STATE_MODULE_INIT:
        app_do_module_init();
        break;

    /* ====== 状态2：读取GPS数据 ====== */
    case NG_APP_STATE_GPS_READ:
        app_do_gps_read();
        break;

    /* ====== 状态3：处理GPS数据（坐标转换、传感器读取） ====== */
    case NG_APP_STATE_GPS_PROCESS:
        app_do_gps_process();
        break;

    /* ====== 状态4：上传数据 ====== */
    case NG_APP_STATE_DATA_UPLOAD:
        app_do_data_upload();
        break;

    /* ====== 状态5：空闲等待 ====== */
    case NG_APP_STATE_IDLE:
        app_do_idle();
        break;

    /* ====== 状态6：错误恢复 ====== */
    case NG_APP_STATE_ERROR_RECOVERY:
        app_do_error_recovery();
        break;

    /* ====== 默认：未知状态，回到初始化 ====== */
    default:
        current_state = NG_APP_STATE_INIT;
        break;
    }
}

/**
 * @brief  获取当前应用状态
 * @return 当前状态枚举值
 */
ng_app_state_t ng_app_get_state(void)
{
    return current_state;
}

/**
 * @brief  获取应用统计信息
 * @return 统计信息结构体指针（只读）
 */
const ng_app_stats_t* ng_app_get_stats(void)
{
    return &app_stats;
}

/**
 * @brief  获取状态名称字符串（用于调试打印）
 * @param  state 状态枚举值
 * @return 状态中文名称字符串
 */
const char* ng_app_state_name(ng_app_state_t state)
{
    switch (state)
    {
    case NG_APP_STATE_INIT:           return "初始化";
    case NG_APP_STATE_MODULE_INIT:    return "模块初始化";
    case NG_APP_STATE_GPS_READ:       return "读取GPS";
    case NG_APP_STATE_GPS_PROCESS:    return "处理GPS";
    case NG_APP_STATE_DATA_UPLOAD:    return "数据上传";
    case NG_APP_STATE_IDLE:           return "空闲等待";
    case NG_APP_STATE_ERROR_RECOVERY: return "错误恢复";
    default:                          return "未知状态";
    }
}

/* ======================================================================
 *                        内部状态处理函数
 * ====================================================================== */

/**
 * @brief  模块初始化状态处理
 * @note   通过HAL调用4G模块初始化、DHT11初始化、获取IMEI
 *         成功后转入GPS_READ，任一环节失败进入ERROR_RECOVERY
 */
static void app_do_module_init(void)
{
    ng_hal_driver_t *drv = ng_hal_get_driver();

    /* LED显示初始化模式（快闪，表示系统启动中） */
    ng_led_set_mode(NG_LED_INIT);

    /* 1. 4G模块初始化（BC20/EC200U上电、AT指令握手） */
    if (drv->module_init != NULL) {
        drv->module_init();
    } else {
        /* module_init函数指针为空，模块不可用，进入错误恢复 */
        current_state = NG_APP_STATE_ERROR_RECOVERY;
        return;
    }

    /* 2. DHT11温湿度传感器初始化（GPIO配置） */
    if (drv->dht11_init != NULL) {
        drv->dht11_init();
    }

    /* 3. 获取4G模块IMEI号（用于设备标识） */
    if (drv->get_imei != NULL) {
        drv->get_imei();
    }

    /* 模块初始化完成，转入GPS读取状态 */
    current_state = NG_APP_STATE_GPS_READ;
}

/**
 * @brief  GPS读取状态处理
 * @note   清空缓冲区 → 发送AT指令 → 等待响应 → 复制缓冲区 → 解析数据
 *         成功 → GPS_PROCESS；失败达到阈值 → ERROR_RECOVERY
 */
static void app_do_gps_read(void)
{
    ng_hal_driver_t *drv = ng_hal_get_driver();

    /* LED显示GPS搜索中（快闪，表示正在定位） */
    ng_led_set_mode(NG_LED_INIT);

    /* 清空UART2接收缓冲区 */
    if (drv->uart2_clear_buf != NULL) {
        drv->uart2_clear_buf();
    }

    /* 发送GPS定位查询AT指令 */
    if (drv->uart2_send_str != NULL) {
        drv->uart2_send_str("AT+QGPSLOC=2\r\n");
    }

    /* 等待GPS模块响应（阻塞延时） */
    ng_timer_delay_ms(NG_GPS_WAIT_MS);

    /* 获取UART接收缓冲区内容，复制到本地缓冲区避免竞争 */
    char local_buf[256];
    char *uart_buf = NULL;
    if (drv->uart2_get_buf != NULL) {
        uart_buf = drv->uart2_get_buf();
    }

    /* 将UART缓冲区内容复制到本地变量，防止解析过程中缓冲区被中断修改 */
    if (uart_buf != NULL) {
        strncpy(local_buf, uart_buf, sizeof(local_buf) - 1);
        local_buf[sizeof(local_buf) - 1] = '\0';
    }

    /* 解析GPS响应数据（使用本地副本，避免缓冲区竞争） */
    if (uart_buf != NULL) {
        int parse_ret = ng_gps_parse(local_buf, &gps_data);
        if (parse_ret == NG_GPS_OK && ng_gps_is_valid(&gps_data)) {
            /* GPS解析成功 */
            gps_valid = 1;
            consecutive_gps_fail = 0;    /* 清零连续失败计数 */
            app_stats.gps_success_count++;
        } else {
            /* GPS解析失败 */
            gps_valid = 0;
            consecutive_gps_fail++;
            app_stats.gps_fail_count++;
            ng_error_record(NG_ERR_GPS_PARSE);
        }
    } else {
        /* 缓冲区为空，视为无响应 */
        gps_valid = 0;
        consecutive_gps_fail++;
        app_stats.gps_fail_count++;
        ng_error_record(NG_ERR_GPS_TIMEOUT);
    }

    /* 检查GPS连续失败是否达到阈值 */
    if (consecutive_gps_fail >= NG_GPS_MAX_FAIL) {
        current_state = NG_APP_STATE_ERROR_RECOVERY;
        return;
    }

    /* 进入数据处理状态 */
    current_state = NG_APP_STATE_GPS_PROCESS;
}

/**
 * @brief  GPS数据处理状态
 * @note   按间隔读取DHT11温湿度 → WGS84转GCJ02坐标 → 转入上传
 */
static void app_do_gps_process(void)
{
    /* 按配置间隔读取DHT11温湿度（非阻塞判断） */
    app_read_dht11_if_needed();

    /* 如果GPS数据有效，进行坐标转换 */
    if (gps_valid) {
        int coord_ret = ng_coord_wgs84_to_gcj02(
            (ng_float_t)gps_data.lon,
            (ng_float_t)gps_data.lat,
            (ng_float_t *)&gps_data_gcj02.lon,
            (ng_float_t *)&gps_data_gcj02.lat
        );

        if (coord_ret == 0) {
            /* 坐标转换成功，保留原始海拔/速度/方向 */
            gps_data_gcj02.alt      = gps_data.alt;
            gps_data_gcj02.speed    = gps_data.speed;
            gps_data_gcj02.direction = gps_data.direction;
            gps_data_gcj02.valid    = gps_data.valid;
        } else {
            /* 坐标转换失败，使用原始数据 */
            gps_data_gcj02 = gps_data;
        }
    }

    /* 转入数据上传状态 */
    current_state = NG_APP_STATE_DATA_UPLOAD;
}

/**
 * @brief  数据上传状态处理
 * @note   GPS有效时通过MQTT上传数据；GPS无效时跳过上传
 *         上传成功清零失败计数；连续失败达阈值触发恢复
 */
static void app_do_data_upload(void)
{
    if (gps_valid) {
        /* GPS有效，执行数据上传 */
        ng_led_set_mode(NG_LED_UPLOADING);

        /* 通过MQTT包装器上传GPS+传感器数据 */
        int send_ret = ng_mqtt_send_gps_data(dht11_temp, dht11_humi, &gps_data_gcj02);

        if (send_ret == 0) {
            /* 上传成功 */
            consecutive_upload_fail = 0;  /* 清零连续失败计数 */
            app_stats.upload_success_count++;
        } else {
            /* 上传失败 */
            consecutive_upload_fail++;
            app_stats.upload_fail_count++;
            ng_error_record(NG_ERR_MQTT_SEND);
        }

        /* 检查上传连续失败是否达到阈值 */
        if (consecutive_upload_fail >= NG_MQTT_MAX_FAIL) {
            current_state = NG_APP_STATE_ERROR_RECOVERY;
            return;
        }
    } else {
        /* GPS数据无效，跳过上传，避免污染云端数据 */
        /* （此处不递增upload_fail_count，因为不是通信错误） */
    }

    /* 转入空闲等待状态 */
    current_state = NG_APP_STATE_IDLE;
}

/**
 * @brief  空闲等待状态处理
 * @note   根据GPS有效性显示不同LED模式，延时后回到GPS读取
 */
static void app_do_idle(void)
{
    /* 根据GPS是否有效显示不同LED模式 */
    if (gps_valid) {
        ng_led_set_mode(NG_LED_READY);       /* GPS有效：慢闪就绪 */
    } else {
        ng_led_set_mode(NG_LED_GPS_FAIL);    /* GPS无效：双闪警告 */
    }

    /* 主循环间隔延时 */
    ng_timer_delay_ms(NG_LOOP_INTERVAL_MS);

    /* 回到GPS读取，开始新一轮循环 */
    current_state = NG_APP_STATE_GPS_READ;
}

/**
 * @brief  错误恢复状态处理
 * @note   LED错误指示 → 分段延时喂狗 → 重新初始化4G和MQTT → 重置计数 → 回到GPS读取
 */
static void app_do_error_recovery(void)
{
    ng_hal_driver_t *drv = ng_hal_get_driver();

    /* LED显示错误模式（三闪） */
    ng_led_set_mode(NG_LED_ERROR);

    /* 递增恢复计数 */
    app_stats.recovery_count++;

    /* 分段延时，每段喂狗，防止看门狗复位（总延时 NG_ERROR_RECOVERY_DELAY_MS） */
    for (int i = 0; i < (NG_ERROR_RECOVERY_DELAY_MS / 500); i++) {
        ng_timer_delay_ms(500);
        if (drv->feed_watchdog != NULL) {
            drv->feed_watchdog();
        }
    }

    /* 重新初始化4G模块并连接MQTT */
    int mqtt_ret = ng_mqtt_connect();
    if (mqtt_ret != 0) {
        /* 重连失败，但继续回到工作循环重试（ng_mqtt_connect内部已记录错误） */
    }

    /* 重置所有连续失败计数 */
    consecutive_gps_fail = 0;
    consecutive_upload_fail = 0;

    /* 逐个重置错误计数（保留已配置的阈值回调，不使用reset_all以免破坏回调） */
    ng_error_reset(NG_ERR_GPS_PARSE);
    ng_error_reset(NG_ERR_GPS_TIMEOUT);
    ng_error_reset(NG_ERR_MQTT_SEND);
    ng_error_reset(NG_ERR_MQTT_CONNECT);

    /* 回到GPS读取状态 */
    current_state = NG_APP_STATE_GPS_READ;
}

/**
 * @brief  按间隔读取DHT11温湿度（非阻塞）
 * @note   使用ng_timer_expired()判断是否到达读取间隔
 *         间隔由 NG_DHT11_INTERVAL_MS 配置
 *         DHT11最高采样率1Hz，不必每次循环都读
 *         通过HAL函数指针访问传感器，不直接操作外部变量
 */
static void app_read_dht11_if_needed(void)
{
    ng_hal_driver_t *drv = ng_hal_get_driver();

    /* 非阻塞检查是否到了读取时间 */
    if (ng_timer_expired(last_dht11_read_time, NG_DHT11_INTERVAL_MS)) {
        /* 通过HAL调用DHT11读取 */
        if (drv->dht11_read != NULL) {
            drv->dht11_read();
        }

        /* 通过HAL函数指针获取温湿度值 */
        if (drv->dht11_get_temp != NULL) {
            dht11_temp = drv->dht11_get_temp();
        }
        if (drv->dht11_get_humi != NULL) {
            dht11_humi = drv->dht11_get_humi();
        }

        /* 记录本次读取时间 */
        last_dht11_read_time = ng_timer_millis();
    }
}
