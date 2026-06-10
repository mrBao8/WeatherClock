#include "mloop.h"
#include "delay.h"  
#include "rtc.h"
#include "page.h"  
#include "espat.h"
#include "page.h"
#include "weather.h"
#include "aht20.h"
#include <stddef.h>
#include <string.h>

static volatile uint32_t time_sync_delay;
static volatile uint32_t wifi_update_delay;
static volatile uint32_t time_update_delay;
static volatile uint32_t inner_update_delay;
static volatile uint32_t outdoor_update_delay;

extern weather_info_t g_first_weather;

	//cpu每1ms自动调用一次
static void cpu_periodic_callback(void)
{
	if(time_sync_delay > 0) time_sync_delay--;
	if(wifi_update_delay > 0) wifi_update_delay--;
	if(time_update_delay > 0) time_update_delay--;
	if(inner_update_delay > 0) inner_update_delay--;
	if(outdoor_update_delay > 0) outdoor_update_delay--;
}

static void time_sync(void)
{
    uint32_t restart_sync_delay = TIME_SYNC_INTERVAL; // 默认 1 天
    rtc_date_time_t rtc_date = { 0 };
    esp_date_time_t esp_date = { 0 };

    if (!esp_at_sntp_get_time(&esp_date) || esp_date.year < 2000)
    {
        printf("[SNTP] get time failed, retry in 1s...\n");
        restart_sync_delay = SECONDS(1); // 失败了 1 秒后重试
        goto err;
    }
    
    // 转换并写入本地 RTC 芯片
    rtc_date.year = esp_date.year; rtc_date.month = esp_date.month; rtc_date.day = esp_date.day;
    rtc_date.hour = esp_date.hour; rtc_date.minute = esp_date.minute; rtc_date.second = esp_date.second;
    rtc_date.weekday = esp_date.weekday;
    rtc_set_time(&rtc_date);
    
err:
    //【核心改法】：用裸机倒计时赋值，代替 RTOS 的 xTimerChangePeriod！
    time_sync_delay = restart_sync_delay;
}

static void wifi_update(void)
{
    static esp_wifi_info_t last_info = { 0 };
	static bool first_run = true;	//引入初次开机标志
    esp_wifi_info_t info = { 0 };
	
    if (!esp_at_get_wifi_info(&info)) 
		return;
	
    if (!first_run) {
        if (memcmp(&info, &last_info, sizeof(esp_wifi_info_t)) == 0) return;
        if (last_info.connected == info.connected) return;
    }
    first_run = false; // 跑过一次后关闭绿灯
    
    if (info.connected) main_page_refresh_wifi_ssid(info.ssid); 
		else            main_page_refresh_wifi_ssid("wifi lost");
    
    memcpy(&last_info, &info, sizeof(esp_wifi_info_t));
}

static void time_update(void)
{
    static rtc_date_time_t last_date = { 0 };
    rtc_date_time_t date;
    rtc_get_time(&date);
    
    if (date.year < 2020) 
	return;
	
    if (memcmp(&date, &last_date, sizeof(rtc_date_time_t)) == 0) 
	return; // 秒数没变直接拦截
    
    memcpy(&last_date, &date, sizeof(rtc_date_time_t));
    
    main_page_refresh_time(&date); 
    main_page_refresh_date(&date);
}

static void inner_update(void)
{
    static float last_temperature, last_humidity;
    
    if (!aht20_start_measurement())
    {
        printf("[AHT20] start measurement failed\n");
        return;
    }
    
    if (!aht20_wait_for_measurement())
    {
        printf("[AHT20] wait for measurement failed\n");
        return;
    }
    
    float temperature = 0.0f, humidity = 0.0f;
    
    if (!aht20_read_measurement(&temperature, &humidity))
    {
        printf("[AHT20] read measurement failed\n");
        return;
    }
    
    if (temperature == last_temperature && humidity == last_humidity)
    {
        return;
    }
    
    last_temperature = temperature;
    last_humidity = humidity;
    
    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature, humidity);
    main_page_refresh_inner_temper(temperature);
    main_page_refresh_inner_humidity(humidity);
}

static void outdoor_update(void)
{
    static weather_info_t last_weather = { 0 };
    
    weather_info_t weather = { 0 };
    const char *weather_url = "https://api.seniverse.com/v3/weather/now.json?key=SPmPWR67OXlwayBSU&location=WQPUYY7S4GV2&language=en&unit=c";
    const char *weather_http_response = esp_at_http_get(weather_url);
    if (weather_http_response == NULL)
    {
        printf("[WEATHER] http error\n");
        return;
    }
    
    if (!parse_seniverse_response(weather_http_response, &weather))
    {
        printf("[WEATHER] parse failed\n");
        return;
    }
    
    if (memcmp(&last_weather, &weather, sizeof(weather_info_t)) == 0)
    {
        return;
    }
    
    memcpy(&last_weather, &weather, sizeof(weather_info_t));
    printf("[WEATHER] %s, %s, %.1f\n", weather.city, weather.weather, weather.temperature);
    
    main_page_refresh_outdoor_temper(weather.temperature);
    main_page_refresh_weather_icon(weather.weather_code);
}

void main_loop_init(void)
{
	systick_register_callback(cpu_periodic_callback);	//注册
    time_update_delay  =0;
    inner_update_delay =0;

	outdoor_update_delay = OUTDOOR_UPDATE_INTERVAL; // 1分钟后再抓
    wifi_update_delay    = WIFI_UPDATE_INTERVAL;    // 5秒后再查
    time_sync_delay       = TIME_SYNC_INTERVAL;      // 1天后再对时
} 

void main_loop_proc(void)
{
    // 1. 只要 1 秒时间到，就去读取并核对 RTC，变了就局部刷屏幕
    if (time_update_delay == 0)
    {
        time_update_delay = TIME_UPDATE_INTERVAL; // 重新上膛 1 秒
        time_update();
    }
    
    // 2. 只要 3 秒时间到，读取传感器，变了就刷
    if (inner_update_delay == 0)
    {
        inner_update_delay = INNIR_UPDATE_INTERVAL;
        inner_update();
    }
    
    // 3. 只要 1 分钟时间到，去网上拉天气
    if (outdoor_update_delay == 0)
    {
        outdoor_update_delay = OUTDOOR_UPDATE_INTERVAL;
        outdoor_update();
    }
    
    // 4. 只要 5 秒时间到，查 WiFi 状态
    if (wifi_update_delay == 0)
    {
        wifi_update_delay = WIFI_UPDATE_INTERVAL;
        wifi_update();
    }
    
    // 5. 天级对时。注意：它的重装上膛在 time_sync_proc() 的内部由于 err 机制动态控制了，这里不需要重装！
    if (time_sync_delay == 0)
    {
        time_sync(); 
    }
}
