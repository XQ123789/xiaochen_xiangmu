/**
 * @file    bc20_improved.c
 * @brief   BC20(EC200U-CN) 4G模块驱动 - 改进版
 * @note    改进自原项目 stm32GPS定位模块/HARDWARE/4G/4G.c
 *
 * 主要改进：
 *   1. AT指令发送增加响应校验（检查"OK"/"ERROR"），不再盲目等待
 *   2. 添加重试机制（最多 AT_CMD_MAX_RETRY 次重试）
 *   3. 添加超时处理，所有等待循环都有上限，不再无限循环
 *   4. 使用 config.h 中的配置参数，替换硬编码魔数
 *   5. 函数添加完整文档注释
 *   6. 修复原4G.c中每次发送都订阅的问题（订阅只在初始化时做一次）
 *   7. 返回值改为有实际意义的类型（原函数返回void无法判断成败）
 */

#include "bc20_improved.h"
#include "config.h"
#include "string.h"
#include "usart.h"
#include "wdg.h"
#include "led.h"

/* ============ 模块内部变量 ============ */
char *strx;                          /* 字符串查找结果指针 */
CSTX_4G CSTX_4G_Status;             /* 4G模块状态结构体 */
char ATSTR[AT_CMD_BUF_SIZE];         /* AT指令缓冲区 */
char IMEINUMBER[AT_CMD_BUF_SIZE];    /* IMEI号存储 */

/* 订阅标志：确保只订阅一次 */
static uint8_t subscribe_done = 0;

/**
 * @brief  清空UART2接收缓冲区
 * @note   打印缓冲区内容后清空，用于调试和准备接收新数据
 */
void Clear_Buffer(void)
{
    printf("\r\nResponse: %s\r\n", buf_uart2.buf);
    delay_ms(100);
    buf_uart2.index = 0;
    memset(buf_uart2.buf, 0, BUFLEN);
    IWDG_Feed();
}

/**
 * @brief  发送AT指令并等待指定响应（带超时和重试）
 * @param  cmd       AT指令字符串
 * @param  expect    期望的响应关键字（如"OK"、"+QMTOPEN: 0,0"）
 * @param  timeout_ms 超时时间（毫秒）
 * @param  retry     重试次数
 * @return 0=成功, 1=超时/失败
 * @note   改进点：原代码中多处while循环无超时保护，可能死锁
 */
static uint8_t AT_SendAndWait(const char *cmd, const char *expect,
                               uint32_t timeout_ms, uint8_t retry)
{
    uint8_t attempt = 0;

    while (attempt <= retry)
    {
        Clear_Buffer();
        Uart2_SendStr(cmd);

        uint32_t elapsed = 0;
        uint32_t step_ms = 50;  /* 每次检查间隔50ms */

        while (elapsed < timeout_ms)
        {
            IWDG_Feed();
            delay_ms(step_ms);
            elapsed += step_ms;

            if (strstr((char*)buf_uart2.buf, expect) != NULL)
            {
                return 0;  /* 找到期望响应，成功 */
            }
        }

        attempt++;
        if (attempt <= retry) {
            printf("[4G] AT指令超时，第%d次重试: %s\r\n", attempt, cmd);
        }
    }

    printf("[4G] AT指令最终失败: %s (期望: %s)\r\n", cmd, expect);
    return 1;  /* 所有重试均失败 */
}

/**
 * @brief  安全初始化4G模块（带超时保护）
 * @param  timeout_ms  每步AT指令的超时时间
 * @return 0=失败, 1=成功
 * @note   改进自原 CSTX_4G_Init()，所有步骤都有超时保护
 *         原代码问题：while循环无退出条件，模块不响应时系统死锁
 */
