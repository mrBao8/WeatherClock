#ifndef __PAGE_H__
#define __PAGE_H__

#include "rtc.h"

void welcome_page_display(void);
void error_page_display(const char *msg);
void wifi_page_display(void);
void main_page_display(void);
void main_page_refresh_wifi_ssid(const char *ssid);
void main_page_refresh_date(const rtc_date_time_t *date);
void main_page_refresh_time(const rtc_date_time_t *time);
void main_page_refresh_weather_icon(const int code);
void main_page_refresh_outdoor_temper(float temp);
void main_page_refresh_inner_humidity(float humi);
void main_page_refresh_inner_temper(float temp);

#endif
