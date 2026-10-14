/**
 * @file    test_gps_parser.c
 * @brief   GPS解析模块单元测试
 * @note    测试BC20模块AT+QGPSLOC响应解析的正确性
 */

#include <stdio.h>
#include <string.h>
#include "ng_gps_parser.h"

/* 测试统计 */
static int gps_pass = 0;
static int gps_fail = 0;

/* 测试宏 */
#define GPS_TEST(name, expr) do { \
    if (expr) { \
        printf("  [PASS] %s\n", name); \
        gps_pass++; \
    } else { \
        printf("  [FAIL] %s\n", name); \
        gps_fail++; \
    } \
} while(0)

/**
 * @brief  测试有效GPS响应解析
 */
static void test_valid_response(void)
{
    ng_gps_data_t data;
    int ret;

    /* 模拟BC20模块有效响应 */
    const char *response = "+QGPSLOC: 081456.0,31.234567,121.456789,10.5,25.3,0,12,8.5";

    ret = ng_gps_parse(response, &data);

    GPS_TEST("有效响应-返回NG_GPS_OK", ret == NG_GPS_OK);
    GPS_TEST("有效响应-valid标志为1", data.valid == 1);
    GPS_TEST("有效响应-纬度正确(31.234567)",
             data.lat > 31.23f && data.lat < 31.24f);
    GPS_TEST("有效响应-经度正确(121.456789)",
             data.lon > 121.45f && data.lon < 121.46f);
    GPS_TEST("有效响应-海拔正确(25.3)",
             data.alt > 25.0f && data.alt < 26.0f);
}

/**
 * @brief  测试无定位响应
 */
static void test_no_fix(void)
{
    ng_gps_data_t data;
    int ret;

    /* 无+QGPSLOC:前缀的响应 */
    const char *response = "+CME ERROR: 516";

    ret = ng_gps_parse(response, &data);

    GPS_TEST("无定位-返回NG_GPS_ERR_FORMAT", ret == NG_GPS_ERR_FORMAT);
    GPS_TEST("无定位-valid标志为0", data.valid == 0);
}

/**
 * @brief  测试格式错误响应
 */
static void test_format_error(void)
{
    ng_gps_data_t data;
    int ret;

    /* 有前缀但格式错误 */
    const char *response = "+QGPSLOC: invalid_data";

    ret = ng_gps_parse(response, &data);

    GPS_TEST("格式错误-返回NG_GPS_ERR_PARSE", ret == NG_GPS_ERR_PARSE);
}

/**
 * @brief  测试空字符串和NULL指针
 */
static void test_null_empty(void)
{
    ng_gps_data_t data;
    int ret;

    /* NULL响应字符串 */
    ret = ng_gps_parse(NULL, &data);
    GPS_TEST("NULL字符串-返回NG_GPS_ERR_NULL", ret == NG_GPS_ERR_NULL);

    /* NULL输出指针 */
    ret = ng_gps_parse("+QGPSLOC: 081456.0,31.23,121.45,10.5,25.3,0,12,8.5", NULL);
    GPS_TEST("NULL输出指针-返回NG_GPS_ERR_NULL", ret == NG_GPS_ERR_NULL);

    /* 空字符串 */
    ret = ng_gps_parse("", &data);
    GPS_TEST("空字符串-返回NG_GPS_ERR_FORMAT", ret == NG_GPS_ERR_FORMAT);
}

/**
 * @brief  测试GPS数据有效性检查
 */
static void test_is_valid(void)
{
    ng_gps_data_t data;

    /* 有效数据 */
    memset(&data, 0, sizeof(data));
    data.lat = 31.23f;
    data.lon = 121.45f;
    data.valid = 1;
    GPS_TEST("有效数据-is_valid返回1", ng_gps_is_valid(&data) == 1);

    /* 无效数据：valid=0 */
    data.valid = 0;
    GPS_TEST("无效标志-is_valid返回0", ng_gps_is_valid(&data) == 0);

    /* 无效数据：坐标全0 */
    data.valid = 1;
    data.lat = 0.0f;
    data.lon = 0.0f;
    GPS_TEST("零坐标-is_valid返回0", ng_gps_is_valid(&data) == 0);

    /* NULL指针 */
    GPS_TEST("NULL指针-is_valid返回0", ng_gps_is_valid(NULL) == 0);
}

/* 运行所有GPS解析测试 */
int test_gps_parser_run(void)
{
    printf("\n====== GPS解析模块测试 ======\n");

    test_valid_response();
    test_no_fix();
    test_format_error();
    test_null_empty();
    test_is_valid();

    printf("GPS解析测试: 通过 %d, 失败 %d\n", gps_pass, gps_fail);
    return gps_fail;
}
