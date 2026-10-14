#include "main.h"

int main(void)
{
    double lat, lon, bd_lat, bd_lon;
    double alt = 0.0, speed = 0.0, dir = 0.0;

    delay_init();
    NVIC_Configuration();
    CSTX_4GCTR_Init();

    uart_init(115200);
    Uart1_SendStr("UART1 Init Successful\r\n");
    delay_ms(1200);

    LED_Init();

    uart2_init(115200);
    Uart2_SendStr("UART2 Init Successful\r\n");
    delay_ms(1200);

    uart3_init(115200);
    Uart3_SendStr("UART3 Init Successful\r\n");
    printf("\r\n ############ 发送数据到ONENET平台/ ############\r\n ############("__DATE__ " - " __TIME__ ")############");

    IWDG_Init(6, 500);  // 预分频64，重装值500 → 约4秒

    CSTX_4G_Init();
    CSTX_4G_RegALIYUNIOT();
    Clear_Buffer_UART1();
    DHT11_Init();
    Get_4GIMEI_NUM();

    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST)!=RESET){printf("[复位] 看门狗触发！\n");RCC_ClearFlag();}
    if(RCC_GetFlagStatus(RCC_FLAG_PORRST)!=RESET) {printf("[复位] 上电复位\n");RCC_ClearFlag();}
    if (RCC_GetFlagStatus(RCC_FLAG_PINRST)!=RESET){printf("[复位] NRST引脚复位\n");RCC_ClearFlag();}

    while(1)
    {
        IWDG_Feed();

        DHT11_Read_TempAndHumidity();

        Clear_Buffer();
        Uart2_SendStr("AT+QGPSLOC=2\r\n");
        delay_ms(500);
        IWDG_Feed();

        if (strstr((char*)buf_uart2.buf, "+QGPSLOC:"))
        {
            char *gps_start = strstr((char*)buf_uart2.buf, "+QGPSLOC:");
            if (sscanf(gps_start + 9, "%*[^,],%lf,%lf,%*f,%lf,%*[^,],%lf,%lf",
                       &lat, &lon, &alt, &speed, &dir) >= 2)
            {
                wgs84_to_amap(lon, lat, &bd_lon, &bd_lat);
                printf("GPS定位成功：%.6f, %.6f\r\n", bd_lat, bd_lon);
            }
            else
            {
                lat = 0; lon = 0;
                printf("GPS数据无效\r\n");
            }
        }
        else
        {
            lat = 0; lon = 0;
            printf("未获取到GPS\r\n");
        }

        CSTX_4G_ALYIOTSenddata(
            DHT11_Data.temp_int,
            DHT11_Data.humi_int,
            bd_lat, bd_lon,
            alt, speed, dir
        );

        IWDG_Feed();
        delay_ms(1000);
    }
}