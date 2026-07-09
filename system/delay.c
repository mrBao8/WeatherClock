#include "stm32f4xx.h"
#include <stdint.h>
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

#define TICKS_PER_US (SystemCoreClock / 1000000UL)

static systick_callback_t systick_callback_g = 0;

static void cpu_cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void cpu_tick_init(void)
{
    cpu_cycle_counter_init();
}

uint64_t cpu_now(void)
{
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0)
    {
        cpu_cycle_counter_init();
    }
    return (uint64_t)DWT->CYCCNT;
}

uint64_t cpu_get_us(void)
{
    return cpu_now() / TICKS_PER_US;
}

uint64_t cpu_get_ms(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        return (uint64_t)xTaskGetTickCount();
    }
    return cpu_get_us() / 1000ULL;
}

void cpu_delay_us(uint32_t us)
{
    uint64_t start = cpu_now();
    uint64_t wait_ticks = (uint64_t)us * TICKS_PER_US;

    while ((cpu_now() - start) < wait_ticks)
    {
    }
}

void cpu_delay_ms(uint32_t ms)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
        return;
    }

    while (ms--)
    {
        cpu_delay_us(1000);
    }
}

void systick_register_callback(systick_callback_t cb)
{
    systick_callback_g = cb;
    (void)systick_callback_g;
}