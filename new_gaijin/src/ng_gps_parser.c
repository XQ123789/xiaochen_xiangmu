/**
 * @file    ng_gps_parser.c
 * @brief   GPS数据解析实现 - BC20模块AT+QGPSLOC响应解析
 * @note    改进自 stm32GPS定位模块/USER/main.c 第49-67行
 *
 * 改进内容：
 *          - 独立解析函数，不依赖UART硬件
 *          - 返回详细错误码，便于上层处理
 *          - 添加坐标范围校验
 *          - 处理4种情况：有效响应/无定位/格式错误/空缓冲区
 */

#include "ng_gps_parser.h"
#include <stdio.h>   /* sscanf */
#include <string.h>  /* strstr */

/* +QGPSLOC: 前缀字符串 */
#define GPS_PREFIX "+QGPSLOC:"
#define GPS_PREFIX_LEN 9  /* strlen("+QGPSLOC:") */

/**
 * @brief  解析BC20模块AT+QGPSLOC响应
 *
 * @note   原代码解析逻辑：
 *         sscanf(gps_start + 9, "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
 *                &lat, &lon, &alt, &speed, &dir)
 *         字段对应：跳过UTC时间, 纬度, 经度, 跳过精度, 海拔, 跳过日期, 速度, 方向
 *
 *         改进：使用float替代double，添加范围校验
 */
int ng_gps_parse(const char *response, ng_gps_data_t *out)
{
    const char *gps_start;
    double lat_d, lon_d, alt_d, speed_d, dir_d;
    int parsed;

    /* 空指针检查 */
    if (response == NULL || out == NULL) {
        return NG_GPS_ERR_NULL;
    }

    /* 初始化输出结构 */
    out->lat = 0.0f;
    out->lon = 0.0f;
    out->alt = 0.0f;
    out->speed = 0.0f;
    out->direction = 0.0f;
    out->valid = 0;

    /* 查找+QGPSLOC:前缀 */
    gps_start = strstr(response, GPS_PREFIX);
    if (gps_start == NULL) {
        /* 未找到GPS前缀，可能是无定位或格式错误 */
        return NG_GPS_ERR_FORMAT;
    }

    /* 跳过前缀，解析逗号分隔的字段 */
    /* 格式：+QGPSLOC: UTC时间,纬度,经度,精度,海拔,日期,卫星数,速度,方向... */
    /* %*[^,] 跳过UTC时间字段，%lf 读取纬度/经度/海拔等 */
    parsed = sscanf(gps_start + GPS_PREFIX_LEN,
                    "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
                    &lat_d, &lon_d, &alt_d, &speed_d, &dir_d);

    /* 至少需要解析出纬度和经度（2个字段） */
    if (parsed < 2) {
        return NG_GPS_ERR_PARSE;
    }

    /* 坐标范围校验 */
    if (lat_d < -90.0 || lat_d > 90.0 ||
        lon_d < -180.0 || lon_d > 180.0) {
        return NG_GPS_ERR_RANGE;
    }

    /* 填充输出结构 */
    out->lat = (float)lat_d;
    out->lon = (float)lon_d;
    out->alt = (parsed >= 3) ? (float)alt_d : 0.0f;
    out->speed = (parsed >= 4) ? (float)speed_d : 0.0f;
    out->direction = (parsed >= 5) ? (float)dir_d : 0.0f;
    out->valid = 1;  /* 标记为有效定位 */

    return NG_GPS_OK;
}

/**
 * @brief  检查GPS数据是否有效
 * @param  data GPS数据指针
 * @return 1=有效, 0=无效
 */
int ng_gps_is_valid(const ng_gps_data_t *data)
{
    if (data == NULL) {
        return 0;
    }

    /* valid标志必须为1 */
    if (data->valid != 1) {
        return 0;
    }

    /* 坐标不能全为0（可能是未定位的默认值） */
    if (data->lat == 0.0f && data->lon == 0.0f) {
        return 0;
    }

    /* 坐标范围校验 */
    if (data->lat < -90.0f || data->lat > 90.0f) {
        return 0;
    }
    if (data->lon < -180.0f || data->lon > 180.0f) {
        return 0;
    }

    return 1;
}
