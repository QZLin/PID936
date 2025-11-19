/* 
Project: PID936/CH32x033F8P6
*/
// ReSharper disable CppRedundantInlineSpecifier
#include "ch32x035_conf.h"
#include "debug.h"
#include "assert.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "IODefine.h"
#include "DevUtil.h"
#include "UARTCmd.h"
#include "Parameter.h"

/* Global define */
#include <stdbool.h>
#include "FreeRTOSDef.h"

typedef struct Flag {
    bool power : 1;
    bool heating : 1;
    bool tempChanged : 1;
    bool  : 1;
    bool  : 1;
    bool  : 1;
    bool  : 1;
    bool  : 1;
} Flag_t;

typedef struct State {
    u16 curTemp;
    u16 dstTemp;
} State_t;

/* Global Variable */
TaskHandle_t Task1Task_Handler, Task2Task_Handler;
TaskHandle_t Handler595DIS, Handler165IN, HandlerCrossZero, HandlerReadTH,
             HandlerControl;
TaskHandle_t HandlerHello;
//
volatile u8 hc595Data = 0;
volatile u8 key = 0;
// 50Hz, t=0.02s interval=0.01s=10ms=10 000us
volatile u8 triggerPeriod = 1;
volatile Flag_t flag = {0};
volatile State_t state = {0};
//
u16 counter = 0;
u16 thVal = 0;
u16 x0Count = 0;

SemaphoreHandle_t xZeroSemaphore = NULL;
SemaphoreHandle_t xCounterMutex = NULL;
SemaphoreHandle_t x595Mutex = NULL;


static inline void SetGPIO(GPIO_TypeDef* GPIOx, GPIO_InitTypeDef def,
                           const uint32_t pin) {
    def.GPIO_Pin = pin;
    GPIO_Init(GPIOx, &def);
}

static void print_binary(const u8 num) {
    for (int i = 7; i >= 0; i--)
        putchar(num & 1 << i ? '1' : '0');
    putchar(13);
    putchar(10);
}

static inline void RCC_Cfg() {
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
        RCC_APB2Periph_ADC1 | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_TIM2, ENABLE);
}

/// @brief   Initializes GPIO
static inline void GPIOInit(void) {
    GPIO_InitTypeDef def = {0};
    def.GPIO_Speed = GPIO_Speed_50MHz;
    // #define SX(port, pin) SetGPIO(port, def, pin)
#define SXN(name) SetGPIO(PORT_##name, def,PIN_##name)
    // HC595D
    def.GPIO_Mode = GPIO_Mode_Out_PP;
    SXN(SER595);
    SXN(ST595);
    SXN(SH595);
    // HC165D
    def.GPIO_Mode = GPIO_Mode_IPU;
    SXN(SER165);
    def.GPIO_Mode = GPIO_Mode_Out_PP;
    SXN(CLK165);
    SXN(LD165);
    //
    def.GPIO_Mode = GPIO_Mode_AIN;
    SXN(ADC_TEMP);
    //
    def.GPIO_Mode = GPIO_Mode_Out_PP;
    SXN(CONTROL);
}

static inline void UART2_CFG(const uint32_t baud_rate) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    USART_InitTypeDef USART_InitStructure = {0};
    // TX
    GPIO_InitStructure.GPIO_Pin = TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(UART_PORT, &GPIO_InitStructure);
    // RX
    GPIO_InitStructure.GPIO_Pin = RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(UART_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baud_rate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;
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

    GPIO_InitStructure.GPIO_Pin = PIN_CROSSZERO;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PORT_CROSSZERO, &GPIO_InitStructure);

    GPIO_EXTILineConfig(PORT_SRC_CROSSZERO, PIN_SRC_CROSSZERO);

    EXTI_InitStructure.EXTI_Line = LINE_CROSSZERO;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = IRQ_CROSSZERO;
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

    TIM_TimeBaseInitStruct.TIM_Period = triggerPeriod; // 自动重装载值 (10kHz/10 = 1kHz -> 1ms)
    TIM_TimeBaseInitStruct.TIM_Prescaler = 4800 - 1; // 预分频值 (48MHz/4800 = 10kHz)
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_SelectOnePulseMode(TIM2, TIM_OPMode_Single);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); // 使能 Update 中断

    NVIC_InitStruct.NVIC_IRQChannel = TIM2_UP_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    TIM_Cmd(TIM2, DISABLE);
}

