#include "stm32f4xx.h"                  // Device header
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "espat.h"
#include "delay.h"


#define ESP_AT_DEBUG    1			//日志调试开关
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
	
static char rxbuf[1024];
static void esp_at_usart_write(const char *data);
bool esp_at_wait_ready(uint32_t timeout);
bool esp_at_write_command(const char *command, uint32_t timeout);

typedef enum			
{
    AT_ACK_NONE,
    AT_ACK_OK,
    AT_ACK_ERROR,
    AT_ACK_BUSY,
    AT_ACK_READY,
} at_ack_t;

typedef struct
{
    at_ack_t ack;
    const char *string;
} at_ack_match_t;

static const at_ack_match_t at_ack_matches[] = 		//为回复指令取名字
{
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy p...\r\n"},
    {AT_ACK_READY, "ready\r\n"},
};

static at_ack_t esp_at_usart_wait_receive(uint32_t timeout);

void esp_at_usart_init(void)						
{
	USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);	
	
	USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	
	//GPIO引脚复用为串口*必填*
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
	
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_Init(USART2, &USART_InitStructure);
    USART_Cmd(USART2, ENABLE);	
}

bool esp_at_init(void)
{
    esp_at_usart_init();
    
    esp_at_write_command("AT", 100);
    if (!esp_at_write_command("AT", 100))
        return false;
//    if (!esp_at_write_command("AT+RESTORE", 2000))
//        return false;
//    if (!esp_at_wait_ready(5000))
//        return false;
    
    return true;
}

static void esp_at_usart_write(const char *data)
{
    while (data && *data)
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
        USART_SendData(USART2, *data++);
    }
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, '\r');
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, '\n');
}

bool esp_at_write_command(const char *command, uint32_t timeout)
{
#if ESP_AT_DEBUG
    printf("[DEBUG] Send: %s\n", command);
#endif

    esp_at_usart_write(command);
    at_ack_t ack = esp_at_usart_wait_receive(timeout);

#if ESP_AT_DEBUG
    printf("[DEBUG] Response:\n%s\n", rxbuf);
#endif

    return ack == AT_ACK_OK;
}

static at_ack_t match_internal_ack(const char *str)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(at_ack_matches); i++)
    {
        if (strcmp(str, at_ack_matches[i].string) == 0)
            return at_ack_matches[i].ack;
    }
    
    return AT_ACK_NONE;
}

static at_ack_t esp_at_usart_wait_receive(uint32_t timeout)
{
    uint32_t rxlen = 0;
    const char *line = rxbuf;
    uint64_t start = cpu_get_ms();
    
    rxbuf[0] = '\0';
    while (rxlen < sizeof(rxbuf) - 1)
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET)
        {
            if (cpu_get_ms() - start >= timeout)
                return AT_ACK_NONE;
        }
        rxbuf[rxlen++] = USART_ReceiveData(USART2);
        rxbuf[rxlen] = '\0';
        if (rxbuf[rxlen - 1] == '\n')
        {
            at_ack_t ack = match_internal_ack(line);
            if (ack != AT_ACK_NONE)
                return ack;
            line = rxbuf + rxlen;
        }
    }
    
    return AT_ACK_NONE;
}

bool esp_at_wait_ready(uint32_t timeout)
{
    return esp_at_usart_wait_receive(timeout) == AT_ACK_READY;
}


const char *esp_at_get_response(void)
{
    return rxbuf;
}

bool esp_at_wifi_init(void)
{
    return esp_at_write_command("AT+CWMODE=1", 2000);
}

bool esp_at_connect_wifi(const char *ssid, const char *pwd, const char *mac)
{
    if (ssid == NULL || pwd == NULL)
        return false;
    
    char cmd[128];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (mac)
        snprintf(cmd + len, sizeof(cmd) - len, ",\"%s\"", mac);
    
    return esp_at_write_command(cmd, 5000);
}

