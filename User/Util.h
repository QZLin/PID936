#pragma once

#define DEV_INFO 1

#ifdef DEV_INFO
#define PRINT(format, ...) printf(format, ##__VA_ARGS__)
#define PRINTB(byte) print_binary(byte)
#else
#define PRINT(X...)
#define PRINTB(X...)
#endif

#define INT __attribute__((interrupt()))
#define vTaskDelayMs(t) vTaskDelay(pdMS_TO_TICKS(t))

#define SetMask(target,mask) target |= mask
#define UnSetMask(target,mask) target &= ~mask
#define SetLED(mask) \
    if(xSemaphoreTake(x595Mutex, portMAX_DELAY)) {\
        SetMask(hc595Data,mask);\
        xSemaphoreGive(x595Mutex);\
    }
#define UnSetLED(mask) \
    if(xSemaphoreTake(x595Mutex, portMAX_DELAY)) {\
        UnSetMask(hc595Data,mask);\
        xSemaphoreGive(x595Mutex);\
    }
