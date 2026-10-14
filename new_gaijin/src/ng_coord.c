/**
 * @file    ng_coord.c
 * @brief   坐标转换实现 - WGS84转GCJ02（火星坐标）
 * @note    改进自 stm32GPS定位模块/HARDWARE/LED/led.c 第58-118行
 *          原代码问题：
 *          1. 与GPIO/LED硬件耦合在同一个文件
 *          2. 使用硬编码double，无法选择float
 *          3. 缺少参数校验和错误返回
 *          4. 注释使用乱码编码
 *
 * 改进内容：
 *          - 移除所有硬件依赖，纯算法模块
 *          - 使用ng_float_t类型，支持float/double切换
 *          - 添加输入参数校验
 *          - 添加完整中文注释说明算法原理
 */

#include "ng_coord.h"
#include <math.h>
#include <stdlib.h>  /* fabs(), sqrt() */

/* ====== 椭球参数定义 ====== */
/* 圆周率 */
#define NG_PI        ((ng_float_t)3.1415926535897932384626)
/* WGS84椭球长半轴（米） */
#define NG_EARTH_A   ((ng_float_t)6378245.0)
/* WGS84椭球第一偏心率的平方 */
#define NG_EARTH_EE  ((ng_float_t)0.00669342162296594323)

/* ====== 固定偏移量 ====== */
/* 经度方向固定偏移（度），向东偏移约0.000365度 */
#define NG_EAST_OFFSET   ((ng_float_t)0.000365)
/* 纬度方向固定偏移（度），向北偏移约0.000275度 */
#define NG_NORTH_OFFSET  ((ng_float_t)0.000275)

/* 迭代次数，原代码为3次 */
#define NG_ITER_COUNT    3

/**
 * @brief  计算坐标偏移量（内部辅助函数）
 * @param  lon       当前经度
 * @param  lat       当前纬度
 * @param  delta_lon 输出经度偏移量
 * @param  delta_lat 输出纬度偏移量
 *
 * @note   算法原理：
 *         以(105°E, 35°N)为参考中心点，计算当前坐标与参考点的差值，
 *         通过多项式+三角函数组合拟合出偏移量。
 *         这是GCJ02加偏算法的经验公式。
 */
static void get_delta(ng_float_t lon, ng_float_t lat,
                      ng_float_t *delta_lon, ng_float_t *delta_lat)
{
    /* 计算相对于参考点(105°E, 35°N)的差值 */
    ng_float_t dLon = lon - (ng_float_t)105.0;
    ng_float_t dLat = lat - (ng_float_t)35.0;

    /* 经度方向偏移量 - 多项式+正弦函数拟合 */
    *delta_lon = (ng_float_t)300.0 + dLon + (ng_float_t)2.0 * dLat
        + (ng_float_t)0.1 * dLon * dLon
        + (ng_float_t)0.1 * dLon * dLat
        + (ng_float_t)0.1 * sqrt(fabs(dLon))
        + ((ng_float_t)20.0 * sin((ng_float_t)6.0 * NG_PI * dLon)
           + (ng_float_t)20.0 * sin((ng_float_t)2.0 * NG_PI * dLon)) * (ng_float_t)2.0 / (ng_float_t)3.0
        + ((ng_float_t)20.0 * sin(NG_PI * dLon)
           + (ng_float_t)40.0 * sin(NG_PI * dLon / (ng_float_t)3.0)) * (ng_float_t)2.0 / (ng_float_t)3.0
        + ((ng_float_t)150.0 * sin(NG_PI * dLon / (ng_float_t)12.0)
           + (ng_float_t)300.0 * sin(NG_PI * dLon / (ng_float_t)30.0)) * (ng_float_t)2.0 / (ng_float_t)3.0
        + (ng_float_t)0.002 * dLon * dLat;

    /* 纬度方向偏移量 - 多项式+正弦函数拟合 */
    *delta_lat = (ng_float_t)-100.0 + (ng_float_t)2.0 * dLon + (ng_float_t)3.0 * dLat
        + (ng_float_t)0.2 * dLat * dLat
        + (ng_float_t)0.1 * dLon * dLat
        + (ng_float_t)0.2 * sqrt(fabs(dLon))
        + ((ng_float_t)20.0 * sin((ng_float_t)6.0 * NG_PI * dLon)
           + (ng_float_t)20.0 * sin((ng_float_t)2.0 * NG_PI * dLat)) * (ng_float_t)2.0 / (ng_float_t)3.0
        + ((ng_float_t)20.0 * sin(NG_PI * dLat)
           + (ng_float_t)40.0 * sin(NG_PI * dLat / (ng_float_t)3.0)) * (ng_float_t)2.0 / (ng_float_t)3.0
        + ((ng_float_t)160.0 * sin(NG_PI * dLat / (ng_float_t)12.0)
           + (ng_float_t)320.0 * sin(NG_PI * dLat / (ng_float_t)30.0)) * (ng_float_t)2.0 / (ng_float_t)3.0
        + (ng_float_t)0.001 * dLat * dLat;
}

