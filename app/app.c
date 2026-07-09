#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "workqueue.h"
#include "rtc.h"
#include "page.h"
#include "espat.h"
#include "weather.h"
#include "aht20.h"
#include "app.h"
#include "wireless.h"

static TimerHandle_t time_sync_timer;
static TimerHandle_t time_update_timer;
static TimerHandle_t wifi_update_timer;
static TimerHandle_t inner_update_timer;
static TimerHandle_t outdoor_update_timer;

static bool esp_ready = false;
static bool wifi_ready = false;
static bool ui_ready = false;
static bool app_inited = false;
static bool network_started = false;

static void wait_ui_ready(void)
{
    while (!ui_ready)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
static bool rtc_time_is_valid(void)
{
    rtc_date_time_t date;

    rtc_get_time(&date);
    return date.year >= 2020;
}

static void time_update(void)
{
    static rtc_date_time_t last_time = { 0 };
    static rtc_date_time_t last_date = { 0 };
    rtc_date_time_t date;

    rtc_get_time(&date);

    if (date.year < 2020)
        return;

    if (date.hour != last_time.hour ||
        date.minute != last_time.minute ||
        date.second != last_time.second)
    {
        main_page_refresh_time(&date);
        last_time.hour = date.hour;
        last_time.minute = date.minute;
        last_time.second = date.second;
    }

    if (date.year != last_date.year ||
        date.month != last_date.month ||
        date.day != last_date.day ||
        date.weekday != last_date.weekday)
    {
        main_page_refresh_date(&date);
        last_date.year = date.year;
        last_date.month = date.month;
        last_date.day = date.day;
        last_date.weekday = date.weekday;
    }
}

static void time_sync(void)
{
    uint32_t restart_sync_delay = TIME_SYNC_INTERVAL;
    rtc_date_time_t rtc_date = { 0 };
    esp_date_time_t esp_date = { 0 };

    if (!esp_ready || !wifi_ready)
    {
        xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(MINUTES(10)), 0);
        return;
    }

    if (!esp_at_sntp_get_time(&esp_date) || esp_date.year < 2000)
    {
        printf("[SNTP] get time failed, retry in 10min...\n");
        restart_sync_delay = MINUTES(10);
        goto out;
    }

    rtc_date.year = esp_date.year;
    rtc_date.month = esp_date.month;
    rtc_date.day = esp_date.day;
    rtc_date.hour = esp_date.hour;
    rtc_date.minute = esp_date.minute;
    rtc_date.second = esp_date.second;
    rtc_date.weekday = esp_date.weekday;
    rtc_set_time(&rtc_date);
    main_page_refresh_time(&rtc_date);
    main_page_refresh_date(&rtc_date);
    printf("[SNTP] sync success: %04u-%02u-%02u %02u:%02u:%02u\n",
           rtc_date.year, rtc_date.month, rtc_date.day,
           rtc_date.hour, rtc_date.minute, rtc_date.second);

out:
    xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(restart_sync_delay), 0);
}
static void wifi_update(void)
{
    static bool first_run = true;
    static esp_wifi_info_t last_info = { 0 };
    esp_wifi_info_t info = { 0 };

    if (!esp_ready)
        return;

    if (!esp_at_get_wifi_info(&info))
        return;

    wifi_ready = info.connected;

    if (!first_run && info.connected == last_info.connected &&
        strcmp(info.ssid, last_info.ssid) == 0)
    {
        return;
    }

    first_run = false;

    if (info.connected)
        main_page_refresh_wifi_ssid(info.ssid);
    else
    {
        main_page_refresh_wifi_ssid("wifi lost");
        main_page_refresh_outdoor_city("Notnet");
    }

    memcpy(&last_info, &info, sizeof(esp_wifi_info_t));
}
static void inner_update(void)
{
    static float last_temperature = -1000.0f;
    static float last_humidity = -1000.0f;
    static uint8_t fail_count = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;

    if (!aht20_start_measurement() ||
        !aht20_wait_for_measurement() ||
        !aht20_read_measurement(&temperature, &humidity))
    {
        fail_count++;
        if ((fail_count % 10) == 0)
        {
            printf("[AHT20] read failed x%d\n", fail_count);
        }
        if (fail_count >= 30)
        {
            aht20_init();
            fail_count = 0;
        }
        return;
    }

    fail_count = 0;

    if (temperature == last_temperature && humidity == last_humidity)
        return;

    last_temperature = temperature;
    last_humidity = humidity;

    main_page_refresh_inner_temper(temperature);
    main_page_refresh_inner_humidity(humidity);
}

