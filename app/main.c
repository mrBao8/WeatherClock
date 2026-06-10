#include "page.h"
#include "app.h"
#include "mloop.h"
#include "weather.h"

extern weather_info_t g_first_weather;		//声明全局变量，存放第一次抓到的天气数据

int main(void)
{
	Board_LowLevel_Init();		//外设时钟使能
	Board_Init();				//底层模块初始化

	welcome_page_display();
	
	wireless_init();			//WIFI连接、天气时间获取、WiFi页面显示
	
	main_loop_init();			//循环更新初始化，注册回调函数，
	main_page_display();
	
				//直接显示上一次抓取的信息,不浪费开机、连接的几秒钟
	main_page_refresh_outdoor_temper(g_first_weather.temperature);
    main_page_refresh_weather_icon(g_first_weather.weather_code);
	
	while(1)
	{
		main_loop_proc();		//循环更新时间、WiFi、温湿度
	}
	
}

