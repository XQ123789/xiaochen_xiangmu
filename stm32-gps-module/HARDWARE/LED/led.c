#include "led.h"
#include "delay.h"
#include <math.h>


//LED IO初始化
void LED_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	//PB3 PB4 需要设置复用
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,ENABLE);	//打开GPIO使能时钟，先开复用功能
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);//映射PB3 PB4 口到GPIO功能 
	

	//LED1-->PB5 LED2-->PB4 LED3-->PB3
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_4|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //输出口
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 //频率
	GPIO_Init(GPIOB, &GPIO_InitStructure);	  				 //初始化PB口
	//PB5 PB4 PB3
	GPIO_SetBits(GPIOB,GPIO_Pin_5|GPIO_Pin_4|GPIO_Pin_3); 	//关闭灯 			

}
 

/*****************************************************
跑马灯
*****************************************************/
void LED_Run(void)
{
	
	GPIO_ResetBits(GPIOB,GPIO_Pin_5); 	
	delay_ms(1000);
	GPIO_ResetBits(GPIOB,GPIO_Pin_4); 	
	delay_ms(1000);
	GPIO_ResetBits(GPIOB,GPIO_Pin_3); 	
	delay_ms(1000);
}
 

/*****************************************************
开启模块 低电平对模块进行供电 高电平不对模块进行供电
*****************************************************/
void CSTX_4GCTR_Init(void)
{  
    GPIO_InitTypeDef  GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	 //使能PC端口时钟
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;				 		//PA12直接控制模块进行供电端口配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 //IO口速度为50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);		  			 //PA12
	
	GPIO_SetBits(GPIOA,GPIO_Pin_12); 	
	delay_ms(1000);
	GPIO_ResetBits(GPIOA,GPIO_Pin_12); 	 
}

// 定义常量（保持原样）
#define PI 3.1415926535897932384626
#define EARTH_A 6378245.0          
#define EARTH_EE 0.00669342162296594323
#define EAST_OFFSET 0.000365  // 调经度，东西方向，往东加，往西减
#define NORTH_OFFSET 0.000275  // 调纬度，南北方向，往北加，往南减


// 核心偏移计算（保留之前的优化项）
static void get_delta(double lon, double lat, double *delta_lon, double *delta_lat)
{
    double dLon = lon - 105.0;
    double dLat = lat - 35.0;

    *delta_lon = 300.0 + dLon + 2.0 * dLat 
        + 0.1 * dLon*dLon + 0.1 * dLon*dLat + 0.1 * sqrt(fabs(dLon))
        + (20.0*sin(6*PI*dLon) + 20.0*sin(2*PI*dLon)) * 2.0/3.0
        + (20.0*sin(PI*dLon) + 40.0*sin(PI*dLon/3.0)) * 2.0/3.0
        + (150.0*sin(PI*dLon/12.0) + 300.0*sin(PI*dLon/30.0)) * 2.0/3.0
        + 0.002 * dLon*dLat;

    *delta_lat = -100.0 + 2.0*dLon + 3.0*dLat 
        + 0.2*dLat*dLat + 0.1*dLon*dLat + 0.2*sqrt(fabs(dLon))
        + (20.0*sin(6*PI*dLon) + 20.0*sin(2*PI*dLat)) * 2.0/3.0
        + (20.0*sin(PI*dLat) + 40.0*sin(PI*dLat/3.0)) * 2.0/3.0
        + (160.0*sin(PI*dLat/12.0) + 320.0*sin(PI*dLat/30.0)) * 2.0/3.0
        + 0.001 * dLat*dLat;
}

void wgs84_to_gcj02(double wgs_lon, double wgs_lat, double *gcj_lon, double *gcj_lat)
{
    double lon = wgs_lon;
    double lat = wgs_lat;
    double delta_lon, delta_lat;
    double rad_lat, magic, sqrt_magic;
    int i;
	double dlon, dlat;

    for (i = 0; i < 3; i++) {
        rad_lat = lat * PI / 180.0;
        magic = 1 - EARTH_EE * sin(rad_lat) * sin(rad_lat);
        sqrt_magic = sqrt(magic);

        get_delta(lon, lat, &delta_lon, &delta_lat);

        dlon = delta_lon * 180.0 / (EARTH_A / sqrt_magic * cos(rad_lat) * PI);
        dlat = delta_lat * 180.0 / ((EARTH_A * (1 - EARTH_EE)) / (magic * sqrt_magic) * PI);

        lon += dlon * (0.5 / (i + 1));
        lat += dlat * (0.5 / (i + 1));
    }

    // 最终叠加向东微调补偿
    *gcj_lon = lon + EAST_OFFSET;
    *gcj_lat = lat + NORTH_OFFSET;
}

void wgs84_to_amap(double wgs_lon, double wgs_lat, double *amap_lon, double *amap_lat)
{
    wgs84_to_gcj02(wgs_lon, wgs_lat, amap_lon, amap_lat);
}



