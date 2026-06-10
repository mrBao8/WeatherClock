#ifndef __DELAY_H
#define __DELAY_H

typedef void (*systick_callback_t) (void);

void cpu_tick_init(void);
void SysTick_Handle(void);
void cpu_delay_us( uint32_t us);
void cpu_delay_ms( uint32_t ms);
uint64_t cpu_get_us(void);
uint64_t cpu_get_ms(void);
uint64_t cpu_now(void);

void systick_register_callback(systick_callback_t cb);

#endif
