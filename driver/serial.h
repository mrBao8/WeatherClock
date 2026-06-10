#ifndef _SERIAL_H
#define _SERIAL_H

#include "stm32f4xx.h"                  // Device header
#include <stdio.h>

void Usart1_Init(void);
void Usart1_write(const char str[]);
int fputc(int ch, FILE *stream);
uint8_t usart_receive(void);

#endif
