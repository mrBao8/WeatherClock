#include "ui.h"

void welcome_page_display(void)			//欢迎页面设计
{
	ui_clear(BLACK);		//清屏
	ui_show_photo(30,10,&image_wel);	//显示欢迎页面图片
	
	uint16_t x_title = (UI_WIDTH - 128) / 2 ;	//四个32号汉字，4*32 = 128
	
	ui_show_string(x_title,200,"天气时钟",COLOR_SUNNY,BLACK,&font32);
	
	uint16_t x_loading = (UI_WIDTH - 119) / 2;
	ui_show_string(x_loading,250,"Loading",WHITE,BLACK,&font32);
}