static bool outdoor_fetch_and_refresh(void)
{
    static weather_info_t last_weather = { 0 };
    weather_info_t weather = { 0 };
    const char *weather_http_response;

    if (!esp_ready || !wifi_ready)
        return false;

    weather_http_response = esp_at_http_get(weather_url);
    if (weather_http_response == NULL)
    {
        printf("[WEATHER] http error\n");
        return false;
    }

    if (!parse_seniverse_response(weather_http_response, &weather))
    {
        printf("[WEATHER] parse failed\n");
        return false;
    }

    if (memcmp(&last_weather, &weather, sizeof(weather_info_t)) != 0)
    {
        memcpy(&last_weather, &weather, sizeof(weather_info_t));
    }

    printf("[WEATHER] %s, %s, %.1f\n", weather.city, weather.weather, weather.temperature);
    main_page_refresh_outdoor_city("ÂåÑô");
    main_page_refresh_outdoor_temper(weather.temperature);
    main_page_refresh_weather_icon(weather.weather_code);
    return true;
}
static void outdoor_update(void)
{
    if (outdoor_fetch_and_refresh())
        xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), 0);
    else if (wifi_ready)
        xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(SECONDS(5)), 0);
}
static void network_startup(void)
{
    uint8_t at_ready = 0;

    while (!esp_at_init())
    {
        at_ready++;
        printf("[AT] init retry %d times\n", at_ready);
        vTaskDelay(pdMS_TO_TICKS(300));
        if (at_ready >= 5)
        {
            printf("[AT] hardware init failed, offline mode\n");
            wait_ui_ready();
            main_page_refresh_wifi_ssid("wifi lost");
            main_page_refresh_outdoor_city("Notnet");
            return;
        }
    }

    esp_ready = true;
    printf("[AT] base protocol inited\n");

    esp_at_wifi_init();

    for (uint32_t i = 0; i < 50; i++)
    {
        esp_wifi_info_t wifi = { 0 };

        if (i == 0)
        {
            if (esp_at_get_wifi_info(&wifi) && wifi.connected)
            {
                wifi_ready = true;
                printf("[WIFI] connected\n");
                wait_ui_ready();
                main_page_refresh_wifi_ssid(wifi.ssid);
                break;
            }

            printf("[WIFI] connecting to %s...\n", WIFI_SSID);
            esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWD, NULL);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            if (esp_at_get_wifi_info(&wifi) && wifi.connected)
            {
                wifi_ready = true;
                printf("[WIFI] connected\n");
                wait_ui_ready();
                main_page_refresh_wifi_ssid(wifi.ssid);
                break;
            }
        }
    }

    if (!wifi_ready)
    {
        printf("[WIFI] timeout, offline mode\n");
        wait_ui_ready();
        main_page_refresh_wifi_ssid("wifi lost");
        main_page_refresh_outdoor_city("Notnet");
        return;
    }

    wait_ui_ready();
    vTaskDelay(pdMS_TO_TICKS(100));

    if (esp_at_sntp_init())
        time_sync();

    if (outdoor_fetch_and_refresh())
        xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), 0);
    else
        xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(SECONDS(5)), 0);
}

typedef void (*app_job_t)(void);

static void app_work(void *param)
{
    app_job_t job = (app_job_t)param;
    job();
}

static void work_timer_cb(TimerHandle_t timer)
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(timer);
    workqueue_run(app_work, job);
}

static void app_timer_cb(TimerHandle_t timer)
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(timer);
    job();
}

void app_init(void)
{
    if (app_inited)
        return;

    time_update_timer = xTimerCreate("time update", pdMS_TO_TICKS(TIME_UPDATE_INTERVAL), pdTRUE, time_update, app_timer_cb);
    time_sync_timer = xTimerCreate("sntp", pdMS_TO_TICKS(TIME_SYNC_INTERVAL), pdFALSE, time_sync, work_timer_cb);
    wifi_update_timer = xTimerCreate("wifi", pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), pdTRUE, wifi_update, work_timer_cb);
    inner_update_timer = xTimerCreate("aht20", pdMS_TO_TICKS(INNER_UPDATE_INTERVAL), pdTRUE, inner_update, work_timer_cb);
    outdoor_update_timer = xTimerCreate("weather", pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), pdFALSE, outdoor_update, work_timer_cb);

    configASSERT(time_update_timer);
    configASSERT(time_sync_timer);
    configASSERT(wifi_update_timer);
    configASSERT(inner_update_timer);
    configASSERT(outdoor_update_timer);

    app_inited = true;
}

void app_network_start(void)
{
    if (!app_inited)
        app_init();

    if (network_started)
        return;

    network_started = true;
    workqueue_run(app_work, network_startup);
}

void app_start(void)
{
    if (!app_inited)
        app_init();

    time_update();
    inner_update();
    ui_ready = true;

    xTimerStart(time_update_timer, 0);
    xTimerStart(time_sync_timer, 0);
    xTimerStart(wifi_update_timer, 0);
    xTimerStart(inner_update_timer, 0);
    xTimerStart(outdoor_update_timer, 0);
}