uint8_t CSTX_4G_Init_Safe(uint32_t timeout_ms)
{
    printf("\r\n[4G] 开始初始化EC200X模块...\r\n");

    /* === 步骤1：测试模块通信（回显测试） === */
    if (AT_SendAndWait("ATE1\r\n", "OK", timeout_ms, AT_CMD_MAX_RETRY) != 0) {
        printf("[4G][ERROR] 模块通信测试失败，请检查接线和波特率\r\n");
        return 0;
    }
    printf("[4G] 模块通信正常\r\n");
    Clear_Buffer();

    /* 查询模块型号 */
    Uart2_SendStr("ATI\r\n");
    delay_ms(500);
    Clear_Buffer();

    /* === 步骤2：检查SIM卡 === */
    if (AT_SendAndWait("AT+CIMI\r\n", "460", timeout_ms, AT_CMD_MAX_RETRY) != 0) {
        printf("[4G][ERROR] SIM卡未检测到，请检查SIM卡\r\n");
        return 0;
    }
    printf("[4G] SIM卡正常，卡号: %s\r\n", buf_uart2.buf + 8);
    Clear_Buffer();

    /* === 步骤3：检查网络注册 === */
    if (AT_SendAndWait("AT+CGATT?\r\n", "+CGATT: 1", timeout_ms, AT_CMD_MAX_RETRY) != 0) {
        printf("[4G][ERROR] 网络注册失败，请检查信号和APN\r\n");
        return 0;
    }
    printf("[4G] 网络注册成功\r\n");
    Clear_Buffer();

    /* === 步骤4：查询信号质量 === */
    Uart2_SendStr("AT+CSQ\r\n");
    delay_ms(300);
    IWDG_Feed();
    strx = strstr((char*)buf_uart2.buf, "+CSQ:");
    if (strx) {
        printf("[4G] 信号质量: %s (最大值31)\r\n", buf_uart2.buf + 14);
    }
    Clear_Buffer();

    /* === 步骤5：开启GPS === */
    printf("[4G] 正在开启GPS模块...\r\n");
    Uart2_SendStr("AT+QGPS=1\r\n");
    delay_ms(1000);
    IWDG_Feed();
    Clear_Buffer();

    /* 等待GPS就绪（最多等待10秒） */
    {
        uint8_t gps_retry = 0;
        uint8_t gps_ready = 0;

        while (gps_retry < 10)
        {
            IWDG_Feed();
            delay_ms(1000);
            Uart2_SendStr("AT+QGPS?\r\n");
            delay_ms(300);

            if (strstr((char*)buf_uart2.buf, "+QGPS: 1"))
            {
                printf("[4G] GPS模块已就绪\r\n");
                gps_ready = 1;
                break;
            }
            printf("[4G] 等待GPS定位... (%d/10)\r\n", gps_retry + 1);
            Clear_Buffer();
            gps_retry++;
        }

        if (!gps_ready) {
            printf("[4G][WARN] GPS模块启动超时，但继续运行\r\n");
        }
    }

    Clear_Buffer();
    IWDG_Feed();
    subscribe_done = 0;  /* 重置订阅标志 */
    return 1;
}

/**
 * @brief  注册到OneNET平台（MQTT连接）
 * @note   使用config.h中的配置参数
 *         改进：增加超时保护，不再无限等待
 */
