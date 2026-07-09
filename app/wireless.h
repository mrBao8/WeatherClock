#ifndef __WIRELESS_H__
#define __WIRELESS_H__

#include "weather.h"

#define WIFI_SSID    "YOUR_WIFI_SSID"
#define WIFI_PASSWD  "YOUR_WIFI_PASSWORD"
#define weather_url  "https://api.seniverse.com/v3/weather/now.json?key=YOUR_SENIVERSE_KEY&location=WQPUYY7S4GV2&language=en&unit=c"

extern weather_info_t g_first_weather;

void wireless_init(void);

#endif
