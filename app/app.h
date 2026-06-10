#ifndef __APP_H__
#define __APP_H__

#define WIFI_SSID 	 "realmeGT5pro"
#define WIFI_PASSWD  "11111111"
#define weather_url  "https://api.seniverse.com/v3/weather/now.json?key=SPmPWR67OXlwayBSU&location=WQPUYY7S4GV2&language=en&unit=c"

void wireless_init(void);
void wireless_wait_connect(void);

void Board_LowLevel_Init(void);
void Board_Init(void);

#endif
