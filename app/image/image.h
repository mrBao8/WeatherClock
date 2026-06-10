#ifndef __IMAGE_H
#define __IMAGE_H

#include <stdint.h>

typedef struct 
{
	uint16_t width;
	uint16_t height;
	const uint8_t *data;
}image_t;

extern const image_t image_wel;
extern const image_t image_error;
extern const image_t image_wifi;
extern const image_t icon_wifi;
extern const image_t icon_yintian;
extern const image_t icon_na;
extern const image_t icon_leizhenyu;
extern const image_t icon_qing;
extern const image_t icon_duoyun;
extern const image_t icon_xue;
extern const image_t icon_zhongyu;
extern const image_t icon_yueliang;
extern const image_t icon_wenduji;
#endif