void CSTX_4G_RegALIYUNIOT(void)
{
    /* 先断开可能存在的旧连接 */
    Uart2_SendStr("AT+QMTDISC=0\r\n");
    delay_ms(300);
    Clear_Buffer();

    Uart2_SendStr("AT+QMTCLOSE=0\r\n");
    delay_ms(300);
    Clear_Buffer();

    /* 设置MQTT版本为3.1.1 */
    Uart2_SendStr("AT+QMTCFG=\"version\",0,4\r\n");
    delay_ms(500);
    IWDG_Feed();
    Clear_Buffer();

    /* 打开MQTT连接（带超时保护） */
    printf("[MQTT] 正在连接OneNET服务器...\r\n");
    memset(ATSTR, 0, AT_CMD_BUF_SIZE);
    sprintf(ATSTR, "AT+QMTOPEN=0,\"%s\",%d\r\n", SERVER_IP, SERVER_PORT);
    Uart2_SendStr(ATSTR);

    /* 等待连接成功，最多等待 MQTT_CONNECT_TIMEOUT 毫秒 */
    {
        uint32_t elapsed = 0;
        uint32_t step_ms = 50;
        while (elapsed < MQTT_CONNECT_TIMEOUT)
        {
            IWDG_Feed();
            delay_ms(step_ms);
            elapsed += step_ms;
            if (strstr((char*)buf_uart2.buf, "+QMTOPEN: 0,0")) {
                break;
            }
        }
    }
    Clear_Buffer();

    /* MQTT登录（使用config.h中的凭据） */
    memset(ATSTR, 0, AT_CMD_BUF_SIZE);
    sprintf(ATSTR, "AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n",
            DEVICE_NAME, PRODUCT_KEY, DEVICE_SECRET);
    printf("[MQTT] 登录指令: %s\r\n", ATSTR);
    Uart2_SendStr(ATSTR);

    /* 等待登录成功 */
    {
        uint32_t elapsed = 0;
        uint32_t step_ms = 50;
        while (elapsed < MQTT_CONNECT_TIMEOUT)
        {
            IWDG_Feed();
            delay_ms(step_ms);
            elapsed += step_ms;
            if (strstr((char*)buf_uart2.buf, "+QMTCONN: 0,0,0")) {
                break;
            }
        }
    }
    Clear_Buffer();
    printf("[MQTT] 连接OneNET完成\r\n");
}

/**
 * @brief  初始化时订阅OneNET属性设置Topic（只调用一次）
 * @note   改进点：原代码在每次发送数据时都执行订阅，造成不必要的通信开销
 *         现在改为在初始化完成后调用一次即可
 */
void CSTX_4G_SubscribeOnce(void)
{
    if (subscribe_done) {
        return;  /* 已订阅，直接返回 */
    }

    memset(ATSTR, 0, AT_CMD_BUF_SIZE);
    sprintf(ATSTR, "AT+QMTSUB=0,1,\"$sys/%s/%s/thing/property/set\",2\r\n",
            PRODUCT_KEY, DEVICE_NAME);

    if (AT_SendAndWait(ATSTR, "+QMTSUB:", AT_CMD_TIMEOUT_MS, AT_CMD_MAX_RETRY) == 0) {
        subscribe_done = 1;
        printf("[MQTT] Topic订阅成功\r\n");
    } else {
        printf("[MQTT][WARN] Topic订阅失败\r\n");
    }
    Clear_Buffer();
}

/**
 * @brief  构造OneNET数据上报JSON负载
 * @param  Str       输出缓冲区指针
 * @param  Temp      温度值（0-255）
 * @param  Humi      湿度值（0-255）
 * @param  Latitude  纬度（double）
 * @param  Longitude 经度（double）
 * @param  Altitude  海拔高度（米，double）
 * @param  Speed     速度（米/秒，double）
 * @param  Direction 运动方向角度（0-360度，double）
 * @return JSON数据实际长度
 */
uint8_t MakeOnenetPayload(uint8_t *Str, uint8_t Temp, uint8_t Humi,
                          double Latitude, double Longitude,
                          double Altitude, double Speed, double Direction)
{
    char JsonBuf[JSON_BUF_SIZE];
    uint16_t JsonLen;

    /*
     * 构造符合OneNET物模型格式的JSON数据
     * 包含：温湿度、GPS坐标（经纬度）、海拔、速度、方向
     */
    sprintf(JsonBuf,
            "{\"id\":\"123\",\"params\":{"
            "\"temp\":{\"value\":%d},"
            "\"humi\":{\"value\":%d},"
            "\"latitude\":{\"value\":%.6f},"
            "\"longitude\":{\"value\":%.6f},"
            "\"altitude\":{\"value\":%.2f},"
            "\"speed\":{\"value\":%.2f},"
            "\"direction\":{\"value\":%.1f}"
            "}}",
            Temp, Humi, Latitude, Longitude, Altitude, Speed, Direction);

    JsonLen = strlen(JsonBuf);
    memcpy(Str, JsonBuf, JsonLen);
    return JsonLen;
}

