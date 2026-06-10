#include <string.h>
#include "lcd.h"
#include "font.h"

void error_page_display(const char *msg)			//错误页面设计
{
	LCD_Clear(BLACK);		//清屏
	LCD_Show_Photo(40,30,&image_error);	//显示错误页面图片
	
	uint16_t startx = 0;		//判断设置居中
	int len = strlen(msg) * font20.size / 2 ;
	if( len < LCD_WIDTH )
	{
		startx = ( LCD_WIDTH - len ) / 2;
	}else
	{
		startx = 0 ;
	}
		LCD_Show_String(startx,220,msg,WHITE,BLACK,&font20); 	//显示错误信息
}
