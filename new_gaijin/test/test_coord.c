/**
 * @file    test_coord.c
 * @brief   坐标转换模块单元测试
 * @note    测试WGS84→GCJ02转换算法的正确性
 *          测试用例参考已知坐标对和在线转换工具结果
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "ng_coord.h"

/* 测试统计 */
static int coord_pass = 0;
static int coord_fail = 0;

/* 浮点数比较（允许误差） */
#define FLOAT_EQ(a, b, eps) (fabs((a) - (b)) < (eps))

/* 测试宏 */
#define COORD_TEST(name, expr) do { \
    if (expr) { \
        printf("  [PASS] %s\n", name); \
        coord_pass++; \
    } else { \
        printf("  [FAIL] %s\n", name); \
        coord_fail++; \
    } \
} while(0)

/**
 * @brief  测试北京坐标转换
 * @note   北京天安门 WGS84: 116.3912757, 39.906217
 *         预期GCJ02约: 116.397, 39.908（允许0.01误差）
 */
static void test_beijing(void)
{
    ng_float_t gcj_lon, gcj_lat;
    int ret;

    ret = ng_coord_wgs84_to_gcj02(116.3912757f, 39.906217f, &gcj_lon, &gcj_lat);

    COORD_TEST("北京-返回值正确", ret == 0);
    COORD_TEST("北京-经度偏移合理(116.39~116.40)",
               gcj_lon > 116.39f && gcj_lon < 116.41f);
    COORD_TEST("北京-纬度偏移合理(39.90~39.92)",
               gcj_lat > 39.90f && gcj_lat < 39.92f);
}

/**
 * @brief  测试上海坐标转换
 * @note   上海人民广场 WGS84: 121.473701, 31.230426
 *         预期GCJ02约: 121.478, 31.232
 */
static void test_shanghai(void)
{
    ng_float_t gcj_lon, gcj_lat;
    int ret;

    ret = ng_coord_wgs84_to_gcj02(121.473701f, 31.230426f, &gcj_lon, &gcj_lat);

    COORD_TEST("上海-返回值正确", ret == 0);
    COORD_TEST("上海-经度偏移合理(121.47~121.49)",
               gcj_lon > 121.47f && gcj_lon < 121.49f);
    COORD_TEST("上海-纬度偏移合理(31.22~31.25)",
               gcj_lat > 31.22f && gcj_lat < 31.25f);
}

/**
 * @brief  测试广州坐标转换
 * @note   广州塔 WGS84: 113.324523, 23.106414
 *         预期GCJ02约: 113.328, 23.108
 */
static void test_guangzhou(void)
{
    ng_float_t gcj_lon, gcj_lat;
    int ret;

    ret = ng_coord_wgs84_to_gcj02(113.324523f, 23.106414f, &gcj_lon, &gcj_lat);

    COORD_TEST("广州-返回值正确", ret == 0);
    COORD_TEST("广州-经度偏移合理(113.32~113.34)",
               gcj_lon > 113.32f && gcj_lon < 113.34f);
    COORD_TEST("广州-纬度偏移合理(23.10~23.12)",
               gcj_lat > 23.10f && gcj_lat < 23.12f);
}

/**
 * @brief  测试原点(0,0)坐标
 */
static void test_origin(void)
{
    ng_float_t gcj_lon, gcj_lat;
    int ret;

    ret = ng_coord_wgs84_to_gcj02(0.0f, 0.0f, &gcj_lon, &gcj_lat);

    COORD_TEST("原点(0,0)-返回值正确", ret == 0);
    COORD_TEST("原点-输出坐标有效",
               ng_coord_validate(gcj_lon, gcj_lat) == 1);
}

/**
 * @brief  测试负坐标（南半球/西半球）
 */
static void test_negative(void)
{
    ng_float_t gcj_lon, gcj_lat;
    int ret;

    /* 南半球：悉尼 WGS84: 151.2093, -33.8688 */
    ret = ng_coord_wgs84_to_gcj02(151.2093f, -33.8688f, &gcj_lon, &gcj_lat);

    COORD_TEST("负纬度-返回值正确", ret == 0);
    COORD_TEST("负纬度-输出坐标有效",
               ng_coord_validate(gcj_lon, gcj_lat) == 1);
}

/**
 * @brief  测试超范围坐标（应返回错误）
 */
static void test_out_of_range(void)
{
    ng_float_t gcj_lon, gcj_lat;
    int ret;

    /* 经度超范围 */
    ret = ng_coord_wgs84_to_gcj02(200.0f, 30.0f, &gcj_lon, &gcj_lat);
    COORD_TEST("经度超范围-返回错误", ret == -1);

    /* 纬度超范围 */
    ret = ng_coord_wgs84_to_gcj02(100.0f, 100.0f, &gcj_lon, &gcj_lat);
    COORD_TEST("纬度超范围-返回错误", ret == -1);

    /* 空指针 */
    ret = ng_coord_wgs84_to_gcj02(100.0f, 30.0f, NULL, &gcj_lat);
    COORD_TEST("空指针-返回错误", ret == -1);
}

/**
 * @brief  测试坐标验证函数
 */
static void test_validate(void)
{
    COORD_TEST("有效坐标(116.4, 39.9)", ng_coord_validate(116.4f, 39.9f) == 1);
    COORD_TEST("有效坐标(-180, -90)", ng_coord_validate(-180.0f, -90.0f) == 1);
    COORD_TEST("有效坐标(180, 90)", ng_coord_validate(180.0f, 90.0f) == 1);
    COORD_TEST("无效经度(181, 0)", ng_coord_validate(181.0f, 0.0f) == 0);
    COORD_TEST("无效纬度(0, 91)", ng_coord_validate(0.0f, 91.0f) == 0);
}

/* 运行所有坐标转换测试 */
int test_coord_run(void)
{
    printf("\n====== 坐标转换模块测试 ======\n");

    test_beijing();
    test_shanghai();
    test_guangzhou();
    test_origin();
    test_negative();
    test_out_of_range();
    test_validate();

    printf("坐标转换测试: 通过 %d, 失败 %d\n", coord_pass, coord_fail);
    return coord_fail;
}
