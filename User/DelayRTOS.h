#pragma once

#define DELAY_US_1() do { \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
} while(0)
