#include "Parameter.h"


u16 ADCToTemp(u16 adcVal)
{
    if (adcVal < 100)
        return UINT16_MAX;
    return adcVal / 4096 * 500;
}

u8 TempToCounter(u16 curTemp, u16 dstTemp)
{
    return 1;
}
