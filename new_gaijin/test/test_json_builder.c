/**
 * @file    test_json_builder.c
 * @brief   JSON构建模块单元测试
 * @note    测试OneNET数据上报JSON构建的正确性
 */

#include <stdio.h>
#include <string.h>
#include "ng_json_builder.h"

/* 测试统计 */
static int json_pass = 0;
static int json_fail = 0;

/* 测试宏 */
#define JSON_TEST(name, expr) do { \
    if (expr) { \
        printf("  [PASS] %s\n", name); \
        json_pass++; \
    } else { \
        printf("  [FAIL] %s\n", name); \
        json_fail++; \
    } \
} while(0)

/**
 * @brief  测试正常值JSON输出
 */
static void test_normal_values(void)
{
    char buf[256];
    int ret;

    ret = ng_json_build_onenet(buf, sizeof(buf),
                                25, 60,
                                31.234567f, 121.456789f, 25.3f,
                                8.5f, 180.0f);

    JSON_TEST("正常值-返回NG_JSON_OK", ret == NG_JSON_OK);
    JSON_TEST("正常值-包含temp字段", strstr(buf, "\"temp\":{\"value\":25}") != NULL);
    JSON_TEST("正常值-包含humi字段", strstr(buf, "\"humi\":{\"value\":60}") != NULL);
    JSON_TEST("正常值-包含latitude字段", strstr(buf, "\"latitude\":") != NULL);
    JSON_TEST("正常值-包含longitude字段", strstr(buf, "\"longitude\":") != NULL);
    JSON_TEST("正常值-JSON开头正确", strncmp(buf, "{\"id\":\"123\"", 11) == 0);
    JSON_TEST("正常值-JSON结尾正确",
              buf[strlen(buf)-1] == '}' && buf[strlen(buf)-2] == '}');
}

/**
 * @brief  测试极值
 */
static void test_extreme_values(void)
{
    char buf[256];
    int ret;

    /* 温度和湿度极值 */
    ret = ng_json_build_onenet(buf, sizeof(buf),
                                0, 100,
                                -90.0f, -180.0f, -100.0f,
                                0.0f, 0.0f);
    JSON_TEST("极值(0,100,-90,-180)-返回NG_JSON_OK", ret == NG_JSON_OK);

    ret = ng_json_build_onenet(buf, sizeof(buf),
                                255, 255,
                                90.0f, 180.0f, 10000.0f,
                                999.9f, 359.9f);
    JSON_TEST("极值(255,255,90,180)-返回NG_JSON_OK", ret == NG_JSON_OK);
}

/**
 * @brief  测试缓冲区溢出
 */
static void test_buffer_overflow(void)
{
    char buf[10];  /* 故意给很小的缓冲区 */
    int ret;

    ret = ng_json_build_onenet(buf, sizeof(buf),
                                25, 60,
                                31.234567f, 121.456789f, 25.3f,
                                8.5f, 180.0f);

    JSON_TEST("小缓冲区-返回NG_JSON_ERR_SIZE", ret == NG_JSON_ERR_SIZE);
}

/**
 * @brief  测试NULL指针
 */
static void test_null_ptr(void)
{
    int ret;

    ret = ng_json_build_onenet(NULL, 256, 25, 60, 31.0f, 121.0f, 10.0f, 5.0f, 90.0f);
    JSON_TEST("NULL缓冲区-返回NG_JSON_ERR_NULL", ret == NG_JSON_ERR_NULL);
}

/**
 * @brief  测试JSON格式合法性
 */
static void test_json_format(void)
{
    char buf[256];
    int ret;
    int brace_count = 0;
    size_t i;

    ret = ng_json_build_onenet(buf, sizeof(buf),
                                25, 60,
                                31.234567f, 121.456789f, 25.3f,
                                8.5f, 180.0f);

    if (ret != NG_JSON_OK) {
        JSON_TEST("JSON格式-构建成功", 0);
        return;
    }

    /* 检查大括号匹配 */
    for (i = 0; i < strlen(buf); i++) {
        if (buf[i] == '{') brace_count++;
        if (buf[i] == '}') brace_count--;
    }
    JSON_TEST("JSON格式-大括号匹配", brace_count == 0);

    /* 检查不以空字符串结束 */
    JSON_TEST("JSON格式-非空输出", strlen(buf) > 0);
}

/* 运行所有JSON构建测试 */
int test_json_builder_run(void)
{
    printf("\n====== JSON构建模块测试 ======\n");

    test_normal_values();
    test_extreme_values();
    test_buffer_overflow();
    test_null_ptr();
    test_json_format();

    printf("JSON构建测试: 通过 %d, 失败 %d\n", json_pass, json_fail);
    return json_fail;
}
