/* 
Project: PID936/CH32x033F8P6
*/
// ReSharper disable CppRedundantInlineSpecifier
#include <stdbool.h>
#include "ch32x035_conf.h"
#include "debug.h"
#include "assert.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"

#include "IODefine.h"
#include "Util.h"
#include "UARTCmd.h"
#include "Parameter.h"

/* Global define */
#include "FreeRTOSDef.h"

/* Global Variable */
volatile u8 hc595Data = LED_READY, key = 0, triggerPeriod = 1; // 1-99 0.1ms/number
volatile u16 thVal = 0, dstVal = 0;
volatile Flag_t flag = {0};
static PID pid = {
    .kp = 5, // 建议初始值，可调
    .ki = 1, // 每ms累加
    .kd = 1, // 微分系数
    .lastErr = 0,
    .intEg = 0
};
//
TaskHandle_t HandlerHello;
u16 counter = 0, x0Count = 0;
SemaphoreHandle_t xZeroSemaphore = NULL, xCounterMutex = NULL, x595Mutex = NULL;
u8 TxBuffer2[16] = {0}, RxBuffer2[16] = {0};

static void print_binary(const u8 num) {
    for(int i = 7; i >= 0; i--)
        putchar(num & 1 << i ? '1' : '0');
    putchar(13);
    putchar(10);
}

static inline void RCCInit() {
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
        RCC_APB2Periph_ADC1 | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_TIM2, ENABLE);
}

/// @brief   Initializes GPIO
static inline void GPIOInit(void) {
    GPIO_InitTypeDef def = {0};
    def.GPIO_Speed = GPIO_Speed_50MHz;
#define SetGPIO(port,pin) \
    def.GPIO_Pin = pin; \
    GPIO_Init(port, &def);
#define SXN(name) SetGPIO(PORT_##name, PIN_##name)
    // HC595D
    def.GPIO_Mode = GPIO_Mode_Out_PP;
    SXN(SER595);
    SXN(ST595);
    SXN(SH595);
    //
    // def.GPIO_Mode = GPIO_Mode_Out_PP;
    SXN(CONTROL);
    // HC165D
    // def.GPIO_Mode = GPIO_Mode_Out_PP;
    SXN(CLK165);
    SXN(LD165);
    def.GPIO_Mode = GPIO_Mode_IPU;
    SXN(SER165);
    //
    def.GPIO_Mode = GPIO_Mode_AIN;
    SXN(ADC_TEMP);
}

// ReSharper disable once CppDFAConstantParameter
static inline void UART2Init(const uint32_t baud_rate) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    USART_InitTypeDef USART_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(UART_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(UART_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baud_rate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    USART_Cmd(USART2, ENABLE);
    UART_ReceiveInit();
}

static inline void ADCInit(void) {
    ADC_InitTypeDef ADC_InitStructure = {0};

    ADC_DeInit(ADC_TEMP);
    ADC_CLKConfig(ADC_TEMP, ADC_CLK_Div6);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = ADC_CH_TEMP;
    ADC_Init(ADC_TEMP, &ADC_InitStructure);

    ADC_Cmd(ADC_TEMP, ENABLE);
}

static inline void EXTIInit(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin = PIN_X0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PORT_X0, &GPIO_InitStructure);

    GPIO_EXTILineConfig(PORT_SRC_X0, PIN_SRC_X0);

    EXTI_InitStructure.EXTI_Line = LINE_X0;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = IRQ_X0;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

#ifdef DEV_INFO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource7);

    EXTI_InitStructure.EXTI_Line = EXTI_Line7;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
#endif
}

static inline void TIM2Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct = {0};
    NVIC_InitTypeDef NVIC_InitStruct = {0};

    TIM_TimeBaseInitStruct.TIM_Period = 10; // 自动重装载值 (10kHz/10 = 1kHz -> 1ms)
    TIM_TimeBaseInitStruct.TIM_Prescaler = 4800 - 1; // 预分频值 (48MHz/4800 = 10kHz)
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_SelectOnePulseMode(TIM2, TIM_OPMode_Single);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitStruct.NVIC_IRQChannel = TIM2_UP_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    TIM_Cmd(TIM2, DISABLE);
}

static inline void StateInit() {
    flag.power = false;
    flag.heating = false;
    flag.tempChanged = true;
    flag.requireRST = false;

    dstVal = VAL_DEF;
}

static inline void TIM2_StartSingleShot(void) {
    if(flag.tempChanged) {
        TIM_Cmd(TIM2, DISABLE);
        TIM_SetAutoreload(TIM2, triggerPeriod);
        flag.tempChanged = false;
    }
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除可能存在的挂起中断
    TIM_SetCounter(TIM2, 0); // 计数器归零
    TIM_Cmd(TIM2, ENABLE); // 使能定时器（单次模式，计数到ARR后自动停止）
}

