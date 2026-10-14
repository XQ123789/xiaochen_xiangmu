/**
 * @file    main.c
 * @brief   GPS实时定位追踪系统 - 改进版主程序
 * @note    改进自原项目 stm32GPS定位模块/USER/main.c
 *
 * 主要改进：
 *   1. 引入状态机架构（INIT→GPS_READ→GPS_PROCESS→DATA_UPLOAD→IDLE）
 *   2. 添加错误计数和自动恢复逻辑
 *   3. 用 config.h 替换所有魔数
 *   4. 添加LED状态指示，直观反映系统状态
 *   5. DHT11读取间隔控制，不再每次循环都读
 *   6. GPS无效时不发送零坐标，避免污染云端数据
 *   7. 通信超时和重试机制
 */

#include "main.h"
#include "config.h"

/* ============ 系统状态枚举 ============ */
typedef enum {
    STATE_INIT = 0,         /* 初始化完成，等待进入工作循环 */
    STATE_GPS_READ,         /* 读取GPS数据 */
    STATE_GPS_PROCESS,      /* 处理GPS数据（坐标转换、传感器读取） */
    STATE_DATA_UPLOAD,      /* 上传数据到OneNET平台 */
    STATE_IDLE,             /* 空闲等待，准备下一轮循环 */
    STATE_ERROR_RECOVERY    /* 错误恢复：重新初始化4G模块 */
} SystemState_t;

/* ============ 错误计数阈值 ============ */
#define MAX_GPS_FAIL      5     /* 连续GPS失败5次触发恢复 */
#define MAX_UPLOAD_FAIL   3     /* 连续上传失败3次触发重连 */

/* ============ 静态变量 ============ */
static uint8_t  gps_fail_count    = 0;   /* GPS连续失败计数 */
static uint8_t  upload_fail_count = 0;   /* 上传连续失败计数 */
static uint32_t last_dht11_read_time = 0; /* DHT11上次读取时间戳 */

/**
 * @brief  获取系统运行时间戳（毫秒）
 * @note   简易实现，利用SysTick或定时器提供毫秒级时间基准
 *         需根据实际硬件资源实现，此处提供框架
 * @return 系统运行毫秒数
 */
static uint32_t get_tick_count(void)
{
    /*
     * 实现方式1：使用SysTick计数器
     * return SysTick->VAL;  // 需配合SysTick_Config使用
     *
     * 实现方式2：使用TIM定时器（推荐）
     * 配置一个1ms中断的定时器，在中断中递增全局变量
     *
     * 实现方式3：使用delay_ms内部的计数变量
     * 如果delay.c中有millis变量可直接使用
     */
    extern volatile uint32_t sys_tick_ms;  /* 需要在delay.c或timer中实现 */
    return sys_tick_ms;
}

/**
 * @brief  主函数
 * @note   采用状态机架构，各状态职责清晰
 */
