#include "sys.h"
#include "usart.h"	  
#include "stdio.h"
#include "string.h"
#include "stm32f10x_tim.h" 
#include "wdg.h"
#include "led.h"


#define RX_BUF_SIZE 128
extern char uart_rx_buf[RX_BUF_SIZE];
extern volatile uint8_t uart_rx_index;
extern volatile uint8_t rx_cmd_ready;
//////////////////////////////////////////////////////////////////

//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
_sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{      
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
    USART1->DR = (u8) ch;      
	return ch;
}
#endif 


UART_BUF buf_uart1;     //CH340
UART_BUF buf_uart2;     //NBIOT
UART_BUF buf_uart3;     //TTL 

void UART1_send_byte(char data)
{
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, data);
}
void UART2_send_byte(char data)
{
	while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	USART_SendData(USART2, data);
}

void UART3_send_byte(char data)
{
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, data);
}

//串口1中断服务程序
//注意,读取USARTx->SR能避免莫名其妙的错误   	

//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目

//初始化IO 串口1 
//bound:波特率
void uart_init(u32 bound){
    //GPIO端口设置
    GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
		 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1|RCC_APB2Periph_GPIOA, ENABLE);	//使能USART1，GPIOA时钟
	USART_DeInit(USART1);  //复位串口1
	//USART1_TX   PA.9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //PA.9
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
    GPIO_Init(GPIOA, &GPIO_InitStructure); //初始化PA9
   
    //USART1_RX	  PA.10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;//浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);  //初始化PA10

   //Usart1 NVIC 配置

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
		
	//USART 初始化设置

	USART_InitStructure.USART_BaudRate = bound;//一般设置为115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式

    USART_Init(USART1, &USART_InitStructure); //初始化串口
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);//开启空闲中断
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//开启接收中断
    USART_Cmd(USART1, ENABLE);                    //使能串口 

}
//初始化IO 串2
//bound:波特率
void uart2_init(u32 bound)
{
    //GPIO端口设置
    GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
		 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//使能，GPIOA时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);//USART2
	USART_DeInit(USART2);  //复位串口2
	 //USART2_TX   PA.2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; //PA.2  tx
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
    GPIO_Init(GPIOA, &GPIO_InitStructure); //初始化PA2
   
    //USART2_RX	  PA.3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);  //初始化PA3

   //Usart1 NVIC 配置

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0 ;//抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
  
   //USART 初始化设置

	USART_InitStructure.USART_BaudRate = bound;//115200
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
    USART_Init(USART2, &USART_InitStructure); //初始化串口
		
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);//开启中断
    USART_Cmd(USART2, ENABLE);                    //使能串口 

}
//初始化IO 串3
//bound:波特率
void uart3_init(u32 bound)
{
    //GPIO端口设置
    GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
		 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//使能，GPIOA时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);//USART3
	USART_DeInit(USART3);  //复位串口3
	//USART3_TX   PB10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; //PB10
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
    GPIO_Init(GPIOB, &GPIO_InitStructure); //初始化PA2
   
    //USART3_RX	  PB11
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
    GPIO_Init(GPIOB, &GPIO_InitStructure);  //初始化PB11

   //Usart3 NVIC 配置

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0 ;//抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
  
   //USART 初始化设置

	USART_InitStructure.USART_BaudRate = bound;//115200
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
    USART_Init(USART3, &USART_InitStructure); //初始化串口
	
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);//开启中断
    USART_Cmd(USART3, ENABLE);                    //使能串口 

}

/*-------------------------------------------------*/
/*函数名：串口2 发送数组                             */
/*参  数：bound：波特率                             */
/*返回值：无                                        */
/*-------------------------------------------------*/
void USART2_Send(char *Data,uint16_t Len)
{ 
	uint16_t i;
	for(i=0; i<Len; i++)
	{
		UART2_send_byte(Data[i]);
	}
}

void Uart1_SendStr(char*SendBuf)//串口1打印数据
{
	while(*SendBuf)
	{
        while((USART1->SR&0X40)==0);//等待发送完成 
        USART1->DR = (u8) *SendBuf; 
        SendBuf++;
	}
}


void Uart2_SendStr(char*SendBuf)//串口1打印数据
{
	while(*SendBuf)
	{
        while((USART2->SR&0X40)==0);//等待发送完成 
        USART2->DR = (u8) *SendBuf; 
        SendBuf++;
	}
	//IWDG_Feed();//喂狗
}


void Uart3_SendStr(char*SendBuf) 
{
    while(*SendBuf) 
	{
        USART_SendData(USART3, *SendBuf++);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    }
}

/*****************************************************
清空电脑反馈的缓冲数据 串口1
*****************************************************/
void Clear_Buffer_UART1(void)//清空缓存
{
    buf_uart1.index=0;
    memset(buf_uart1.buf,0,BUFLEN);
	//IWDG_Feed();//喂狗
}

void UART1_receive_process_event(char ch )     //串口2给4g用
{
    if(buf_uart1.index >= BUFLEN)	
    {
        buf_uart1.index = 0 ;
    }
    else
    {
        buf_uart1.buf[buf_uart1.index++] = ch;
    }
}

//串口1的接收中断程序
void USART1_IRQHandler(void)                                //串口1中断服务程序
{
	char Res;
	Res=Res;
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  //接收中断，可以扩展来控制
    {
        Res=USART_ReceiveData(USART1);//接收模块的数据;
        UART1_receive_process_event(Res);//接收模块的数据
    } 
    if(USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)  //模块空闲
    {
        Res=USART_ReceiveData(USART1);//接收模块的数据;
    } 

} 


void ec200x_receive_process_event(unsigned char ch )     //串口2给4g用
{
    if(buf_uart2.index >= BUFLEN)
    {
        buf_uart2.index = 0 ;
    }
    else
    {
        buf_uart2.buf[buf_uart2.index++] = ch;
    }
}

void USART2_IRQHandler(void)                            //串口2接收函数
{
    if(USART_GetITStatus(USART2, USART_IT_RXNE)==SET)
    {
        ec200x_receive_process_event(USART_ReceiveData(USART2));
        USART_ClearITPendingBit(USART2,USART_IT_RXNE);
    }

    if(USART_GetFlagStatus(USART2,USART_FLAG_ORE)==SET)
    {
        ec200x_receive_process_event(USART_ReceiveData(USART2));
        USART_ClearFlag(USART2,USART_FLAG_ORE);
    }
}

void USART3_IRQHandler(void)
{
    // 检查接收中断标志
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = USART_ReceiveData(USART3);
        
        // 处理接收到的字符
        if(ch == '\n' || ch == '\r')  // 以换行或回车作为命令结束符
        {
            if(uart_rx_index > 0)  // 确保缓冲区有数据
            {
                uart_rx_buf[uart_rx_index] = '\0';  // 添加字符串结束符
                rx_cmd_ready = 1;  // ready = 1;  // 设置命令就绪标志
                uart_rx_index = 0;  // 重置索引
            }
        }
        else if(uart_rx_index < RX_BUF_SIZE - 1)  // 防止缓冲区溢出
        {
            uart_rx_buf[uart_rx_index++] = ch;
        }
        
        // 清除中断标志
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}

