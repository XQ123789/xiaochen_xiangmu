#ifndef __MAIN_H
#define __MAIN_H 	

#include "wdg.h"
#include "led.h"
#include "sys.h"	 			
#include "stdlib.h"
#include "string.h"
#include "timer.h"
#include "4G.h"	 
#include "dht11.h"

#define RX_BUF_SIZE 128
char uart_rx_buf[RX_BUF_SIZE];
volatile uint8_t uart_rx_index = 0;
volatile uint8_t rx_cmd_ready = 0;

void Process_Navigation_Command(const char* cmd);
void Send_GPS_Data(float bd_lat, float bd_lon);
uint8_t Calculate_CRC8(const char *data, uint32_t length);

#endif