int main(void)
{
    /* GPS数据变量 */
    double lat = 0, lon = 0, bd_lat = 0, bd_lon = 0;
    double alt = 0.0, speed = 0.0, dir = 0.0;
    uint8_t gps_valid = 0;

    /* 状态机初始状态 */
    SystemState_t state = STATE_INIT;

    /* ========== 硬件初始化 ========== */
    delay_init();
    NVIC_Configuration();
    CSTX_4GCTR_Init();

    /* UART1 - 调试串口 */
    uart_init(UART_DEBUG_BAUD);
    Uart1_SendStr("UART1 Init OK\r\n");
    delay_ms(500);

    /* LED初始化 */
    LED_Init();
    LED_SetMode(LED_MODE_INIT);

    /* UART2 - 4G模块 */
    uart2_init(UART_4G_BAUD);
    Uart2_SendStr("UART2 Init OK\r\n");
    delay_ms(500);

    /* UART3 - 树莓派 */
    uart3_init(UART_PI_BAUD);

    /* 打印启动信息 */
    printf("\r\n================================\r\n");
    printf("  GPS Tracker Started\r\n");
    printf("  Build: %s %s\r\n", __DATE__, __TIME__);
    printf("================================\r\n");

    /* 看门狗初始化 */
    IWDG_Init(WDG_PRESCALE, WDG_RELOAD);

    /* DHT11传感器初始化 */
    DHT11_Init();

    /* ========== 4G模块初始化（带超时保护） ========== */
    if (!CSTX_4G_Init_Safe(AT_CMD_TIMEOUT_MS)) {
        printf("[ERROR] 4G模块初始化失败！\r\n");
        LED_SetMode(LED_MODE_ERROR);
    } else {
        /* 注册到OneNET平台（MQTT连接） */
        CSTX_4G_RegALIYUNIOT();
        /* 获取IMEI号 */
        Get_4GIMEI_NUM();
        LED_SetMode(LED_MODE_READY);
    }

    /* ========== 主循环 - 状态机 ========== */
    while (1)
    {
        /* 每次循环都喂狗，防止系统死锁 */
        IWDG_Feed();

        switch (state)
        {
        /* ---------- 状态0：初始化完成 ---------- */
        case STATE_INIT:
            /* 初始化完成，直接转入GPS读取 */
            state = STATE_GPS_READ;
            break;

        /* ---------- 状态1：读取GPS数据 ---------- */
        case STATE_GPS_READ:
            LED_SetMode(LED_MODE_GPS_OK);
            Clear_Buffer();

            /* 发送GPS定位查询AT指令 */
            Uart2_SendStr("AT+QGPSLOC=2\r\n");
            delay_ms(GPS_WAIT_MS);

            /* 解析GPS响应 */
            if (strstr((char*)buf_uart2.buf, "+QGPSLOC:"))
            {
                char *gps_start = strstr((char*)buf_uart2.buf, "+QGPSLOC:");

                /*
                 * 解析格式：+QGPSLOC: UTC,latitude,longitude,...,altitude,...,speed,direction
                 * 使用sscanf跳过不需要的字段，提取关键数据
                 */
                if (sscanf(gps_start + 9, "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
                           &lat, &lon, &alt, &speed, &dir) >= 2)
                {
                    /* WGS84坐标转高德(GCJ-02)坐标 */
                    wgs84_to_amap(lon, lat, &bd_lon, &bd_lat);
                    gps_valid = 1;
                    gps_fail_count = 0;  /* 成功则清零失败计数 */
                    printf("GPS OK: 纬度=%.6f, 经度=%.6f\r\n", bd_lat, bd_lon);
                }
                else
                {
                    gps_valid = 0;
                    gps_fail_count++;
                    printf("[WARN] GPS数据解析失败\r\n");
                }
            }
            else
            {
                gps_valid = 0;
                gps_fail_count++;
                printf("[WARN] GPS无数据响应\r\n");
            }

            /* 检查是否需要进入错误恢复 */
            if (gps_fail_count >= MAX_GPS_FAIL) {
                printf("[WARN] GPS连续失败%d次，进入恢复流程...\r\n", gps_fail_count);
                state = STATE_ERROR_RECOVERY;
                break;
            }

            state = STATE_GPS_PROCESS;
            break;

        /* ---------- 状态2：数据处理 ---------- */
        case STATE_GPS_PROCESS:
            /*
             * 按间隔读取DHT11温湿度
             * DHT11最高采样频率1Hz，不必每次循环都读
             * 每隔 DHT11_READ_INTERVAL 毫秒读取一次
             */
            {
                uint32_t now = get_tick_count();
                if (now - last_dht11_read_time >= DHT11_READ_INTERVAL) {
                    DHT11_Read_TempAndHumidity();
                    last_dht11_read_time = now;
                    printf("DHT11: 温度=%d°C, 湿度=%d%%\r\n",
                           DHT11_Data.temp_int, DHT11_Data.humi_int);
                }
            }
            state = STATE_DATA_UPLOAD;
            break;

        /* ---------- 状态3：数据上传 ---------- */
        case STATE_DATA_UPLOAD:
            if (gps_valid) {
                LED_SetMode(LED_MODE_UPLOADING);

                /* 上传数据到OneNET（温度、湿度、坐标、高度、速度、方向） */
                uint8_t ret = CSTX_4G_ALYIOTSenddata(
                    DHT11_Data.temp_int,
                    DHT11_Data.humi_int,
                    bd_lat, bd_lon,
                    alt, speed, dir
                );

                if (ret == 0) {
                    upload_fail_count = 0;  /* 成功则清零 */
                    printf("Upload OK\r\n");
                } else {
                    upload_fail_count++;
                    printf("[WARN] 上传失败 (第%d次)\r\n", upload_fail_count);
                }

                /* 连续上传失败过多，触发重连 */
                if (upload_fail_count >= MAX_UPLOAD_FAIL) {
                    printf("[WARN] 连续上传失败%d次，重新连接...\r\n", upload_fail_count);
                    state = STATE_ERROR_RECOVERY;
                    break;
                }
            } else {
                /*
                 * 改进点：GPS无效时不发送数据
                 * 原项目即使没有有效GPS也会发送零坐标，导致云端出现错误定位点
                 */
                printf("Skip upload (GPS数据无效)\r\n");
            }
            state = STATE_IDLE;
            break;

        /* ---------- 状态4：空闲等待 ---------- */
        case STATE_IDLE:
            /* 根据GPS状态显示不同LED模式 */
            LED_SetMode(gps_valid ? LED_MODE_READY : LED_MODE_GPS_FAIL);
            delay_ms(LOOP_INTERVAL_MS);
            /* 回到GPS读取，开始新一轮循环 */
            state = STATE_GPS_READ;
            break;

        /* ---------- 状态5：错误恢复 ---------- */
        case STATE_ERROR_RECOVERY:
            printf("[RECOVERY] 正在重新初始化4G模块...\r\n");
            LED_SetMode(LED_MODE_ERROR);
            delay_ms(2000);  /* 等待2秒后重试 */

            /* 重新初始化4G模块和MQTT连接 */
            if (CSTX_4G_Init_Safe(AT_CMD_TIMEOUT_MS)) {
                CSTX_4G_RegALIYUNIOT();
                printf("[RECOVERY] 4G模块重连成功\r\n");
            } else {
                printf("[RECOVERY] 4G模块重连失败，将继续重试\r\n");
            }

            /* 清零错误计数，重新开始工作循环 */
            gps_fail_count = 0;
            upload_fail_count = 0;
            state = STATE_GPS_READ;
            break;

        default:
            /* 未知状态，回到初始化 */
            state = STATE_INIT;
            break;
        }
    }
}