static inline void FlagInit() {
    flag.power = false;
    flag.heating = false;
    flag.tempChanged = false;

    state.dstTemp = 200;
}

static inline void TIM2_StartSingleShot(void) {
    if (flag.tempChanged) {
        TIM_Cmd(TIM2, DISABLE); // 使能定时器（单次模式，计数到ARR后自动停止）
        TIM_SetAutoreload(TIM2, triggerPeriod);
        flag.tempChanged = false;
    }
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除可能存在的挂起中断
    TIM_SetCounter(TIM2, 0); // 计数器归零
    TIM_Cmd(TIM2, ENABLE); // 使能定时器（单次模式，计数到ARR后自动停止）
}

static inline void TriggerOnce(void) {
    if (flag.heating) {
        GPIO_SetBits(PORT_CONTROL, PIN_CONTROL);
        TIM2_StartSingleShot();
    }
}

/// @brief
void taskCrossZero(void* pvParameters) {
    PRINT("TASK x0\r\n");
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (xSemaphoreTake(xCounterMutex, pdMS_TO_TICKS(100))) {
            x0Count = counter;
            counter = 0;
            xSemaphoreGive(xCounterMutex);
        }
    }
}

/// @brief cross zero counter per second
void taskX0Counter(void* pvParameters) {
    GPIO_ResetBits(PORT_CONTROL, PIN_CONTROL);

    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        if (xSemaphoreTake(xZeroSemaphore, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(xCounterMutex, portMAX_DELAY) == pdTRUE) {
                counter++;
                xSemaphoreGive(xCounterMutex);
            }
        }
    }
}

/// @brief
void taskControl(void* pvParameters) {
    // u8 lastPeriod = 0;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        if (!flag.power) {
            flag.heating = false;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        state.curTemp = ADCToTemp(thVal);
        flag.heating = true;
        // u8 period = TempToPeriod(state.curTemp, state.dstTemp);
        // if (state.curTemp < state.dstTemp)
        // {
        //     flag.heating = true;
        // }
        // else
        // {
        //     flag.heating = false;
        // }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void taskShowTemp(void* pvParameters) {
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        if (!flag.heating) {
            hc595Data &= ~LED_TEMP;
            vTaskDelay(TICK_INFO);
            continue;
        }
        u16 highTick = pdMS_TO_TICKS(500);
        u16 lowTick = pdMS_TO_TICKS(500);
        PeriodToLEDf(triggerPeriod, &highTick, &lowTick);
        if (xSemaphoreTake(x595Mutex, portMAX_DELAY)) {
            hc595Data |= LED_TEMP;
            xSemaphoreGive(x595Mutex);
        }
        vTaskDelay(highTick);
        if (xSemaphoreTake(x595Mutex, portMAX_DELAY)) {
            hc595Data &= ~LED_TEMP;
            xSemaphoreGive(x595Mutex);
        }
        vTaskDelay(lowTick);
    }
}

/**
 * @brief 处理中断线 11（即 PA11）
 */
__attribute__((interrupt())) void EXTI15_8_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (EXTI_GetITStatus(LINE_CROSSZERO) != RESET) {
        EXTI_ClearITPendingBit(LINE_CROSSZERO);
        xSemaphoreGiveFromISR(xZeroSemaphore, &xHigherPriorityTaskWoken);
        TriggerOnce();
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief TIM2
 */
__attribute__((interrupt())) void TIM2_UP_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        GPIO_ResetBits(PORT_CONTROL, PIN_CONTROL); // 拉低
    }
}

