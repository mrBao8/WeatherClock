#ifndef __SPI_H
#define __SPI_H

#include "font.h"
#include "image.h"

/*颜色*/
#define RED   0xF800	//标准纯色
#define GREEN 0x07E0 
#define BLUE  0x001F
#define WHITE 0xFFFF
#define BLACK 0x0000

#define COLOR_WARM_CREAM      0xFFDE   // 暖奶油白
#define COLOR_MINT       0x57EA   // 薄荷绿（代表清新、生机、正在加载）
#define COLOR_SUNNY      0xFD20   // 向日葵黄（温暖、明亮、极其适合天气）
#define COLOR_CORAL      0xF34C   // 珊瑚粉橙（有活力但不刺眼，代表温馨）
#define COLOR_SKYBLUE    0x5D1C   // 科技天蓝（清爽不深沉，最适合WiFi）
#define COLOR_PURPLE     0xA298   // 薰衣草紫（现代潮流感，适合做独特标识）
#define COLOR_SILVER     0xD69A   // 极简银灰（高档过渡色，适合用于单位或提示）
#define COLOR_AMBER_GOLD      0xFD60   // 琥珀高级金
#define COLOR_DARK_CHARCOAL   0x2B56   // 高级炭黑

#define COLOR_TITANIUM_GRAY   0xA514   // 钛金灰（比之前的银灰深了三档，清晰可辨，高级感十足）
#define COLOR_MEDIUM_GRAY     0x8410   // 标准中灰（最规矩的灰色，在白底上对比度刚刚好，不刺眼）
#define COLOR_COOL_SLATE      0x7412   // 冷石板灰（带一点点高级蓝调的深灰，UI设计大师的最爱）
#define COLOR_CHARCOAL_GRAY   0x528A   // 炭黑灰（非常深的灰色，极其沉稳，适合想要字迹清晰但不想用纯黑的场景）

// 注意：宽和高根据你的真实屏幕修改，常见的是 240x240 或 240x320
#define LCD_WIDTH  240
#define LCD_HEIGHT 320 


/*定义引脚*/
#define LCD_SCK_Port GPIOB
#define LCD_SCK_Pin GPIO_Pin_13

#define LCD_SDA_Port GPIOB
#define LCD_SDA_Pin GPIO_Pin_15

#define LCD_CS_Port GPIOD
#define LCD_CS_Pin GPIO_Pin_12
#define LCD_CS_High() GPIO_SetBits(GPIOD,GPIO_Pin_12)	//空闲状态
#define LCD_CS_Low() GPIO_ResetBits(GPIOD,GPIO_Pin_12) //开始通信

#define LCD_DC_Port GPIOD
#define LCD_DC_Pin GPIO_Pin_10
#define LCD_DC_Data() GPIO_SetBits(GPIOD,GPIO_Pin_10)		//参数
#define LCD_DC_Cmd() GPIO_ResetBits(GPIOD,GPIO_Pin_10)	//命令

#define LCD_RST_Port GPIOD
#define LCD_RST_Pin GPIO_Pin_11
#define LCD_RST_Set() GPIO_SetBits(GPIOD,GPIO_Pin_11)	//拉高开始工作
#define LCD_RST_Clr() GPIO_ResetBits(GPIOD,GPIO_Pin_11)	//复位中


void LCD_Init(void);
void LCD_AddressSet(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);void LCD_Fill_Color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LCD_Clear(uint16_t color);
void LCD_Show_String(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc, const font_t *font);
void LCD_Show_Photo(uint16_t x, uint16_t y, const image_t *image);
#endif
