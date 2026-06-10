#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

// !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~

//单个字符的身份证
typedef struct 
{
	const char *name;		// 汉字字符串
	const uint8_t *model;  	// 指向该汉字128字节点阵数据的指针
}font_chinese_t;

//作为管理，检查是中文还是英文
typedef struct
{
	uint16_t size;                  // 字体大小，比如 32 (代表32x32中文，16x32英文)
	const char *ascii_map;
    const uint8_t *ascii_model;     // 指向全体英文 ASCII 字模大数组的指针
    const font_chinese_t *chinese;  // 指向中文字库字典数组的指针
}font_t;

extern const font_t font16_ascii;
extern const font_t font20;
extern const font_t font24;
extern const font_t font32;
extern const font_t font54_temper;
extern const font_t font54_humidity;
extern const font_t font76_time;
#endif
