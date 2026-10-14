/**
 * @file    ng_json_builder.h
 * @brief   JSON构建模块 - OneNET数据上报
 * @note    改进自 stm32GPS定位模块/HARDWARE/4G/4G.c 第182-205行
 *          原代码问题：
 *          1. 使用sprintf，存在缓冲区溢出风险
 *          2. 固定256字节缓冲区，无法适配不同场景
 *          3. 返回值仅为长度，无错误处理
 */

#ifndef __NG_JSON_BUILDER_H
#define __NG_JSON_BUILDER_H

#include "ng_config.h"
#include <stdint.h>

/* JSON构建错误码 */
#define NG_JSON_OK        0   /* 构建成功 */
#define NG_JSON_ERR_NULL -1   /* 输入指针为空 */
#define NG_JSON_ERR_SIZE -2   /* 缓冲区太小 */
#define NG_JSON_ERR_FMT  -3   /* 格式化失败 */

/**
 * @brief  构建OneNET数据上报JSON
 * @param  buf       输出缓冲区
 * @param  buf_size  缓冲区大小
 * @param  temp      温度
 * @param  humi      湿度
 * @param  lat       纬度
 * @param  lon       经度
 * @param  alt       海拔
 * @param  speed     速度
 * @param  direction 方向
 * @return NG_JSON_OK=成功, 其他=错误码
 *
 * @note   JSON格式与原 MakeOnenetPayload() 兼容：
 *         {"id":"123","params":{"temp":{"value":25},"humi":{"value":60},...}}
 */
int ng_json_build_onenet(char *buf, int buf_size,
                          uint8_t temp, uint8_t humi,
                          float lat, float lon, float alt,
                          float speed, float direction);

#endif /* __NG_JSON_BUILDER_H */
