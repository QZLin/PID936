#pragma once

#include "ch32x035_conf.h"

u16 ADCToTemp(u16 adcVal);
u8 TempToCounter(u16 curTemp, u16 dstTemp);

#define MAX_PERIOD (100-1)
#define MIN_PERIOD 1

#define MAX_TEMP 500
#define MIN_TEMP 0
#define STEP_TEMP 10
#define DEF_TEMP 200
