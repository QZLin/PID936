#pragma once

#define DEV_INFO 1

#ifdef DEV_INFO
#define PRINT(format, ...) printf(format, ##__VA_ARGS__)
#define PRINTB(byte) print_binary(byte)
#else
#define PRINT(X...)
#define PRINTB(X...)
#endif
