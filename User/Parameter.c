#include "Parameter.h"
#include "FreeRTOS.h"

// curValue 和 dstValue 范围 0-4095
// 返回 period 0-99，对应 0~9.9ms 导通时间
uint8_t PIDUpdate(PID* p, const uint16_t curValue, const uint16_t dstValue)
{
    const int32_t tempErr = (int32_t)dstValue - (int32_t)curValue;
    const int16_t err = (int16_t)tempErr;

    // 积分累加，每ms步长小
    p->intEg += err;
    if (p->intEg > 5000)
        p->intEg = 5000;
    if (p->intEg < -5000)
        p->intEg = -5000;

    const int32_t tempDiff = (int32_t)err - (int32_t)p->lastErr;
    const int16_t diff = (int16_t)tempDiff;
    p->lastErr = err;

    // PID 计算
    int32_t out = (int32_t)p->kp * err
        + (int32_t)p->ki * p->intEg
        + (int32_t)p->kd * diff;

    // 缩放映射到 0-99
    out /= 200; // 缩放系数，可根据系统响应调整

    if (out < 0)
        out = 0;
    if (out > 99)
        out = 99;

    return (uint8_t)out;
}


u16 ADCToTemp(const u16 adcVal)
{
    if (adcVal == VAL_MAX)
        return UINT16_MAX;
    return adcVal / 4096 * 500;
}

/* led_high_ticks_pdms */
const TickType_t led_high_ticks_pdms[100] = {
    pdMS_TO_TICKS(400), pdMS_TO_TICKS(405), pdMS_TO_TICKS(409), pdMS_TO_TICKS(413), pdMS_TO_TICKS(417),
    pdMS_TO_TICKS(421), pdMS_TO_TICKS(424), pdMS_TO_TICKS(427), pdMS_TO_TICKS(430), pdMS_TO_TICKS(433),
    pdMS_TO_TICKS(435), pdMS_TO_TICKS(437), pdMS_TO_TICKS(439), pdMS_TO_TICKS(441), pdMS_TO_TICKS(442),
    pdMS_TO_TICKS(443), pdMS_TO_TICKS(445), pdMS_TO_TICKS(446), pdMS_TO_TICKS(446), pdMS_TO_TICKS(447),
    pdMS_TO_TICKS(447), pdMS_TO_TICKS(448), pdMS_TO_TICKS(448), pdMS_TO_TICKS(448), pdMS_TO_TICKS(447),
    pdMS_TO_TICKS(447), pdMS_TO_TICKS(447), pdMS_TO_TICKS(446), pdMS_TO_TICKS(445), pdMS_TO_TICKS(445),
    pdMS_TO_TICKS(444), pdMS_TO_TICKS(443), pdMS_TO_TICKS(442), pdMS_TO_TICKS(440), pdMS_TO_TICKS(439),
    pdMS_TO_TICKS(437), pdMS_TO_TICKS(436), pdMS_TO_TICKS(434), pdMS_TO_TICKS(433), pdMS_TO_TICKS(431),
    pdMS_TO_TICKS(429), pdMS_TO_TICKS(427), pdMS_TO_TICKS(425), pdMS_TO_TICKS(423), pdMS_TO_TICKS(421),
    pdMS_TO_TICKS(419), pdMS_TO_TICKS(416), pdMS_TO_TICKS(414), pdMS_TO_TICKS(412), pdMS_TO_TICKS(409),
    pdMS_TO_TICKS(407), pdMS_TO_TICKS(405), pdMS_TO_TICKS(402), pdMS_TO_TICKS(399), pdMS_TO_TICKS(397),
    pdMS_TO_TICKS(394), pdMS_TO_TICKS(392), pdMS_TO_TICKS(389), pdMS_TO_TICKS(386), pdMS_TO_TICKS(383),
    pdMS_TO_TICKS(381), pdMS_TO_TICKS(378), pdMS_TO_TICKS(375), pdMS_TO_TICKS(372), pdMS_TO_TICKS(369),
    pdMS_TO_TICKS(366), pdMS_TO_TICKS(363), pdMS_TO_TICKS(361), pdMS_TO_TICKS(358), pdMS_TO_TICKS(355),
    pdMS_TO_TICKS(352), pdMS_TO_TICKS(349), pdMS_TO_TICKS(346), pdMS_TO_TICKS(343), pdMS_TO_TICKS(340),
    pdMS_TO_TICKS(337), pdMS_TO_TICKS(334), pdMS_TO_TICKS(331), pdMS_TO_TICKS(328), pdMS_TO_TICKS(325),
    pdMS_TO_TICKS(322), pdMS_TO_TICKS(319), pdMS_TO_TICKS(316), pdMS_TO_TICKS(313), pdMS_TO_TICKS(310),
    pdMS_TO_TICKS(307), pdMS_TO_TICKS(304), pdMS_TO_TICKS(301), pdMS_TO_TICKS(298), pdMS_TO_TICKS(295),
    pdMS_TO_TICKS(292), pdMS_TO_TICKS(290), pdMS_TO_TICKS(287), pdMS_TO_TICKS(284), pdMS_TO_TICKS(281),
    pdMS_TO_TICKS(278), pdMS_TO_TICKS(275), pdMS_TO_TICKS(272), pdMS_TO_TICKS(269), pdMS_TO_TICKS(267)
};