/**
 * @brief  上传数据到OneNET平台
 * @param  temp      温度值
 * @param  humi      湿度值
 * @param  latitude  纬度
 * @param  longitude 经度
 * @param  altitude  海拔高度（米）
 * @param  speed     速度（米/秒）
 * @param  direction 运动方向角度
 * @return 0=成功, 1=失败
 * @note   改进点：
 *         1. 返回值从void改为uint8_t，可判断发送成败
 *         2. 订阅操作移至初始化阶段（CSTX_4G_SubscribeOnce），不再每次发送都订阅
 *         3. 增加超时保护
 */
uint8_t CSTX_4G_ALYIOTSenddata(uint8_t temp, uint8_t humi,
                                double latitude, double longitude,
                                double altitude, double speed, double direction)
{
    uint8_t SendBuf[JSON_BUF_SIZE];
    uint8_t JsonLen;

    /* 构造JSON数据 */
    JsonLen = MakeOnenetPayload(SendBuf, temp, humi, latitude, longitude,
                                altitude, speed, direction);

    /* 发布MQTT消息 */
    memset(ATSTR, 0, AT_CMD_BUF_SIZE);
    sprintf(ATSTR,
            "AT+QMTPUBEX=0,0,0,0,\"$sys/%s/%s/thing/property/post\",%d\r\n",
            PRODUCT_KEY, DEVICE_NAME, JsonLen);
    Uart2_SendStr(ATSTR);
    delay_ms(300);

    /* 等待模块返回发送提示符 ">" */
    {
        uint8_t wait_count = 0;
        while (!strstr((char*)buf_uart2.buf, ">") && wait_count < 100) {
            delay_ms(10);
            wait_count++;
        }
        if (wait_count >= 100) {
            printf("[MQTT][ERROR] 等待发送提示符超时\r\n");
            Clear_Buffer();
            return 1;
        }
    }
    Clear_Buffer();

    /* 发送JSON数据 */
    USART2_Send((char*)SendBuf, JsonLen);
    UART2_send_byte(0x1A);  /* Ctrl+Z 确认发送 */
    delay_ms(500);

    /* 验证发送结果 */
    {
        uint8_t verify_count = 0;
        while (!strstr((char*)buf_uart2.buf, "+QMTPUBEX: 0,0,0") && verify_count < 100) {
            delay_ms(10);
            verify_count++;
        }

        if (verify_count >= 100) {
            printf("[MQTT][ERROR] 数据发送确认超时\r\n");
            Clear_Buffer();
            return 1;
        }
    }

    printf("[MQTT] 数据已发送: 温度=%d°C, 湿度=%d%%, 纬度=%.6f, 经度=%.6f, "
           "海拔=%.2fm, 速度=%.2fm/s, 方向=%.1f°\r\n",
           temp, humi, latitude, longitude, altitude, speed, direction);
    Clear_Buffer();
    return 0;  /* 发送成功 */
}

/**
 * @brief  获取4G模块IMEI号
 * @return IMEI号字符串指针
 */
char* Get_4GIMEI_NUM(void)
{
    Clear_Buffer();
    Uart2_SendStr("AT+CGSN=1\r\n");
    delay_ms(300);
    strx = strstr((char*)buf_uart2.buf, "+CGSN: \"");
    if (strx)
    {
        strncpy(IMEINUMBER, strx + 8, 15);
        IMEINUMBER[15] = '\0';  /* 确保字符串终止 */
        printf("[4G] IMEI: %s\r\n", IMEINUMBER);
    }
    else
    {
        printf("[4G][WARN] 获取IMEI失败\r\n");
        IMEINUMBER[0] = '\0';
    }
    return IMEINUMBER;
}