/**
 * @brief  WGS84坐标转换为GCJ02坐标
 *
 * @note   算法流程：
 *         1. 以WGS84坐标为初始值
 *         2. 计算当前坐标的偏移量delta
 *         3. 利用椭球参数将偏移量转换为角度增量dlon/dlat
 *         4. 按衰减系数(0.5/(i+1))累加增量，迭代逼近
 *         5. 最后加上固定偏移量得到最终GCJ02坐标
 */
int ng_coord_wgs84_to_gcj02(ng_float_t wgs_lon, ng_float_t wgs_lat,
                             ng_float_t *gcj_lon, ng_float_t *gcj_lat)
{
    ng_float_t lon, lat;
    ng_float_t delta_lon, delta_lat;
    ng_float_t rad_lat, magic, sqrt_magic;
    ng_float_t dlon, dlat;
    int i;

    /* 参数校验：输出指针不能为空 */
    if (gcj_lon == NULL || gcj_lat == NULL) {
        return -1;
    }

    /* 输入坐标范围校验 */
    if (wgs_lon < (ng_float_t)-180.0 || wgs_lon > (ng_float_t)180.0 ||
        wgs_lat < (ng_float_t)-90.0  || wgs_lat > (ng_float_t)90.0) {
        return -1;
    }

    /* 以WGS84坐标为迭代初始值 */
    lon = wgs_lon;
    lat = wgs_lat;

    /* 迭代逼近，共NG_ITER_COUNT次 */
    for (i = 0; i < NG_ITER_COUNT; i++) {
        /* 纬度转弧度 */
        rad_lat = lat * NG_PI / (ng_float_t)180.0;

        /* 计算椭球曲率相关参数 */
        magic = (ng_float_t)1.0 - NG_EARTH_EE * sin(rad_lat) * sin(rad_lat);
        sqrt_magic = sqrt(magic);

        /* 计算当前坐标的偏移量 */
        get_delta(lon, lat, &delta_lon, &delta_lat);

        /* 将偏移量转换为角度增量 */
        /* dlon: 经度方向增量 = 偏移量 / (曲率半径 * cos(纬度) * PI) * 180 */
        dlon = delta_lon * (ng_float_t)180.0
             / (NG_EARTH_A / sqrt_magic * cos(rad_lat) * NG_PI);
        /* dlat: 纬度方向增量 = 偏移量 / (子午圈曲率半径 * PI) * 180 */
        dlat = delta_lat * (ng_float_t)180.0
             / ((NG_EARTH_A * ((ng_float_t)1.0 - NG_EARTH_EE))
                / (magic * sqrt_magic) * NG_PI);

        /* 按衰减系数累加，逐步逼近真实值 */
        lon += dlon * ((ng_float_t)0.5 / (ng_float_t)(i + 1));
        lat += dlat * ((ng_float_t)0.5 / (ng_float_t)(i + 1));
    }

    /* 加上固定偏移量得到最终GCJ02坐标 */
    *gcj_lon = lon + NG_EAST_OFFSET;
    *gcj_lat = lat + NG_NORTH_OFFSET;

    return 0;
}

/**
 * @brief  验证坐标是否在有效范围内
 * @param  lon 经度 (-180 ~ 180)
 * @param  lat 纬度 (-90 ~ 90)
 * @return 1=有效, 0=无效
 */
int ng_coord_validate(ng_float_t lon, ng_float_t lat)
{
    if (lon < (ng_float_t)-180.0 || lon > (ng_float_t)180.0) {
        return 0;
    }
    if (lat < (ng_float_t)-90.0 || lat > (ng_float_t)90.0) {
        return 0;
    }
    return 1;
}