/* led_low_ticks_pdms */
const TickType_t led_low_ticks_pdms[100] = {
    pdMS_TO_TICKS(1600), pdMS_TO_TICKS(1559), pdMS_TO_TICKS(1520), pdMS_TO_TICKS(1481), pdMS_TO_TICKS(1443),
    pdMS_TO_TICKS(1406), pdMS_TO_TICKS(1370), pdMS_TO_TICKS(1335), pdMS_TO_TICKS(1300), pdMS_TO_TICKS(1266),
    pdMS_TO_TICKS(1234), pdMS_TO_TICKS(1202), pdMS_TO_TICKS(1171), pdMS_TO_TICKS(1140), pdMS_TO_TICKS(1110),
    pdMS_TO_TICKS(1082), pdMS_TO_TICKS(1052), pdMS_TO_TICKS(1024), pdMS_TO_TICKS(998), pdMS_TO_TICKS(971),
    pdMS_TO_TICKS(946), pdMS_TO_TICKS(920), pdMS_TO_TICKS(895), pdMS_TO_TICKS(871), pdMS_TO_TICKS(848),
    pdMS_TO_TICKS(825), pdMS_TO_TICKS(802), pdMS_TO_TICKS(781), pdMS_TO_TICKS(760), pdMS_TO_TICKS(738),
    pdMS_TO_TICKS(718), pdMS_TO_TICKS(698), pdMS_TO_TICKS(679), pdMS_TO_TICKS(661), pdMS_TO_TICKS(642),
    pdMS_TO_TICKS(625), pdMS_TO_TICKS(606), pdMS_TO_TICKS(590), pdMS_TO_TICKS(572), pdMS_TO_TICKS(556),
    pdMS_TO_TICKS(541), pdMS_TO_TICKS(525), pdMS_TO_TICKS(510), pdMS_TO_TICKS(495), pdMS_TO_TICKS(481),
    pdMS_TO_TICKS(467), pdMS_TO_TICKS(454), pdMS_TO_TICKS(440), pdMS_TO_TICKS(427), pdMS_TO_TICKS(415),
    pdMS_TO_TICKS(402), pdMS_TO_TICKS(390), pdMS_TO_TICKS(378), pdMS_TO_TICKS(367), pdMS_TO_TICKS(356),
    pdMS_TO_TICKS(345), pdMS_TO_TICKS(334), pdMS_TO_TICKS(324), pdMS_TO_TICKS(314), pdMS_TO_TICKS(305),
    pdMS_TO_TICKS(294), pdMS_TO_TICKS(285), pdMS_TO_TICKS(276), pdMS_TO_TICKS(268), pdMS_TO_TICKS(259),
    pdMS_TO_TICKS(251), pdMS_TO_TICKS(243), pdMS_TO_TICKS(234), pdMS_TO_TICKS(226), pdMS_TO_TICKS(219),
    pdMS_TO_TICKS(211), pdMS_TO_TICKS(204), pdMS_TO_TICKS(197), pdMS_TO_TICKS(191), pdMS_TO_TICKS(184),
    pdMS_TO_TICKS(178), pdMS_TO_TICKS(171), pdMS_TO_TICKS(165), pdMS_TO_TICKS(159), pdMS_TO_TICKS(154),
    pdMS_TO_TICKS(148), pdMS_TO_TICKS(143), pdMS_TO_TICKS(137), pdMS_TO_TICKS(132), pdMS_TO_TICKS(127),
    pdMS_TO_TICKS(122), pdMS_TO_TICKS(118), pdMS_TO_TICKS(113), pdMS_TO_TICKS(109), pdMS_TO_TICKS(104),
    pdMS_TO_TICKS(100), pdMS_TO_TICKS(95), pdMS_TO_TICKS(91), pdMS_TO_TICKS(88), pdMS_TO_TICKS(84),
    pdMS_TO_TICKS(80), pdMS_TO_TICKS(77), pdMS_TO_TICKS(74), pdMS_TO_TICKS(70), pdMS_TO_TICKS(66)
};

void PeriodToLEDf(const uint8_t period, uint16_t* highTick, uint16_t* lowTick)
{
    uint8_t idx = period;
    if (idx > 99)
        idx = 99; // 限幅

    *highTick = (uint16_t)led_high_ticks_pdms[idx];
    *lowTick = (uint16_t)led_low_ticks_pdms[idx];
}