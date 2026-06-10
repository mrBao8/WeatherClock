#include "stm32f4xx.h"                  // Device header
#include "stdint.h"
#include "string.h"
#include "delay.h"

#define TICKS_PER_MS (SystemCoreClock / 1000)
#define TICKS_PER_US (SystemCoreClock / 1000 / 1000)

static volatile uint64_t cpu_tick_count; 		//cpu滴答计数，在中断中改变用volatile修饰
static systick_callback_t systick_callback_g = NULL;	//定义一个全局指针变量存放函数地址
	
void cpu_tick_init(void)
{
	SysTick->LOAD = TICKS_PER_MS - 1;
	SysTick->VAL = 0 ;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |SysTick_CTRL_TICKINT_Msk  | SysTick_CTRL_ENABLE_Msk;
}

/*
*@parm 
	VAL 是从LOAD开始向0倒数。LOAD-VAL算出来的就是当前这一毫秒内，已经走过的滴答数；
	cpu_tick_count + 1ms内走过的滴答数 = 总滴答数；
	last_count = 如果上次的记录的滴答数跟cpu_tick_count不相等，说明发生了中断，now的值落后了，重新循环一次
*/

uint64_t cpu_now(void)		//计算程序跑到现在的总滴答数
{
	uint64_t now,last_count;
	do{
		last_count = cpu_tick_count;		//提前记录一下滴答数
		now = cpu_tick_count + SysTick->LOAD - SysTick->VAL;	//开机开始的滴答数+(LOAD-VAL)
	}while(last_count != cpu_tick_count );
	
	return now;
}

uint64_t cpu_get_ms(void)
{
	return cpu_tick_count / TICKS_PER_MS;	//记录当前走了多少ms
}

uint64_t cpu_get_us(void)
{
	return cpu_tick_count / TICKS_PER_US;	//记录当前走了多少us
}

void cpu_delay_ms( uint32_t ms)		//延时，逻辑是现在走过的滴答数有没有超过我想要的滴答数，也就是时间
{
	uint64_t now = 0 ;
	now = cpu_now();
	while( cpu_now() - now < (uint64_t)ms * TICKS_PER_MS);
}

void cpu_delay_us( uint32_t us)
{
	uint64_t now = 0 ;
	now = cpu_now();
	while( cpu_now() - now < (uint64_t)us * TICKS_PER_US);
}

void systick_register_callback(systick_callback_t cb)	//提供接口，让外面的函数能注册
{
	systick_callback_g = cb ;	//记录函数地址
}


void SysTick_Handler(void)
{
	cpu_tick_count += TICKS_PER_MS; 		//每1ms进一次中断，cpu_tick_count加1ms的滴答数
	
	if(systick_callback_g != NULL)			//登记回调函数
	{
		systick_callback_g();
	}
}