#ifdef DEV_INFO
/**
 * @brief RST
 */
__attribute__((interrupt())) void EXTI7_0_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line7) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line7);
        printf("RST\r\n");
        NVIC_SystemReset();
    }
}
#endif

#define TxSize2    (size(TxBuffer2))
#define size(a)    (sizeof(a) / sizeof(*(a)))
u8 TxBuffer2[] = "#Buffer2 Send from USART3 to USART2 using Interrupt!";
u8 RxBuffer2[TxSize2] = {0}; /* USART3 Using  */
/**
 * @brief   This function handles USART2 global interrupt request.
 */
__attribute__((interrupt())) void USART2_IRQHandler(void) {
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        const u8 data = USART_ReceiveData(USART2);
        UART_ReceiveHandler(data);
    }
}

/// @brief 74HC595 LED
void taskHC595Out(void* pvParameters) {
    PRINT("TASK 595dis\r\n");
    u8 lastState = 0b0;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        if (lastState != hc595Data) {
            taskENTER_CRITICAL();
            u8 data = (u8)~hc595Data;
            // __asm__ volatile ("nop");
            taskEXIT_CRITICAL();
            for (u8 i = 0; i < 8; i++) {
                if (data & 0x80)
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
    if (!xSemaphoreTake(x595Mutex, pdMS_TO_TICKS(100)))
        return;
    const u8 lastState = hc595Data;
    u8 temp = 0x1;
    hc595Data = temp;
    for (u8 i = 0; i < 8; i++) {
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
    if (v == 0b01111111) {
        printf("TH:%u X/S:%u T:%u\r\n", thVal, x0Count, state.dstTemp);
    } else if (MASK(BTN_DBG)) // DBG
    {
        /*if (MASK(0b00000001))
        {
            if (xSemaphoreTake(x595Mutex, pdMS_TO_TICKS(100)))
            {
                hc595Data++;
                xSemaphoreGive(x595Mutex);
            }
        }
        else*/
        if (MASK(BTN_RST)) {
            if (HandlerHello == NULL) {
                printf("OwO\r\n");
                xTaskCreate(taskHello, "OwO",TASK_STK_SIZE3,NULL,PRIO_DISPLAY,
                            &HandlerHello);
            }
        } else if (MASK(BTN_PWR)) {
            //手动触发一次以测试
            printf("*");
            TriggerOnce();
        } else if (MASK(BTN_ADD)) {
            if (triggerPeriod < MAX_PERIOD) {
                triggerPeriod++;
                flag.tempChanged = true;
            }
        } else if (MASK(BTN_SUB)) {
            if (triggerPeriod > MIN_PERIOD) {
                triggerPeriod--;
                flag.tempChanged = true;
            }
        }
    } else if (MASK(BTN_ADD)) {
        if (state.dstTemp < TEMP_MAX)
            state.dstTemp += TEMP_STEP;
        printf("T:Temp=%u\r\n", state.dstTemp);
    } else if (MASK(BTN_SUB)) {
        if (state.dstTemp < TEMP_MAX)
            state.dstTemp -= TEMP_STEP;
        printf("T:Temp=%u\r\n", state.dstTemp);
    } else if (MASK(BTN_RST)) {
        state.dstTemp = TEMP_DEF;
        printf("T:Temp=%u\r\n", state.dstTemp);
    } else if (MASK(BTN_PWR)) {
        flag.power = !flag.power;
        xSemaphoreTake(x595Mutex, portMAX_DELAY);
        if (flag.power) {
            hc595Data |= LED_PWR;
        } else {
            hc595Data &= ~LED_PWR;
        }
        xSemaphoreGive(x595Mutex);
    }
}

void taskHC165In(void* pvParameters) {
    PRINT("TASK HC165IN\r\n");
    u8 lastKey = 0b00000000;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        u8 data = 0;
        GPIO_WriteBit(PORT_LD165, PIN_LD165, Bit_RESET);
        vTaskDelay(pdUS_TO_TICKS(1));
        GPIO_WriteBit(PORT_LD165, PIN_LD165, Bit_SET);
        vTaskDelay(pdUS_TO_TICKS(1));

        for (u8 i = 0; i <= 7; i++) {
            data <<= 1;
            if (GPIO_ReadInputDataBit(PORT_SER165, PIN_SER165))
                data |= 0x01;

            GPIO_WriteBit(PORT_CLK165, PIN_CLK165, Bit_SET);
            vTaskDelay(pdUS_TO_TICKS(1));
            GPIO_WriteBit(PORT_CLK165, PIN_CLK165, Bit_RESET);
            vTaskDelay(pdUS_TO_TICKS(1));
        }
        key = data;
        if (key != lastKey) {
            handleKey(key);
            lastKey = key;
            PRINT("key:");
            PRINTB(key);
        }
        vTaskDelay(TICK_INPUT);
    }
}

/// @brief Read TH ADC val
void taskReadTH(void* pvParameters) {
    u16 lastVal = UINT16_MAX;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        ADC_RegularChannelConfig(ADC_TEMP, ADC_CH_TEMP, 1, ADC_SampleTime_11Cycles);
        ADC_SoftwareStartConvCmd(ADC_TEMP, ENABLE);

        while (!ADC_GetFlagStatus(ADC_TEMP, ADC_FLAG_EOC)) {
            vTaskDelay(TICK_SENSOR);
        }
        thVal = ADC_GetConversionValue(ADC_TEMP);
        if (lastVal != thVal) {
            lastVal = thVal;
        }
        vTaskDelay(TICK_SENSOR);
    }
}

/// @brief
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    printf("\r\nStack overflow in task: %s\r\n", pcTaskName);
    // ReSharper disable once CppDFAEndlessLoop
    for (;;);
}

