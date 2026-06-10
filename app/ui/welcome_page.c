#include "lcd.h"
#include "font.h"

void welcome_page_display(void)			//欢迎页面设计
{
	LCD_Clear(BLACK);		//清屏
	LCD_Show_Photo(30,10,&image_wel);	//显示欢迎页面图片
	
	uint16_t x_title = (LCD_WIDTH - 128) / 2 ;	//四个32号汉字，4*32 = 128
	
	LCD_Show_String(x_title,200,"天气时钟",COLOR_SUNNY,BLACK,&font32);
	
	uint16_t x_loading = (LCD_WIDTH - 119) / 2;
	LCD_Show_String(x_loading,250,"Loading",WHITE,BLACK,&font32);
}
