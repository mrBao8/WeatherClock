#include <stdio.h>
#include <string.h>
#include "page.h"
#include "wireless.h"

weather_info_t g_first_weather = { 0 };

static void set_offline_weather(void)
{
    memset(&g_first_weather, 0, sizeof(g_first_weather));
    g_first_weather.weather_code = -1;
    g_first_weather.temperature = 0.0f;
    strcpy(g_first_weather.city, "NoNet");
}

void wireless_init(void)
{
    set_offline_weather();
    wifi_page_display();
}
