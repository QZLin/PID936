#include "DelayRTOS.h"

#include <stdint.h>
#include "ch32x035_conf.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

void xDelay_Init(void)
{
    // 使能TIM2时钟
    RCC->APB1PCENR |= RCC_APB1Periph_TIM2;

    // CH32x033 定时器配置
    TIM2->PSC = 47; // 预分频器: 48MHz/(47+1) = 1MHz
    TIM2->ATRLR = 0xFFFF; // CH32使用ATRLR而不是ARR
    TIM2->CNT = 0; // 计数器清零
    TIM2->CTLR1 |= TIM_CEN; // 启动定时器
}


void xDelay_Us(uint32_t n)
{
    TickType_t start_tick;
    uint32_t start_time, elapsed;

    // 临界段保护
    taskENTER_CRITICAL();

    if (n < 50)
    {
        // 极短延时：忙等待
        for (volatile uint32_t i = 0; i < n * 6; i++)
        {
            __asm__("nop");
        }
    }
    else if (n < 500)
    {
        // 短延时：使用硬件定时器忙等待
        start_time = TIM2->CNT;
        do
        {
            elapsed = TIM2->CNT - start_time;
            if (elapsed > 0x80000000)
            {
                // 处理溢出
                elapsed = TIM2->CNT + (0xFFFF - start_time);
            }
        }
        while (elapsed < n);
    }
    else
    {
        // 长延时：结合FreeRTOS延时
        start_tick = xTaskGetTickCount();

        // 首先延时整数毫秒部分
        uint32_t ms_delay = n / 1000;
        if (ms_delay > 0)
        {
            taskEXIT_CRITICAL();
            vTaskDelay(pdMS_TO_TICKS(ms_delay));
            taskENTER_CRITICAL();
        }

        // 剩余微秒部分使用硬件定时器
        uint32_t us_remaining = n % 1000;
        if (us_remaining > 0)
        {
            start_time = TIM2->CNT;
            do
            {
                elapsed = TIM2->CNT - start_time;
                if (elapsed > 0x80000000)
                {
                    elapsed = TIM2->CNT + (0xFFFF - start_time);
                }
            }
            while (elapsed < us_remaining);
        }
    }

    taskEXIT_CRITICAL();
}
