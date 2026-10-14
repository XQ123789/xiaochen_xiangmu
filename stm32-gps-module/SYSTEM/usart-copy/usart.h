#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "sys.h" 
	

#define BUFLEN 256      //数组缓存大小
typedef struct _UART_BUF
{
    char buf [BUFLEN+1];               
    unsigned int index ;
}UART_BUF;	
	
#define RX_BUF_SIZE 128
static char rx_buffer[RX_BUF_SIZE];
static uint32_t rx_index = 0;	

void uart_init(u32 bound);
void uart2_init(u32 bound);
void uart3_init(u32 bound);
void UART1_send_byte(char data);
void UART2_send_byte(char data);
void UART3_send_byte(char data);
void Uart1_SendStr(char*SendBuf);
void Uart2_SendStr(char*SendBuf);
void Uart3_SendStr(char*SendBuf);
void USART2_Send(char *Data,uint16_t Len);
void Clear_Buffer_UART1(void);



extern UART_BUF buf_uart1;     //PC
extern UART_BUF buf_uart2;     //4G
extern UART_BUF buf_uart3;     //TTL

#endif


