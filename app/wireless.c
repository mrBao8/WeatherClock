#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "espat.h"
#include "DELAY.H"
#include "weather.h"
#include "page.h"
#include "app.h"

// 定义一个全局变量，用来寄存开机第一次抓到的天气数据
weather_info_t g_first_weather = { 0 };

void wireless_init(void)
{
    uint8_t sync_success = 0;  // 【核心修复】：挪到最顶端声明，防止 Keil 的作用域报警
    uint8_t at_ready = 0;
    uint8_t wifi_connected = 0;
	
    // =========================================================================
    // 阶段一：欢迎页面静止期（此时屏幕亮着“天气时钟 Loading...”）
    // =========================================================================
    cpu_delay_ms(1500); // 默默等待 ESP32 芯片硬件通电、加载内核
    
    while (!esp_at_init())
    {
        at_ready++;
        printf("[AT] init retry %d times\n", at_ready);
        cpu_delay_ms(300);
        if (at_ready >= 5)
        {
            printf("[AT] hardware init failed\n");
            error_page_display("ESP32 Wakeup Fail"); // 只有硬件完全不响应才报这个错
            goto err_loop;
        }
    }
    printf("[AT] base protocol inited\n");
    
    // =========================================================================
    // 阶段二：网络连接攻坚期（立刻切到 WiFi 连接提示页，绝不在欢迎页盲等）
    // =========================================================================
    wifi_page_display(); // 优雅切换，屏幕亮起大无线图标和 "realmeGT5pro 连接中..."
    
    printf("[WIFI] sending connect command...\n");
    esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWD, NULL); // 强行下发连接热点指令
    
    for (uint32_t i = 0; i < 50; i++)
    {
        cpu_delay_ms(200); // 每 200ms 盘问一次状态，总共死磕 10 秒
        esp_wifi_info_t wifi = { 0 };
        if (esp_at_get_wifi_info(&wifi) && wifi.connected)
        {
            printf("[WIFI] wifi Connected!\n");
            printf("[WIFI] SSID: %s, RSSI: %d\n", wifi.ssid, wifi.rssi);
            wifi_connected = 1;
            break; // 抓到网络，成功破局！
        }
        printf("[WIFI] authenticating...\n");
    }
    
    if (!wifi_connected)
    {
        printf("[WIFI] Connection timeout! Switch to OFFLINE mode...\n");
        // 断网不自杀,强制给个离线提示，等 2 秒直接 return 退出，放行进入主页
        error_page_display("Running Offline"); 
        cpu_delay_ms(2000);
		
		goto err_loop;
    }
    
    // =========================================================================
    // 阶段三：网络服务激活与数据抓取（此时屏幕依然静稳地停留在 WiFi 连接页）
    // =========================================================================
    if (!esp_at_wifi_init())
    {
        printf("[WIFI] service init failed\n");
        error_page_display("WIFI Service Fail");
        goto err_loop;
    }
    
    if (!esp_at_sntp_init())
    {
        printf("[SNTP] service init failed\n");
        error_page_display("SNTP Service Fail");
        goto err_loop;
    }
    printf("[SNTP] inited\n");
    
    // 开始抓取时间和天气，最多容错重试 3 次，防止偶发性丢包
    sync_success = 0; 
    for (uint8_t retry = 0; retry < 3; retry++)
    {
        esp_date_time_t date = { 0 };
        if (!esp_at_sntp_get_time(&date) || date.year <= 2000)
        {
            printf("[SNTP] get time failed, retry...\n");
            cpu_delay_ms(1000);
            continue;
        }
        
        const char *weather_http_response = esp_at_http_get(weather_url);
        if (weather_http_response == NULL)
        {
            printf("[WEATHER] http req failed, retry...\n");
            cpu_delay_ms(1000);
            continue;
        }
        
        weather_info_t weather = { 0 };
        if (!parse_seniverse_response(weather_http_response, &weather))
        {
            printf("[WEATHER] parse json failed, retry...\n");
            cpu_delay_ms(1000);
            continue;
        }
        
        printf("[WEATHER] %s, %s, %.1f\n", weather.city, weather.weather, weather.temperature);
		
		rtc_date_time_t rtc_clock;
        rtc_clock.year    = date.year;
        rtc_clock.month   = date.month;
        rtc_clock.day     = date.day;
        rtc_clock.hour    = date.hour;
        rtc_clock.minute  = date.minute;
        rtc_clock.second  = date.second;
        rtc_clock.weekday = date.weekday;
        rtc_set_time(&rtc_clock); 

        //把开机抓到的天气存入全局变量，留给主页秒刷
        memcpy(&g_first_weather, &weather, sizeof(weather_info_t));
		
        sync_success = 1;
        break; // 核心数据全部拿到，直接跳出重试循环
    }
    
		if (!sync_success)
    {
        error_page_display("Net Data Sync Fail");
        goto err_loop; 
    }
    
    return; // 退出显示主页面

err_loop:
    printf("[SYSTEM] Launching Offline Mode...\n");
    
    // 伪造一套安全的离线天气数据，防止主页由于空指针闪烁乱码
    g_first_weather.weather_code = -1; // 触发 N/A 图标
    g_first_weather.temperature = 0.0f;
    strcpy(g_first_weather.city, "NoNet");
    // 这样上面任何一个阶段网络挂了，goto 飞到这里消完毒，立刻安全放行进主页！
    return; 
}
