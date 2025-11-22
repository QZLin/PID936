#pragma once

#include <stdbool.h>

#include "ch32x035_conf.h"

typedef struct {
    int16_t kp;
    int16_t ki;
    int16_t kd;
    int16_t lastErr;
    int32_t intEg;
} PID;

typedef struct Flag {
    bool power : 1;
    bool heating : 1;
    bool tempChanged : 1;
    bool requireRST : 1;
    bool  : 1;
    bool  : 1;
    bool  : 1;
    bool  : 1;
} Flag_t;

uint8_t PIDUpdate(PID* p, uint16_t curValue, uint16_t dstValue);

u16 ADCToTemp(u16 adcVal);

u8 TempToPeriod(u16 curTemp, u16 dstTemp);

void PeriodToLEDf(u8 period, u16* highTick, u16* lowTick);

#define MAX_PERIOD (100-1)
#define MIN_PERIOD 1

#define VAL_MAX 4095
#define VAL_MIN 0
#define VAL_STEP 100
#define VAL_DEF 1500

//LED_MAX_F 3Hz
//LED_MIN_F .5Hz
