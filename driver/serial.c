#include "stm32f4xx.h"                  // Device header
#include <string.h>
#include <stdio.h>

void Usart1_Init(void)						//串口初始化
{
	USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);	//自动装填函数
	
	USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	
	//GPIO引脚复用为串口*必填*
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
	
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_Init(USART1, &USART_InitStructure);
//	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);	//开启串口中断
    USART_Cmd(USART1, ENABLE);	
}

void Usart1_write(const char str[])			//串口打印函数
{
	int len = strlen(str);
	for(int i=0 ; i<len ; i++)				//将字节保存进数组
	{	
		while (USART_GetFlagStatus(USART1,USART_FLAG_TXE)==RESET);
		USART_SendData(USART1,str[i]);
		while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
	}
}

int fputc(int ch, FILE *stream)				//串口重定向，实现printf直接打印字符
{
	while (USART_GetFlagStatus(USART1,USART_FLAG_TXE)==RESET);
	USART_SendData(USART1,(uint16_t)ch);
	
	return ch;
}
	
// 接收一个字节（阻塞等待）
uint8_t usart_receive(void)					
{
    // 等待接收寄存器非空
    while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
    
    // 返回读到的数据
    return USART_ReceiveData(USART1);
}
