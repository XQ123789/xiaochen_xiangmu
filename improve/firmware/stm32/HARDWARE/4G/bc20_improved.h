/**
 * @file    bc20_improved.h
 * @brief   BC20(EC200U-CN) 4G模块驱动头文件 - 改进版
 * @note    改进自原项目 stm32GPS定位模块/HARDWARE/4G/4G.h
 */

#ifndef __BC20_IMPROVED_H
#define __BC20_IMPROVED_H

#include "usart.h"
#include <stm32f10x.h>
#include "delay.h"

/* ============ 函数声明 ============ */

/**
 * @brief  清空UART2接收缓冲区
 */
void Clear_Buffer(void);

/**
 * @brief  安全初始化4G模块（带超时保护）
 * @param  timeout_ms 每步AT指令的超时时间（毫秒）
 * @return 0=失败, 1=成功
 */
uint8_t CSTX_4G_Init_Safe(uint32_t timeout_ms);

/**
 * @brief  注册到OneNET平台（MQTT连接）
 */
void CSTX_4G_RegALIYUNIOT(void);

/**
 * @brief  初始化时订阅OneNET属性设置Topic（只调用一次）
 * @note   改进点：原代码每次发送都订阅，现在只在初始化时订阅一次
 */
void CSTX_4G_SubscribeOnce(void);

/**
 * @brief  构造OneNET数据上报JSON负载
 * @return JSON数据实际长度
 */
uint8_t MakeOnenetPayload(uint8_t *Str, uint8_t Temp, uint8_t Humi,
                          double Latitude, double Longitude,
                          double Altitude, double Speed, double Direction);

/**
 * @brief  上传数据到OneNET平台
 * @return 0=成功, 1=失败
 */
uint8_t CSTX_4G_ALYIOTSenddata(uint8_t temp, uint8_t humi,
                                double latitude, double longitude,
                                double altitude, double speed, double direction);

/**
 * @brief  获取4G模块IMEI号
 * @return IMEI号字符串指针
 */
char* Get_4GIMEI_NUM(void);

/* ============ 结构体定义 ============ */
typedef struct
{
    uint8_t CSQ;          /* 信号质量 */
    uint8_t Socketnum;    /* Socket编号 */
    uint8_t reclen;       /* 接收数据长度 */
    uint8_t res;          /* 保留 */
    uint8_t recdatalen[10];  /* 接收数据长度数组 */
    uint8_t recdata[100];    /* 接收数据缓冲区 */
    uint8_t netstatus;       /* 网络状态指示 */
} CSTX_4G;

/* 外部变量声明 */
extern CSTX_4G CSTX_4G_Status;
extern char ATSTR[];
extern char IMEINUMBER[];

#endif /* __BC20_IMPROVED_H */
