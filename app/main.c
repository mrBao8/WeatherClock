#include "page.h"
#include "app.h"
#include "weather.h"
#include "wireless.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include "workqueue.h"
#include <stdio.h>

extern weather_info_t g_first_weather;

extern void Board_LowLevel_Init(void);
extern void Board_Init(void);

static void main_init_task(void *param)
{
    (void)param;

    Board_Init();
    ui_init();
    printf("[SYSTEM] WeatherClock FreeRTOS start\n");

    welcome_page_display();
    vTaskDelay(pdMS_TO_TICKS(2000));

    wireless_init();
	
    app_init();
	
    app_network_start();
    vTaskDelay(pdMS_TO_TICKS(2000));

    main_page_display();
    main_page_refresh_outdoor_temper(g_first_weather.temperature);
    main_page_refresh_weather_icon(g_first_weather.weather_code);

    app_start();
    printf("[SYSTEM] main tasks started\n");

    vTaskDelete(NULL);
}

int main(void)
{
    Board_LowLevel_Init();
	
    workqueue_init();

    xTaskCreate(main_init_task, "init", 1024, NULL, 9, NULL);
    vTaskStartScheduler();

    while (1)
    {
    }
}