static inline void TriggerOnce(void) {
    if(!flag.heating)
        return;
    GPIO_SetBits(PORT_CONTROL, PIN_CONTROL);
    TIM2_StartSingleShot();
}

/// @brief 定时器回调函数
void CrossZeroTimerCallback(TimerHandle_t xTimer) {
    if(xSemaphoreTake(xCounterMutex, pdMS_TO_TICKS(100))) {
        x0Count = counter;
        counter = 0;
        xSemaphoreGive(xCounterMutex);
    }
}

/// @brief 创建并启动定时器
void CreateCrossZeroTimer() {
    TimerHandle_t xCrossZeroTimer = xTimerCreate( // 创建自动重载的定时器，周期1000ms
        "X0TIM", // 定时器名称
        pdMS_TO_TICKS(1000), // 定时周期（1秒）
        pdTRUE, // 自动重载
        0, // 定时器ID
        CrossZeroTimerCallback // 回调函数
        );

    if(xCrossZeroTimer != NULL) {
        xTimerStart(xCrossZeroTimer, 0); // 启动定时器
    }
}

/// @brief cross zero counter per second
_Noreturn void taskX0Counter(void* pvParameters) {
    GPIO_ResetBits(PORT_CONTROL, PIN_CONTROL);
    for(;;) {
        if(xSemaphoreTake(xZeroSemaphore, portMAX_DELAY) == pdTRUE) {
            if(xSemaphoreTake(xCounterMutex, portMAX_DELAY) == pdTRUE) {
                counter++;
                xSemaphoreGive(xCounterMutex);
            }
        }
    }
}


