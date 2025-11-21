#pragma once

#define DEV_INFO 1

#ifdef DEV_INFO
#define PRINT(format, ...) printf(format, ##__VA_ARGS__)
#define PRINTB(byte) print_binary(byte)
#else
#define PRINT(X...)
#define PRINTB(X...)
#endif

#define DELAY_US_1() do { \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
__asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop;"); \
} while(0)