static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)
{
//    AT+CWSTATE?
//    +CWSTATE:2,"realmeGT5pro"

//    OK
	response = strstr(response, "+CWSTATE:");
	if (response == NULL)
		return false;
	
	int wifi_state;
	if (sscanf(response, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2)
		return false;
	
	info->connected = (wifi_state == 2);
	
	return true;
}

static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info)
{
//    AT+CWJAP?
//    +CWJAP:"realmeGT5pro","da:b5:3a:e3:2f:60",9,-48,0,1,3,0,1

//    OK
	response = strstr(response, "+CWJAP:");
	if (response == NULL)
		return false;
	
	if (sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->bssid, &info->channel, &info->rssi) != 4)
		return false;
	
	return true;
}

bool esp_at_get_wifi_info(esp_wifi_info_t *info)
{
    if (!esp_at_write_command("AT+CWSTATE?", 2000))
        return false;
    
    if (!parse_cwstate_response(esp_at_get_response(), info))
        return false;
    if (!info->connected)
    {
        strcpy(info->ssid, "wifi lost");
        return true; // 返回 true，告诉主循环确认断网
    }
    if (!esp_at_write_command("AT+CWJAP?", 2000))
        return false;
    
    if (!parse_cwjap_response(esp_at_get_response(), info))
        return false;
    
    return true;
}

bool wifi_is_connected(void)
{
    esp_wifi_info_t info;
    if (esp_at_get_wifi_info(&info))
    {
        return info.connected;
    }
    return false;
}

bool esp_at_sntp_init(void)
{
    if (!esp_at_write_command("AT+CIPSNTPCFG=1,8", 2000))
        return false;
    
    return true;
}

static uint8_t month_str_to_num(const char *month_str)
{
	const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	for (uint8_t i = 0; i < 12; i++)
	{
		if (strcmp(month_str, months[i]) == 0)
		{
			return i + 1;
		}
	}
	return 0;
}

static uint8_t weekday_str_to_num(const char *weekday_str)
{
	const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	for (uint8_t i = 0; i < 7; i++) {
		if (strcmp(weekday_str, weekdays[i]) == 0)
		{
			return i + 1;
		}
	}
	return 0;
}

static bool parse_cipsntptime_response(const char *response, esp_date_time_t *date)
{
//	AT+CIPSNTPTIME?
//	+CIPSNTPTIME:Sun May 10 14:07:19 2026
//	OK
	char weekday_str[8];
	char month_str[4];
	response = strstr(response, "+CIPSNTPTIME:");
	if (sscanf(response, "+CIPSNTPTIME:%3s %3s %hhu %hhu:%hhu:%hhu %hu", 
			   weekday_str, month_str, 
			   &date->day, &date->hour, &date->minute, &date->second, &date->year) != 7)
		return false;
	
	date->weekday = weekday_str_to_num(weekday_str);
	date->month = month_str_to_num(month_str);
	
	return true;
}

bool esp_at_sntp_get_time(esp_date_time_t *date)
{
    if (!esp_at_write_command("AT+CIPSNTPTIME?", 2000))
        return false;
    
    if (!parse_cipsntptime_response(esp_at_get_response(), date))
        return false;
    
    return true;
}

const char *esp_at_http_get(const char *url)
{
//    AT+HTTPCLIENT=2,1,"https://api.seniverse.com/v3/weather/now.json?key=SPmPWR67OXlwayBSU&location=WQPUYY7S4GV2&language=en&unit=c",,,2
//    +HTTPCLIENT:261,{"results":[{"location":{"id":"WQPUYY7S4GV2","name":"Luoyang","country":"CN","path":"Luoyang,Luoyang,Henan,China","timezone":"Asia/Shanghai","timezone_offset":"+08:00"},"now":{"text":"Light rain","code":"13","temperature":"18"},"last_update":"2026-05-16T17:10:18+08:00"}]}
//    OK
    char *txbuf = rxbuf;
    snprintf(txbuf, sizeof(rxbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2", url);
    bool ret = esp_at_write_command(txbuf, 5000);
    return ret ? esp_at_get_response() : NULL;
}
