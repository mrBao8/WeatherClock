#include "stm32f4xx.h"                  // Device header
#include <string.h>
#include "rtc.h"  
void rtc_init(void)
{
	RTC_InitTypeDef RTC_InitStreuct;
	RTC_StructInit(&RTC_InitStreuct);
	RTC_Init(&RTC_InitStreuct);
	
	RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
}

static void _rtc_set_time_once(const rtc_date_time_t *date_time)
{
    RTC_DateTypeDef date; // 1. 声明 STM32 固件库自带的“日期”结构体
    RTC_TimeTypeDef time; // 2. 声明 STM32 固件库自带的“时间”结构体
    
    RTC_DateStructInit(&date); // 3. 帮官方日期结构体清零并初始化
    RTC_TimeStructInit(&time); // 4. 帮官方时间结构体清零并初始化
    
    // 5.核心转换：STM32 硬件年份寄存器只能存两位数（0~99）
    // 所以如果你传入 2026，这里减去 2000，存进硬件的就是 26
    date.RTC_Year = date_time->year - 2000;
    date.RTC_Month = date_time->month;
    date.RTC_Date = date_time->day;
    date.RTC_WeekDay = date_time->weekday;
    
    // 6. 把时、分、秒也像素级赋值过去
    time.RTC_Hours = date_time->hour;
    time.RTC_Minutes = date_time->minute;
    time.RTC_Seconds = date_time->second;
    
    // 7. 真正写入硬件：RTC_Format_BIN 告诉固件库“我们给的是标准的十进制数”
    // 固件库在底层会自动帮你转成硬件寄存器要求的 BCD 码写入
    RTC_SetDate(RTC_Format_BIN, &date); // 写入年月日星期
    RTC_SetTime(RTC_Format_BIN, &time); // 写入时分秒
}

static void _rtc_get_time_once(rtc_date_time_t *date_time)
{
    RTC_DateTypeDef date; // 1. 声明官方日期结构体接收器
    RTC_TimeTypeDef time; // 2. 声明官方时间结构体接收器
    
    RTC_DateStructInit(&date);
    RTC_TimeStructInit(&time);
    
    // 3. 从芯片硬件寄存器里，把当前的洛阳时间捞出来
    RTC_GetDate(RTC_Format_BIN, &date);
    RTC_GetTime(RTC_Format_BIN, &time);
    
    // 4. 反向还原：硬件里读出来的是两位数（比如 26），加上 2000，还原成 2026 给 LCD 
    date_time->year = 2000 + date.RTC_Year;
    date_time->month = date.RTC_Month;
    date_time->day = date.RTC_Date;
    date_time->weekday = date.RTC_WeekDay;
    
    // 5. 还原时、分、秒
    date_time->hour = time.RTC_Hours;
    date_time->minute = time.RTC_Minutes;
    date_time->second = time.RTC_Seconds;
}

void rtc_set_time(const rtc_date_time_t *date_time)
{
    rtc_date_time_t rtime; // 声明一个临时变量，用来做核对
    do {
        _rtc_set_time_once(date_time); // 1. 轰击一次寄存器，把时间写进去
        _rtc_get_time_once(&rtime);    // 2. 紧接着立刻把刚才写进去的时间读回来
    } while (date_time->second != rtime.second); 
    // 3.校验：核对“我想写的秒数”和“实际读到的秒数”是否一致。
    // 如果不一致（极罕见情况：刚写完硬件秒针刚好跳变了），就无条件掉头重新写，直到锁死一致才退出！
}

void rtc_get_time(rtc_date_time_t *date_time)
{
    rtc_date_time_t time1, time2; // 1. 准备两个小口袋 time1 和 time2
    do {
        _rtc_get_time_once(&time1); // 2. 第一次读取硬件时间，存入 time1
        _rtc_get_time_once(&time2); // 3. 紧接着以微秒级的速度再读一次，存入 time2
    } while (memcmp(&time1, &time2, sizeof(rtc_date_time_t)) != 0);
    // 4. 找茬：用 memcmp 像素级比对两次读到的整块内存（年、月、日、时、分、秒）。
    // 如果返回值不等于 0（说明两次读到的不一样！刚好撞上了比如 11:59:59 变成 12:00:00 的进位翻转瞬间）
    // do-while 循环会掉头重新读取，直到连续两次读到的数据完全一模一样
    
    // 5. 此时此刻，time1 已经是被连续两次验证过、绝对安全的纯净数据了
    // 用 memcpy 把 time1 里的数据完整复制到最终的 date_time 内存中，交还给主程序
    memcpy(date_time, &time1, sizeof(rtc_date_time_t));
}
