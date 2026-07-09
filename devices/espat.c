#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "espat.h"
#include "delay.h"

#define ESP_AT_DEBUG    0
#define ESP_AT_BUSY_RETRY_TIMES      2
#define ESP_AT_BUSY_RETRY_DELAY_MS   300
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

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

static const at_ack_match_t at_ack_matches[] =
{
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy p...\r\n"},
    {AT_ACK_READY, "ready\r\n"},
};

static char rxbuf[1024];
static char txbuf[384];
static char *rxline;
static volatile uint32_t rxlen;
static volatile at_ack_t rxack;
static SemaphoreHandle_t at_ack_semaphore;

static void esp_at_usart_flush_rx(void)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET)
    {
        (void)USART_ReceiveData(USART2);
    }
}

static void esp_at_usart_start_receive(void)
{
    USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
    esp_at_usart_flush_rx();
    rxlen = 0;
    rxbuf[0] = '\0';
    rxline = rxbuf;
    rxack = AT_ACK_NONE;
    while (at_ack_semaphore && xSemaphoreTake(at_ack_semaphore, 0) == pdPASS);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
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
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}

void esp_at_usart_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
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
    if (at_ack_semaphore == NULL)
        return AT_ACK_NONE;

    if (xSemaphoreTake(at_ack_semaphore, pdMS_TO_TICKS(timeout)) == pdPASS)
        return rxack;

    return AT_ACK_NONE;
}

bool esp_at_write_command(const char *command, uint32_t timeout)
{
    at_ack_t ack;
    uint8_t retry;

    for (retry = 0; retry <= ESP_AT_BUSY_RETRY_TIMES; retry++)
    {
        esp_at_usart_start_receive();

#if ESP_AT_DEBUG
        printf("[DEBUG] Send: %s\n", command);
#endif

        esp_at_usart_write(command);
        ack = esp_at_usart_wait_receive(timeout);

#if ESP_AT_DEBUG
        printf("[DEBUG] Response:\n%s\n", rxbuf);
#endif

        if (ack == AT_ACK_OK)
            return true;

        if (ack != AT_ACK_BUSY)
            return false;

        vTaskDelay(pdMS_TO_TICKS(ESP_AT_BUSY_RETRY_DELAY_MS));
    }

    return false;
}

bool esp_at_wait_ready(uint32_t timeout)
{
    esp_at_usart_start_receive();
    return esp_at_usart_wait_receive(timeout) == AT_ACK_READY;
}

bool esp_at_init(void)
{
    if (at_ack_semaphore == NULL)
    {
        at_ack_semaphore = xSemaphoreCreateBinary();
        configASSERT(at_ack_semaphore);
    }

    esp_at_usart_init();

    esp_at_write_command("AT", 100);
    if (!esp_at_write_command("AT", 100))
        return false;

    return true;
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
    char cmd[128];
    int len;

    if (ssid == NULL || pwd == NULL)
        return false;

    len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pwd);
    if (mac)
        snprintf(cmd + len, sizeof(cmd) - len, ",\"%s\"", mac);

    return esp_at_write_command(cmd, 5000);
}

static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)
{
    int wifi_state;

    response = strstr(response, "+CWSTATE:");
    if (response == NULL)
        return false;

    if (sscanf(response, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2)
        return false;

    info->connected = (wifi_state == 2);
    return true;
}

static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info)
{
    response = strstr(response, "+CWJAP:");
    if (response == NULL)
        return false;

    if (sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->bssid, &info->channel, &info->rssi) != 4)
        return false;

    return true;
}

bool esp_at_get_wifi_info(esp_wifi_info_t *info)
{
    memset(info, 0, sizeof(*info));

    if (!esp_at_write_command("AT+CWSTATE?", 2000))
        return false;

    if (!parse_cwstate_response(esp_at_get_response(), info))
        return false;

    if (!info->connected)
    {
        strcpy(info->ssid, "wifi lost");
        return true;
    }

    return true;
}

bool wifi_is_connected(void)
{
    esp_wifi_info_t info;

    if (esp_at_get_wifi_info(&info))
        return info.connected;

    return false;
}

bool esp_at_sntp_init(void)
{
    return esp_at_write_command("AT+CIPSNTPCFG=1,8", 2000);
}

static uint8_t month_str_to_num(const char *month_str)
{
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    for (uint8_t i = 0; i < 12; i++)
    {
        if (strcmp(month_str, months[i]) == 0)
            return i + 1;
    }

    return 0;
}

static uint8_t weekday_str_to_num(const char *weekday_str)
{
    const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

    for (uint8_t i = 0; i < 7; i++)
    {
        if (strcmp(weekday_str, weekdays[i]) == 0)
            return i + 1;
    }

    return 0;
}

static bool parse_cipsntptime_response(const char *response, esp_date_time_t *date)
{
    char weekday_str[8];
    char month_str[4];

    response = strstr(response, "+CIPSNTPTIME:");
    if (response == NULL)
        return false;

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
    int transport_type = (strncmp(url, "https://", 8) == 0) ? 2 : 1;

    snprintf(txbuf, sizeof(txbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,%d", url, transport_type);
    if (!esp_at_write_command(txbuf, 10000))
        return NULL;

    return esp_at_get_response();
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {
        char ch = (char)USART_ReceiveData(USART2);

        if (rxlen < sizeof(rxbuf) - 1)
        {
            rxbuf[rxlen++] = ch;
            rxbuf[rxlen] = '\0';

            if (ch == '\n')
            {
                at_ack_t ack = match_internal_ack(rxline);
                if (ack != AT_ACK_NONE)
                {
                    BaseType_t higher_priority_task_woken = pdFALSE;
                    rxack = ack;
                    if (at_ack_semaphore != NULL)
                    {
                        xSemaphoreGiveFromISR(at_ack_semaphore, &higher_priority_task_woken);
                        portYIELD_FROM_ISR(higher_priority_task_woken);
                    }
                }
                rxline = rxbuf + rxlen;
            }
        }
        else
        {
            rxbuf[sizeof(rxbuf) - 1] = '\0';
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}
