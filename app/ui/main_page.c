#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "image.h"
#include "weather.h"
#include "page.h"
#include "lcd.h"
#include "app.h"
#include "rtc.h"

extern weather_info_t g_first_weather;

void main_page_display(void)
{
	LCD_Clear(BLACK);		//清屏，打黑色底
	
	//--时间日期板块：显示WiFi图标、ssid、时间日期
	do{
		LCD_Fill_Color(15, 15, 224, 154, COLOR_WARM_CREAM);	
		
		//显示WiFi图标
		LCD_Show_Photo(23, 20, &icon_wifi);		
		
		//WIFI_ssid
		const char *ssid = WIFI_SSID ;
		int ssid_len = strlen(ssid) * 9 ;
		uint16_t ssid_x = 203 - ssid_len ;
		if(ssid_len > 165 )
		{ssid_x = 215 - ssid_len;}
		LCD_Show_String(ssid_x, 22, "------", BLACK, WHITE, &font16_ascii);
		
		// 初始占位时间
		LCD_Show_String(25,42,"--:--",BLACK,COLOR_WARM_CREAM,&font76_time);
		LCD_Show_String(25,121,"----/--/-- 星期-",COLOR_TITANIUM_GRAY,COLOR_WARM_CREAM,&font20);
		}while(0);
	
	//--室内温湿度板块：显示室内温湿度
	do{
		
		LCD_Fill_Color(15, 165, 114, 304, WHITE);	
		//显示室内温度
		LCD_Show_String(19,170,"室内环境",BLACK,WHITE,&font24);
		LCD_Show_String(86,191,"C",BLACK,WHITE,&font32);
		LCD_Show_String(91,255,"%",BLACK,WHITE,&font32);
		// 初始温度占位
		LCD_Show_String(30,192,"--",BLACK,WHITE,&font54_temper);
		LCD_Show_String(28,239,"--",BLACK,WHITE,&font54_temper);
	}while(0);
	
	//--城市温度板块：显示城市温度、温度计和日常天气图标
	do{
		LCD_Fill_Color(125, 165, 224, 304, WHITE);	

		if (g_first_weather.weather_code == -1) {
            LCD_Show_String(127, 166, "Notnet", BLACK, WHITE, &font24);
            LCD_Show_String(135, 190, "--", BLACK, WHITE, &font54_temper); // 温度显示 "--" 替代 "0"
        } else {
            LCD_Show_String(127, 166, "洛阳", BLACK, WHITE, &font24);
            LCD_Show_String(135, 190, "--", BLACK, WHITE, &font54_temper);
        }
		// 初始城市天气占位
		LCD_Show_String(192,189,"C",BLACK,WHITE,&font32);
		LCD_Show_Photo(166, 240, &icon_na);
		LCD_Show_Photo(139, 239, &icon_wenduji);

	}while(0);
}

void main_page_refresh_wifi_ssid(const char *ssid)
{
    char str[21];
    snprintf(str, sizeof(str), "%20s", ssid);
    LCD_Show_String(50, 23, str,BLACK,WHITE,&font16_ascii);
}

void main_page_refresh_time(const rtc_date_time_t *time)	//时间局部刷新，每秒调用一次
{
	char str[6];
	char comma = (time->second % 2 == 0) ? ':' : ' ';		//对秒取余，偶数闪烁一次
	snprintf(str,sizeof(str),"%02u%c%02u",time->hour,comma,time->minute);
	LCD_Show_String(25,42,str,BLACK,WHITE,&font76_time);
}

void main_page_refresh_date(const rtc_date_time_t *date)
{
    char str[18];

    snprintf(str, sizeof(str), "%04u/%02u/%02u 星期%s", date->year, date->month, date->day,
        date->weekday == 1 ? "一" :
        date->weekday == 2 ? "二" :
        date->weekday == 3 ? "三" :
        date->weekday == 4 ? "四" :
        date->weekday == 5 ? "五" :
        date->weekday == 6 ? "六" :
        date->weekday == 7 ? "天" : "X");
        
    LCD_Show_String(25, 121, str, COLOR_TITANIUM_GRAY, COLOR_WARM_CREAM, &font20);
}

void main_page_refresh_inner_temper(float temp)	//室内温度局部刷新
{
	char str[3];
	snprintf(str, sizeof(str), "%2.0f", temp); // 强制限宽2格，完美覆盖旧数字
	LCD_Show_String(30, 192, str, BLACK, WHITE, &font54_temper);
}

void main_page_refresh_inner_humidity(float humi) //室内湿度局部刷新
{
    char str[3];
    snprintf(str, sizeof(str), "%2.0f", humi);
    LCD_Show_String(28, 239, str, BLACK, WHITE, &font54_temper);
}

void main_page_refresh_outdoor_temper(float temp) //室外温度局部刷新
{
    char str[3];
    snprintf(str, sizeof(str), "%2.0f", temp);
    // 刷新室外温度
    LCD_Show_String(135, 190, str, BLACK, WHITE, &font54_temper);
}

void main_page_refresh_weather_icon(const int code) //天气图标局部刷新
{
    const image_t *icon;
    if (code == 0 || code == 2 || code == 38)	//晴天
        icon = &icon_qing;
    else if (code == 1 || code == 3)			//晚上
        icon = &icon_yueliang;
    else if (code == 4 || code == 9)			//阴天
        icon = &icon_yintian;
    else if (code == 5 || code == 6 || code == 7 || code == 8)		//多云
        icon = &icon_duoyun;
    else if (code == 10 || code == 13 || code == 14 || code == 15 || code == 16 || code == 17 || code == 18 || code == 19)	//统一雨天
        icon = &icon_zhongyu;
    else if (code == 11 || code == 12)			//雷阵雨
        icon = &icon_leizhenyu;
    else if (code == 20 || code == 21 || code == 22 || code == 23 || code == 24 || code == 25)	//统一雪天
        icon = &icon_xue;
    else // 扬沙、龙卷风等
        icon = &icon_na;		//特殊天气未知
	LCD_Show_Photo(166,240,icon);
}
