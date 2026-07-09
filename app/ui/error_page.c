#include <string.h>
#include "ui.h"

void error_page_display(const char *msg)			//错误页面设计
{
	ui_clear(BLACK);		//清屏
	ui_show_photo(40,30,&image_error);	//显示错误页面图片
	
	uint16_t startx = 0;		//判断设置居中
	int len = strlen(msg) * font20.size / 2 ;
	if( len < UI_WIDTH )
	{
		startx = ( UI_WIDTH - len ) / 2;
	}else
	{
		startx = 0 ;
	}
		ui_show_string(startx,220,msg,WHITE,BLACK,&font20); 	//显示错误信息
}
