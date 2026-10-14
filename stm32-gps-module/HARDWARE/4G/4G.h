#ifndef __BC26_H
#define __BC26_H	
#include "usart.h"
#include <stm32f10x.h>
#include "delay.h"

void CSTX_4G_Init(void);
void CSTX_4G_PDPACT(void);
void CSTX_4G_ConTCP(void);
void CSTX_4G_CreateTCPSokcet(void);
void CSTX_4G_Senddata(int len,uint8_t *data);
void CSTX_4G_CreateSokcet(void);
void Clear_Buffer(void);
void CSTX_4G_ChecekConStatus(void);
void CSTX_4G_RECTCPData(void);
void CSTX_4G_RegALIYUNIOT(void);
void CSTX_4G_ALYIOTSenddata(u8 temp, u8 humi, double latitude, double longitude, 
                           double altitude, double speed, double direction);
char* Get_4GIMEI_NUM(void);
typedef struct
{
    uint8_t CSQ;    
    uint8_t Socketnum;
    uint8_t reclen;
    uint8_t res;      
    uint8_t recdatalen[10];
    uint8_t recdata[100];
    uint8_t netstatus;
} CSTX_4G;


#endif
