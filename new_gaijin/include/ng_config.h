/**
 * @file    ng_config.h
 * @brief   新版改进工程 - 编译开关与配置参数
 * @note    改进自 improve/firmware/stm32/config.h
 *          将所有功能模块的开关、算法参数、通信配置集中管理
 */

#ifndef __NG_CONFIG_H
#define __NG_CONFIG_H

#include <stdint.h>

/*
 * 编译开关说明：将对应开关设为1以启用新模块，设为0以保持原有行为
 *
 * USE_NEW_COORD       - 坐标转换模块（WGS84→GCJ02）
 * USE_NEW_GPS_PARSER  - GPS NMEA解析模块
 * USE_NEW_JSON_BUILDER- JSON构建模块（用于MQTT上报）
 * USE_NEW_TIMER       - 定时器管理模块
 * USE_NEW_ERROR       - 错误处理与恢复模块
 * USE_NEW_LED_STATUS  - LED状态指示模块
 * USE_NEW_MQTT        - MQTT通信封装模块
 * USE_NEW_APP         - 应用主逻辑模块（需验证后才开启）
 * NG_USE_DHT11        - DHT11温湿度传感器采集开关
 */
#define USE_NEW_COORD       1   /* 1=新坐标模块, 0=原有行为 */
#define USE_NEW_GPS_PARSER  1   /* 1=新GPS解析,   0=原有行为 */
#define USE_NEW_JSON_BUILDER 1  /* 1=新JSON构建,  0=原有行为 */
#define USE_NEW_TIMER       1   /* 1=新定时器,    0=原有行为 */
#define USE_NEW_ERROR       1   /* 1=新错误处理,  0=原有行为 */
#define USE_NEW_LED_STATUS  1   /* 1=新LED状态,   0=原有行为 */
#define USE_NEW_MQTT        1   /* 1=新MQTT封装,  0=原有行为 */
#define USE_NEW_APP         0   /* 1=新应用逻辑,  0=原有行为（需验证后才开启） */

/* ====== 算法参数 ====== */
/*
 * 浮点精度说明：
 * float = 1: 性能优先（Cortex-M3无FPU，float比double快3倍）
 *            精度约7位有效数字，GPS坐标可能损失约10米精度
 * float = 0: 精度优先（使用double，软件浮点运算较慢）
 *            精度约15位有效数字，满足GPS 6位小数需求
 * 建议：开发调试时用double(0)，部署时用float(1)
 */
#define NG_COORD_USE_FLOAT  1   /* 1=用float(快), 0=用double(精确) */

/* ====== GPS配置 ====== */
#define NG_GPS_MAX_FAIL      5
#define NG_GPS_WAIT_MS       500
#define NG_GPS_PARSE_BUF_SIZE 128

/* ====== 通信配置 ====== */
#define NG_AT_TIMEOUT_MS     3000
#define NG_AT_MAX_RETRY      3
#define NG_MQTT_MAX_FAIL     3

/* ====== 传感器配置 ====== */
#define NG_USE_DHT11        1   /* 1=启用DHT11温湿度采集, 0=跳过 */
#define NG_DHT11_INTERVAL_MS 5000

/* ====== 看门狗配置 ====== */
#define NG_WDG_PRESCALE       6     /* IWDG预分频（64分频） */
#define NG_WDG_RELOAD         500   /* IWDG重装值 */
/* 超时约4秒: 40kHz / 64 / 500 */

/* ====== 错误恢复配置 ====== */
#define NG_ERROR_RECOVERY_DELAY_MS  2000  /* 恢复前延时(ms) */
#define NG_ERROR_RECOVERY_MAX       5     /* 最大连续恢复次数 */

/* ====== 主循环配置 ====== */
#define NG_LOOP_INTERVAL_MS  1000

/* ====== 版本号 ====== */
#define NG_FW_VERSION_MAJOR  1
#define NG_FW_VERSION_MINOR  0
#define NG_FW_VERSION_PATCH  0

#endif /* __NG_CONFIG_H */
