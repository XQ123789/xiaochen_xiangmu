/**
 * @file    ng_json_builder.c
 * @brief   JSON构建实现 - OneNET数据上报
 * @note    改进自 stm32GPS定位模块/HARDWARE/4G/4G.c 第182-205行
 *
 * 改进内容：
 *          - 使用snprintf替代sprintf，防止缓冲区溢出
 *          - 检查buf_size是否足够
 *          - 返回错误码而非仅返回长度
 *          - JSON格式与原MakeOnenetPayload()完全兼容
 */

#include "ng_json_builder.h"
#include <stdio.h>   /* snprintf */
#include <string.h>  /* strlen */

/**
 * @brief  构建OneNET数据上报JSON
 *
 * @note   原代码：
 *         sprintf(JsonBuf,
 *                 "{\"id\":\"123\",\"params\":{"
 *                 "\"temp\":{\"value\":%d},"
 *                 "\"humi\":{\"value\":%d},"
 *                 "\"latitude\":{\"value\":%.6f},"
 *                 "\"longitude\":{\"value\":%.6f},"
 *                 "\"altitude\":{\"value\":%.2f},"
 *                 "\"speed\":{\"value\":%.2f},"
 *                 "\"direction\":{\"value\":%.1f}"
 *                 "}}",
 *                 Temp, Humi, Latitude, Longitude, Altitude, Speed, Direction);
 *
 *         改进：使用snprintf，添加缓冲区大小检查
 */
int ng_json_build_onenet(char *buf, int buf_size,
                          uint8_t temp, uint8_t humi,
                          float lat, float lon, float alt,
                          float speed, float direction)
{
    int written;
    int needed;

    /* 空指针检查 */
    if (buf == NULL) {
        return NG_JSON_ERR_NULL;
    }

    /* 缓冲区大小检查（至少需要容纳最小JSON） */
    if (buf_size <= 0) {
        return NG_JSON_ERR_SIZE;
    }

    /*
     * 使用snprintf安全格式化
     * JSON格式与OneNET数据点格式兼容
     * 字段：温度、湿度、纬度、经度、海拔、速度、方向
     */
    written = snprintf(buf, (size_t)buf_size,
            "{\"id\":\"123\",\"params\":{"
            "\"temp\":{\"value\":%d},"
            "\"humi\":{\"value\":%d},"
            "\"latitude\":{\"value\":%.6f},"
            "\"longitude\":{\"value\":%.6f},"
            "\"altitude\":{\"value\":%.2f},"
            "\"speed\":{\"value\":%.2f},"
            "\"direction\":{\"value\":%.1f}"
            "}}",
            temp, humi, lat, lon, alt, speed, direction);

    /* 检查snprintf返回值 */
    if (written < 0) {
        /* 格式化失败 */
        return NG_JSON_ERR_FMT;
    }

    /* 检查是否因缓冲区太小而被截断 */
    needed = written;
    if (needed >= buf_size) {
        /* 缓冲区太小，JSON被截断 */
        buf[0] = '\0';  /* 清空缓冲区，避免不完整JSON */
        return NG_JSON_ERR_SIZE;
    }

    return NG_JSON_OK;
}
