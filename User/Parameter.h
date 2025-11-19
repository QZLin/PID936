#pragma once

#include "ch32x035_conf.h"

u16 ADCToTemp(u16 adcVal);

u8 TempToPeriod(u16 curTemp, u16 dstTemp);

u8 PeriodToLEDf(u8 period, u16 *highTick, u16 *lowTick);

#define MAX_PERIOD (100-1)
#define MIN_PERIOD 1

#define TEMP_MAX 500
#define TEMP_MIN 100
#define TEMP_STEP 10
#define TEMP_DEF 200

//LED_MAX_F 3Hz
//LED_MIN_F .5Hz
