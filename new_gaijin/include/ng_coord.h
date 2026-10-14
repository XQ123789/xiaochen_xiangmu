/**
 * @file    ng_coord.h
 * @brief   坐标转换模块 - WGS84转GCJ02
 * @note    改进自 stm32GPS定位模块/HARDWARE/LED/led.c 第58-118行
 *          移除GPIO硬件依赖，提取为纯算法独立模块
 */

#ifndef __NG_COORD_H
#define __NG_COORD_H

#include "ng_config.h"

/* 根据配置选择浮点类型：float速度快，double精度高 */
#if NG_COORD_USE_FLOAT
typedef float ng_float_t;
#else
typedef double ng_float_t;
#endif

/* 地理坐标结构体 */
typedef struct {
    ng_float_t lon;  /* 经度 */
    ng_float_t lat;  /* 纬度 */
} ng_geo_t;

/**
 * @brief  WGS84坐标转换为GCJ02坐标（火星坐标）
 * @param  wgs_lon WGS84经度
 * @param  wgs_lat WGS84纬度
 * @param  gcj_lon 输出GCJ02经度指针
 * @param  gcj_lat 输出GCJ02纬度指针
 * @return 0=成功, -1=参数错误
 *
 * @note   算法原理：迭代近似法
 *         1. 计算当前坐标相对于参考点(105°E, 35°N)的偏移量
 *         2. 利用椭球参数将偏移量转换为角度增量
 *         3. 迭代3次逐步逼近真实GCJ02坐标
 *         4. 最后加上固定偏移量(东偏0.000365°, 北偏0.000275°)
 */
int ng_coord_wgs84_to_gcj02(ng_float_t wgs_lon, ng_float_t wgs_lat,
                             ng_float_t *gcj_lon, ng_float_t *gcj_lat);

/**
 * @brief  验证坐标是否在有效范围内
 * @param  lon 经度 (-180 ~ 180)
 * @param  lat 纬度 (-90 ~ 90)
 * @return 1=有效, 0=无效
 */
int ng_coord_validate(ng_float_t lon, ng_float_t lat);

#endif /* __NG_COORD_H */
