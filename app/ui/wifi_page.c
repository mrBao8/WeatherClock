#include <string.h>
#include "page.h"
#include "lcd.h"
#include "app.h"

void wifi_page_display(void)			//wifi页面设计
{
	static const char *ssid =WIFI_SSID;

	uint16_t x_wifi = (LCD_WIDTH - 68) / 2;
	
    // 2. 【WIFI_SSID】动态名称居中：用 font24。每个英文字符实际占用 12 + 1 = 13 像素步进！
	
    int len_ssid = strlen(ssid) * 9;
    uint16_t x_ssid = 0;
    if (len_ssid < LCD_WIDTH)
    {
        x_ssid = (LCD_WIDTH - len_ssid) / 2;
    }
    else
    {
        x_ssid = 0; // 如果热点名长得逆天，贴左边边缘开始打印
    }
	
	uint16_t x_connecting = (LCD_WIDTH - 111) / 2;
	
	LCD_Clear(BLACK);		//清屏
	LCD_Show_Photo(25,15,&image_wifi);	//显示wifi页面图片
	LCD_Show_String(x_wifi,187,"WIFI",COLOR_MINT,BLACK,&font32);
	LCD_Show_String(x_ssid,230,WIFI_SSID,WHITE,BLACK,&font24);
	LCD_Show_String(x_connecting,260,"连接中...",COLOR_SKYBLUE,BLACK,&font24);
}
