#pragma once
#include <stdint.h>
void xDelay_Init(void);
void xDelay_Us(uint32_t n);

#define DELAY_US_1() do { \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
} while(0)