/// @brief
void vApplicationMallocFailedHook(void) {
    printf("\r\nMALLOC FAILED! Free Heap: %u\r\n", xPortGetFreeHeapSize());
    // ReSharper disable once CppDFAEndlessLoop
    for (;;);
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
void taskUARTCmd(void* pvParameters) {
    PRINT("UART CMD Task\r\n");
    UART_SendString(">>> ");

    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
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
    printf("SystemClk:%lu\r\n", SystemCoreClock);
    printf("ChipID:%08x\r\n", (u8)DBGMCU_GetCHIPID());
    printf("FreeRTOS Kernel Version:%s\r\n", tskKERNEL_VERSION_NUMBER);
    RCC_Cfg();
    UART2_CFG(115200);

    GPIOInit();
    ADCInit();
    TIM2Init();
    EXTIInit();
    FlagInit();

    xZeroSemaphore = xSemaphoreCreateBinary();
    xCounterMutex = xSemaphoreCreateMutex();

    x595Mutex = xSemaphoreCreateMutex();

#define TSK(func,handler,depth,priority) xTaskCreate(func,#func,depth,NULL,priority,handler)
    TSK(taskHC595Out, &Handler595DIS, TASK_STK_SIZE1, PRIO_DISPLAY);
    TSK(taskHC165In, &Handler165IN, TASK_STK_SIZE2, PRIO_INPUT);
    TSK(taskCrossZero, &HandlerCrossZero, TASK_STK_SIZE2, PRIO_SENSOR);
    TSK(taskX0Counter, NULL, TASK_STK_SIZE1, PRIO_INFO);
    TSK(taskReadTH, &HandlerReadTH, TASK_STK_SIZE2, PRIO_SENSOR);
    TSK(taskUARTCmd, NULL, TASK_STK_SIZE2, PRIO_INPUT);

    TSK(taskShowTemp, NULL, TASK_STK_SIZE4, PRIO_INFO);

    TSK(taskHello, &HandlerHello, TASK_STK_SIZE4, PRIO_INFO);
    TSK(taskControl, NULL, TASK_STK_SIZE1, PRIO_INPUT);
    vTaskStartScheduler();
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
        assert(1);
}
