#include "stm32f4xx.h"                  // Device header
#include "serial.h"
#include "DELAY.H"
#include "espat.h"
#include "weather.h"
#include "lcd.h"
#include "aht20.h"
#include "rtc.h"

void Board_LowLevel_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);//使能GPIOA时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);//使能GPIOB时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);//使能GPIOB时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);//使能I2C时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);//使能USART1时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);//使能USART2时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,ENABLE);//使能SPI2时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);//使能RTC时钟
	PWR_BackupAccessCmd(ENABLE);
    RCC_LSEConfig(RCC_LSE_ON);
    while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
}

void Board_Init(void)
{
    cpu_tick_init();
    Usart1_Init();
	LCD_Init();
	rtc_init();
	aht20_init();
}