/// @brief
_Noreturn void taskControl(void* pvParameters) {
    for(;;) {
        if(!flag.power) {
            flag.heating = false;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if(thVal == VAL_MAX) {
            flag.power = false;
            flag.heating = false;
            flag.requireRST = true;
            SetLED(LED_WARN);
            printf("TH Offline!\r\n");
        }
        // state.curTemp = ADCToTemp(thVal);
        const u8 period = PIDUpdate(&pid, thVal, dstVal);
        if(period != 0) {
            flag.heating = true;
            triggerPeriod = period;
        }
        else {
            flag.heating = false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

_Noreturn void taskShowPeriod(void* pvParameters) {
    for(;;) {
        if(!flag.heating) {
            UnSetLED(LED_TEMP);
            vTaskDelay(TICK_INFO);
            continue;
        }
        u16 highTick = 0;
        u16 lowTick = 0;
        PeriodToLEDf(triggerPeriod, &highTick, &lowTick);
        SetLED(LED_TEMP);
        vTaskDelay(highTick);
        UnSetLED(LED_TEMP);
        vTaskDelay(lowTick);
    }
}

_Noreturn void taskShowTarget(void* pvParameters) {
#define T 100
    for(;;) {
        vTaskDelayMs(1000);
        const u16 localDst = dstVal;
        const u8 t10e2 = localDst / 100 % 10;
        u8 t10e3;
        if(localDst < 1000)
            t10e3 = 0;
        else if(localDst < 2000)
            t10e3 = 1;
        else if(localDst < 3000)
            t10e3 = 2;
        else if(localDst < 4000)
            t10e3 = 3;
        else
            t10e3 = 4;

        for(int i = 0; i < t10e3; i++) {
            SetLED(LED3);
            vTaskDelayMs(T);
            UnSetLED(LED3);
            vTaskDelayMs(T*2);
        }
        if(t10e2 == 0)
            continue;

        u8 shortFlashes = 0, hasLongFlash = 0;
        if(t10e2 == 1)
            shortFlashes = 1;
        else if(t10e2 == 2)
            hasLongFlash = 1;
        else {
            // 3-9: 短闪表示2的个数，长闪表示+1
            shortFlashes = t10e2 >> 1; // 除以2，等价于 t10e2 / 2
            hasLongFlash = t10e2 & 1; // 检查最低位，等价于 t10e2 % 2
        }

        for(int i = 0; i < shortFlashes; i++) {
            SetLED(LED4);
            vTaskDelayMs(T);
            UnSetLED(LED4);
            vTaskDelayMs(T*2);
        }
        if(hasLongFlash) {
            SetLED(LED4);
            vTaskDelayMs(T*3);
            UnSetLED(LED4);
            vTaskDelayMs(T*2);
        }
    }
}


/// @brief 处理中断线 11（即 PA11）
INT void EXTI15_8_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if(EXTI_GetITStatus(LINE_X0) != RESET) {
        EXTI_ClearITPendingBit(LINE_X0);
        xSemaphoreGiveFromISR(xZeroSemaphore, &xHigherPriorityTaskWoken);
        TriggerOnce();
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/// @brief TIM2 UP EXTI
INT void TIM2_UP_IRQHandler(void) {
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        GPIO_ResetBits(PORT_CONTROL, PIN_CONTROL); // 拉低
    }
}

#ifdef DEV_INFO
/// @brief RST
INT void EXTI7_0_IRQHandler(void) {
    if(EXTI_GetITStatus(EXTI_Line7) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line7);
        printf("RST\r\n");
        NVIC_SystemReset();
    }
}
#endif


/**
 * @brief   This function handles USART2 global interrupt request.
 */
INT void USART2_IRQHandler(void) {
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        const u8 data = USART_ReceiveData(USART2);
        UART_ReceiveHandler(data);
    }
}

/// @brief 74HC595 LED
_Noreturn void taskHC595Out(void* pvParameters) {
    PRINT("TASK 595dis\r\n");
    u8 lastState = 0b0;
    for(;;) {
        if(lastState != hc595Data) {
            taskENTER_CRITICAL();
            u8 data = (u8)~hc595Data;
            taskEXIT_CRITICAL();
            for(u8 i = 0; i < 8; i++) {
                if(data & 0x80)
                    GPIO_SetBits(PORT_SER595, PIN_SER595);
                else
                    GPIO_ResetBits(PORT_SER595, PIN_SER595);
                GPIO_SetBits(PORT_SH595, PIN_SH595);
                vTaskDelay(pdUS_TO_TICKS(1));
                GPIO_ResetBits(PORT_SH595, PIN_SH595);
                vTaskDelay(pdUS_TO_TICKS(1));
                data <<= 1;
            }
            vTaskDelay(pdUS_TO_TICKS(1)); // 确保数据稳定
            GPIO_SetBits(PORT_ST595, PIN_ST595);
            vTaskDelay(pdUS_TO_TICKS(1)); // 锁存脉冲宽度
            GPIO_ResetBits(PORT_ST595, PIN_ST595);
            lastState = hc595Data; // 更新最后状态
        }
        vTaskDelay(TICK_DISPLAY);
    }
}

void taskHello(void* pvParameters) {
    if(!xSemaphoreTake(x595Mutex, pdMS_TO_TICKS(100)))
        return;
    const u8 lastState = hc595Data;
    u8 temp = 0x1;
    hc595Data = temp;
    for(u8 i = 0; i < 8; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        temp = temp << 1;
        hc595Data = temp;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    hc595Data = lastState;
    HandlerHello = NULL;
    xSemaphoreGive(x595Mutex);
    vTaskDelete(NULL);
}

static void handleKey(const u8 v) {
#define MASK(bin) ((v & bin) == 0)
    if(v == 0b01111111) {
        printf("TH:%u X0:%u T:%u\r\n", thVal, x0Count, dstVal);
    }
    else if(MASK(BTN_DBG)) {
        if(MASK(BTN_RST)) {
            if(HandlerHello == NULL) {
                printf("OwO\r\n");
                xTaskCreate(taskHello, "OwO",TASK_STK_SIZE2,NULL,PRIO_DISPLAY, &HandlerHello);
            }
        }
        else if(MASK(BTN_PWR)) {
            printf("*");
            TriggerOnce(); //手动触发一次以测试
        }
        else if(MASK(BTN_ADD)) {
            if(triggerPeriod < MAX_PERIOD) {
                triggerPeriod++;
                flag.tempChanged = true;
            }
        }
        else if(MASK(BTN_SUB)) {
            if(triggerPeriod > MIN_PERIOD) {
                triggerPeriod--;
                flag.tempChanged = true;
            }
        }
    }
    else if(MASK(BTN_ADD)) {
        if(dstVal < VAL_MAX) {
            dstVal += VAL_STEP;
            printf("T+:Val=%u\r\n", dstVal);
        }
    }
    else if(MASK(BTN_SUB)) {
        if(dstVal > VAL_STEP) {
            dstVal -= VAL_STEP;
            printf("T-:Val=%u\r\n", dstVal);
        }
    }
    else if(MASK(BTN_RST)) {
        dstVal = VAL_DEF;
        printf("T*:Val=%u\r\n", dstVal);
    }
    else if(MASK(BTN_PWR)) {
        flag.power = !flag.power;
        xSemaphoreTake(x595Mutex, portMAX_DELAY);
        if(flag.power) {
            SetMask(hc595Data, LED_PWR);
        }
        else {
            UnSetMask(hc595Data, LED_PWR);
        }
        xSemaphoreGive(x595Mutex);
    }
}

_Noreturn void taskHC165In(void* pvParameters) {
    PRINT("TASK HC165IN\r\n");
    u8 lastKey = 0b00000000;
    for(;;) {
        u8 data = 0;
        GPIO_WriteBit(PORT_LD165, PIN_LD165, Bit_RESET);
        vTaskDelay(pdUS_TO_TICKS(1));
        GPIO_WriteBit(PORT_LD165, PIN_LD165, Bit_SET);
        vTaskDelay(pdUS_TO_TICKS(1));

        for(u8 i = 0; i <= 7; i++) {
            data <<= 1;
            if(GPIO_ReadInputDataBit(PORT_SER165, PIN_SER165))
                data |= 0x01;

            GPIO_WriteBit(PORT_CLK165, PIN_CLK165, Bit_SET);
            vTaskDelay(pdUS_TO_TICKS(1));
            GPIO_WriteBit(PORT_CLK165, PIN_CLK165, Bit_RESET);
            vTaskDelay(pdUS_TO_TICKS(1));
        }
        key = data;
        if(key != lastKey) {
            handleKey(key);
            lastKey = key;
            PRINT("key:");
            PRINTB(key);
        }
        vTaskDelay(TICK_INPUT);
    }
}

/// @brief Read TH ADC val
_Noreturn void taskReadTH(void* pvParameters) {
    u16 lastVal = UINT16_MAX;
    for(;;) {
        ADC_RegularChannelConfig(ADC_TEMP, ADC_CH_TEMP, 1, ADC_SampleTime_11Cycles);
        ADC_SoftwareStartConvCmd(ADC_TEMP, ENABLE);

        while(!ADC_GetFlagStatus(ADC_TEMP, ADC_FLAG_EOC)) {
            vTaskDelay(TICK_SENSOR);
        }
        thVal = ADC_GetConversionValue(ADC_TEMP);
        if(lastVal != thVal) {
            lastVal = thVal;
        }
        vTaskDelay(TICK_SENSOR);
    }
}

/**
 * @brief   task2 program.
 */
void task2_task(void* pvParameters) {
    printf("FreeHeap:%u\r\n", xPortGetFreeHeapSize());
    vTaskDelay(pdMS_TO_TICKS(1000));
    vTaskDelete(NULL);
}


/// @brief UART Command handler task
_Noreturn void taskUARTCmd(void* pvParameters) {
    PRINT("UART CMD Task\r\n");
    UART_SendString(">>> ");

    for(;;) {
        UART_ProcessReceived();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


/**
 * @fn      main
 * @brief   Main program.
 * @return  none
 */
int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    // Delay_Init();
    USART_Printf_Init(115200);
    printf("SysClk:%lu\r\n", SystemCoreClock);
    printf("ChipID:%08x\r\n", (u8)DBGMCU_GetCHIPID());
    printf("FreeRTOS Ver:%s\r\n", tskKERNEL_VERSION_NUMBER);
    RCCInit();
    UART2Init(115200);

    GPIOInit();
    ADCInit();
    TIM2Init();
    EXTIInit();
    StateInit();

    xZeroSemaphore = xSemaphoreCreateBinary();
    xCounterMutex = xSemaphoreCreateMutex();
    x595Mutex = xSemaphoreCreateMutex();

#define TSK(func,handler,depth,priority) xTaskCreate(func,#func,depth,NULL,priority,handler)
    TSK(taskReadTH, NULL, TASK_STK_SIZE3, PRIO_SENSOR);
    TSK(taskHC165In, NULL, TASK_STK_SIZE3, PRIO_INPUT);
    TSK(taskUARTCmd, NULL, TASK_STK_SIZE3, PRIO_INPUT);
    TSK(taskControl, NULL, TASK_STK_SIZE5, PRIO_INPUT);
    TSK(taskHC595Out, NULL, TASK_STK_SIZE3, PRIO_DISPLAY);


    TSK(taskX0Counter, NULL, TASK_STK_SIZE3, PRIO_INFO);
    TSK(taskShowPeriod, NULL, TASK_STK_SIZE2, PRIO_INFO);
    TSK(taskShowTarget, NULL, TASK_STK_SIZE2, PRIO_INFO);

    TSK(taskHello, &HandlerHello, TASK_STK_SIZE2, PRIO_INFO);

    CreateCrossZeroTimer();
    vTaskStartScheduler();
    // ReSharper disable once CppDFAEndlessLoop
    for(;;)
        assert(1);
}

/// @brief
_Noreturn void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    printf("\r\nStack overflow in task: %s\r\n", pcTaskName);
    for(;;);
}

/// @brief
_Noreturn void vApplicationMallocFailedHook(void) {
    printf("\r\nMALLOC FAILED! Free Heap: %u\r\n", xPortGetFreeHeapSize());
    for(;;);
}
