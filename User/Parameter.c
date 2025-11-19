#include "Parameter.h"
#include "FreeRTOS.h"


u16 ADCToTemp(const u16 adcVal) {
    if (adcVal < 100)
        return UINT16_MAX;
    return adcVal / 4096 * 500;
}

u8 TempToPeriod(const u16 curTemp, const u16 dstTemp) {
    if (dstTemp < curTemp)
        return 0;
    const u16 delta = dstTemp - curTemp;
    if (delta > 100) {
        return 20;
    }
    if (delta > 50) {
        return 10;
    }
    return MIN_PERIOD;
}

u8 PeriodToLEDf(const u8 period, u16 *highTick, u16 *lowTick) {
    if (period == MIN_PERIOD) {
        // .5Hz
        *highTick = pdMS_TO_TICKS(2000);
        *lowTick = *highTick;
        return 1;
    }
    if (period > 80) {
        *highTick = pdMS_TO_TICKS(500);
        *lowTick = *highTick;
        return 1;
    }
    if (period > 50) {
        *highTick = pdMS_TO_TICKS(600);
        *lowTick = *highTick;
        return 1;
    }
    if (period > 30) {
        *highTick = pdMS_TO_TICKS(700);
        *lowTick = *highTick;
        return 1;
    }
    return 0;
}
