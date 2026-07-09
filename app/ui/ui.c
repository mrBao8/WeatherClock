#pragma anon_unions
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ui.h"

typedef enum
{
    UI_ACTION_CLEAR,
    UI_ACTION_FILL_COLOR,
    UI_ACTION_SHOW_STRING,
    UI_ACTION_SHOW_PHOTO,
} ui_action_t;

typedef struct
{
    ui_action_t action;
    union
    {
        struct
        {
            uint16_t color;
        } clear;
        struct
        {
            uint16_t x1;
            uint16_t y1;
            uint16_t x2;
            uint16_t y2;
            uint16_t color;
        } fill_color;
        struct
        {
            uint16_t x;
            uint16_t y;
            char *str;
            uint16_t fc;
            uint16_t bc;
            const font_t *font;
        } show_string;
        struct
        {
            uint16_t x;
            uint16_t y;
            const image_t *image;
        } show_photo;
    };
} ui_message_t;

static QueueHandle_t ui_queue;

static void ui_task(void *param)
{
    ui_message_t msg;

    (void)param;

    while (1)
    {
        xQueueReceive(ui_queue, &msg, portMAX_DELAY);

        switch (msg.action)
        {
        case UI_ACTION_CLEAR:
            LCD_Clear(msg.clear.color);
            break;

        case UI_ACTION_FILL_COLOR:
            LCD_Fill_Color(msg.fill_color.x1, msg.fill_color.y1,
                           msg.fill_color.x2, msg.fill_color.y2,
                           msg.fill_color.color);
            break;

        case UI_ACTION_SHOW_STRING:
            LCD_Show_String(msg.show_string.x, msg.show_string.y,
                            msg.show_string.str,
                            msg.show_string.fc, msg.show_string.bc,
                            msg.show_string.font);
            vPortFree(msg.show_string.str);
            break;

        case UI_ACTION_SHOW_PHOTO:
            LCD_Show_Photo(msg.show_photo.x, msg.show_photo.y,
                           msg.show_photo.image);
            break;

        default:
            printf("[UI] unknown action: %d\n", msg.action);
            break;
        }
    }
}

void ui_init(void)
{
    if (ui_queue != NULL)
        return;

    ui_queue = xQueueCreate(32, sizeof(ui_message_t));
    configASSERT(ui_queue);
    xTaskCreate(ui_task, "ui", 1024, NULL, 8, NULL);
}

void ui_clear(uint16_t color)
{
    ui_message_t msg;

    configASSERT(ui_queue);
    msg.action = UI_ACTION_CLEAR;
    msg.clear.color = color;

    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}

void ui_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    ui_message_t msg;

    configASSERT(ui_queue);
    msg.action = UI_ACTION_FILL_COLOR;
    msg.fill_color.x1 = x1;
    msg.fill_color.y1 = y1;
    msg.fill_color.x2 = x2;
    msg.fill_color.y2 = y2;
    msg.fill_color.color = color;

    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}

void ui_show_string(uint16_t x, uint16_t y, const char *str, uint16_t fc, uint16_t bc, const font_t *font)
{
    ui_message_t msg;
    char *str_copy;

    configASSERT(ui_queue);

    str_copy = pvPortMalloc(strlen(str) + 1);
    if (str_copy == NULL)
    {
        printf("[UI] string malloc failed: %s\n", str);
        return;
    }
    strcpy(str_copy, str);

    msg.action = UI_ACTION_SHOW_STRING;
    msg.show_string.x = x;
    msg.show_string.y = y;
    msg.show_string.str = str_copy;
    msg.show_string.fc = fc;
    msg.show_string.bc = bc;
    msg.show_string.font = font;

    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}

void ui_show_photo(uint16_t x, uint16_t y, const image_t *image)
{
    ui_message_t msg;

    configASSERT(ui_queue);
    msg.action = UI_ACTION_SHOW_PHOTO;
    msg.show_photo.x = x;
    msg.show_photo.y = y;
    msg.show_photo.image = image;

    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}
