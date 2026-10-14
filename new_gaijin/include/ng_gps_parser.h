/**
 * @file    ng_gps_parser.h
 * @brief   GPS数据解析模块 - BC20模块AT+QGPSLOC响应解析
 * @note    改进自 stm32GPS定位模块/USER/main.c 第49-67行
 *          原代码问题：
 *          1. 解析逻辑与UART中断处理耦合
 *          2. 无错误码返回，只打印信息
 *          3. 缺少格式校验和范围检查
 */

#ifndef __NG_GPS_PARSER_H
#define __NG_GPS_PARSER_H

#include "ng_config.h"
#include <stdint.h>

/* GPS解析错误码 */
#define NG_GPS_OK           0   /* 解析成功 */
#define NG_GPS_ERR_NULL    -1   /* 输入指针为空 */
#define NG_GPS_ERR_FORMAT  -2   /* 格式不匹配（无+QGPSLOC:前缀） */
#define NG_GPS_ERR_PARSE   -3   /* sscanf解析失败 */
#define NG_GPS_ERR_RANGE   -4   /* 坐标超出范围 */

/* GPS数据结构 */
typedef struct {
    float lat;          /* 纬度 */
    float lon;          /* 经度 */
    float alt;          /* 海拔(米) */
    float speed;        /* 速度(km/h) */
    float direction;    /* 方向(度) */
    uint8_t valid;      /* 0=无效, 1=有效定位 */
    uint8_t reserved;   /* 保留对齐 */
} ng_gps_data_t;

/**
 * @brief  解析BC20模块AT+QGPSLOC响应
 * @param  response AT响应字符串
 * @param  out      输出GPS数据指针
 * @return NG_GPS_OK=成功, 其他=错误码
 *
 * @note   响应格式示例：
 *         +QGPSLOC: 081456.0,31.234567,121.456789,10.5,0.0,0,12,8.5
 *         字段含义：UTC时间,纬度,经度,精度,海拔,日期,卫星数,速度...
 */
int ng_gps_parse(const char *response, ng_gps_data_t *out);

/**
 * @brief  检查GPS数据是否有效
 * @param  data GPS数据指针
 * @return 1=有效, 0=无效
 */
int ng_gps_is_valid(const ng_gps_data_t *data);

#endif /* __NG_GPS_PARSER_H */
