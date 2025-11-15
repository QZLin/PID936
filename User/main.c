/* 
Project: PID936/CH32x033F8P6
*/

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
#include "uart_cmd.h"

/* Global define */
#include "FreeRTOSDef.h"

/* Global Variable */
TaskHandle_t Task1Task_Handler, Task2Task_Handler;
TaskHandle_t Handler595DIS, Handler165IN, HandlerCrossZero, HandlerReadTH, HandlerControl;
TaskHandle_t HandlerHello;
//
volatile u8 hc595Data = 0;
volatile u8 key = 0;

// 50Hz, t=0.02s
// interval=0.01s=10ms=10 000us
SemaphoreHandle_t xZeroSemaphore = NULL;
u16 counter = 0;
SemaphoreHandle_t xCounterMutex = NULL;
u8 parseLen = 0;
SemaphoreHandle_t xPhLenMutex = NULL;

u16 thVal = 0;
u16 x0Count = 0;

inline void SetGPIO(GPIO_TypeDef* GPIOx, GPIO_InitTypeDef def, const uint32_t pin)
{
    def.GPIO_Pin = pin;
    GPIO_Init(GPIOx, &def);
}

static void print_binary(const uint8_t num)
{
    for (int i = 7; i >= 0; i--)
        putchar(num & 1 << i ? '1' : '0');
    putchar(13);
    putchar(10);
}

/*********************************************************************
 * @fn      GPIO_INIT
 * @brief   Initializes GPIO
 * @return  none
 */
static void GPIO_CFG(void)
{
    GPIO_InitTypeDef def = {0};
    def.GPIO_Speed = GPIO_Speed_50MHz;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
#define SX(port, pin) SetGPIO(port, def, pin)

    def.GPIO_Mode = GPIO_Mode_Out_PP;
    // HC595D
    SX(PORT_SER595, PIN_SER595);
    SX(PORT_ST595, PIN_ST595);
    SX(PORT_SH595, PIN_SH595);
    // HC165D
    def.GPIO_Mode = GPIO_Mode_IPU;
    SX(PORT_SER165, PIN_SER165);
    def.GPIO_Mode = GPIO_Mode_Out_PP;
    SX(PORT_CLK165, PIN_CLK165);
    SX(PORT_LD165, PIN_LD165);
    //
    def.GPIO_Mode = GPIO_Mode_Out_PP;
    SX(PORT_CONTROL, PIN_CONTROL);
    //
    def.GPIO_Mode = GPIO_Mode_AIN;
    SX(PORT_ADC_TEMP, PIN_ADC_TEMP);
}

static void UART2_CFG(const uint32_t baud_rate)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    // TX
    GPIO_InitStructure.GPIO_Pin = TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_PORT, &GPIO_InitStructure);
    // RX
    GPIO_InitStructure.GPIO_Pin = RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(UART_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baud_rate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    USART_Cmd(USART2, ENABLE);

    // Initialize receive interrupt
    UART_ReceiveInit();
}

static void ADC_CFG(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
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

static void EXTI_CFG(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

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
    //
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

/// @brief
/// @param pvParameters
void taskCrossZero(void* pvParameters)
{
    PRINT("TASK x0\r\n");
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (xSemaphoreTake(xCounterMutex, pdMS_TO_TICKS(100)))
        {
            x0Count = counter;
            counter = 0;
            xSemaphoreGive(xCounterMutex);
        }
    }
}

/// @brief
/// @param pvParameters
void taskControl(void* pvParameters)
{
    GPIO_ResetBits(PORT_CONTROL,PIN_CONTROL);
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        if (xSemaphoreTake(xZeroSemaphore, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(xCounterMutex, portMAX_DELAY) == pdTRUE)
            {
                counter++;
                xSemaphoreGive(xCounterMutex);
            }
            if (parseLen != 0)
            {
                GPIO_SetBits(PORT_CONTROL, PIN_CONTROL);
                vTaskDelay(pdMS_TO_TICKS(parseLen));
                GPIO_ResetBits(PORT_CONTROL, PIN_CONTROL);
            }
            // printf("*");
        }
    }
}

/*********************************************************************
 * @fn      EXTI15_8_IRQHandler
 *
 * @brief   处理中断线 11（即 PA11）
 *
 * @return  none
 */
__attribute__((interrupt())) void EXTI15_8_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (EXTI_GetITStatus(LINE_CROSSZERO) != RESET)
    {
        EXTI_ClearITPendingBit(LINE_CROSSZERO);
        xSemaphoreGiveFromISR(xZeroSemaphore, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#ifdef DEV_INFO
__attribute__((interrupt())) void EXTI7_0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line7) != RESET)
    {
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
/*********************************************************************
 * @fn      USART2_IRQHandler
 *
 * @brief   This function handles USART2 global interrupt request.
 *
 * @return  none
 */
__attribute__((interrupt())) void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t data = USART_ReceiveData(USART2);
        UART_ReceiveHandler(data);
    }
}

/// @brief
/// @param pvParameters
void taskHC595Display(void* pvParameters)
{
    PRINT("TASK 595dis\r\n");
    u8 lastState = 0b0;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        if (lastState != hc595Data)
        {
            taskENTER_CRITICAL();
            u8 data = ~hc595Data;
            taskEXIT_CRITICAL();
            for (u8 i = 0; i < 8; i++)
            {
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
            // 数据移位完成后，产生锁存信号
            vTaskDelay(pdUS_TO_TICKS(1)); // 确保数据稳定
            GPIO_SetBits(PORT_ST595, PIN_ST595);
            vTaskDelay(pdUS_TO_TICKS(1)); // 锁存脉冲宽度
            GPIO_ResetBits(PORT_ST595, PIN_ST595);

            lastState = hc595Data; // 更新最后状态
        }
        vTaskDelay(TICK_DISPLAY);
    }
}

void task_hello(void* pvParameters);


static void handleKey(const u8 v)
{
#define MASK(bin) ((v & bin) == 0)
    if (MASK(0b10000000))
    {
        // DBG
        if (MASK(0b00000001))
        {
            hc595Data++;
        }
        else if (MASK(0b00000010))
        {
            printf("OwO\r\n");
            if (HandlerHello == NULL)
                xTaskCreate(task_hello, "OwO",TASK_STK_SIZE4,NULL,PRIO_DISPLAY, &HandlerHello);
        }
        else if (MASK(0b00000100))
        {
            printf("TH:%u X/S:%u T:%u\r\n", thVal, x0Count, parseLen);
        }
    }
    else if (MASK(0b00001000))
    {
        parseLen += 1;
        printf("T:Temp=%u\r\n", parseLen);
    }
    else if (MASK(0b00000100))
    {
        parseLen -= 1;
        printf("T:Temp=%u\r\n", parseLen);
    }
}

void taskHC165Input(void* pvParameters)
{
    PRINT("TASK HC165IN\r\n");
    u8 lastKey = 0b00000000;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        u8 data = 0;
        GPIO_WriteBit(PORT_LD165, PIN_LD165, Bit_RESET);
        vTaskDelay(pdUS_TO_TICKS(2));
        GPIO_WriteBit(PORT_LD165, PIN_LD165, Bit_SET);

        for (u8 i = 0; i <= 7; i++)
        {
            data <<= 1;
            if (GPIO_ReadInputDataBit(PORT_SER165, PIN_SER165))
                data |= 0x01;

            GPIO_WriteBit(PORT_CLK165, PIN_CLK165, Bit_SET);
            vTaskDelay(pdUS_TO_TICKS(1));
            GPIO_WriteBit(PORT_CLK165, PIN_CLK165, Bit_RESET);
            vTaskDelay(pdUS_TO_TICKS(1));
        }
        key = data;
        if (key != lastKey)
        {
            handleKey(key);
            lastKey = key;
            PRINT("key:");
            PRINTB(key);
        }
        vTaskDelay(TICK_INPUT);
    }
}

/// @brief
/// @param pvParameters
void taskReadTH(void* pvParameters)
{
    u16 lastVal = UINT16_MAX;
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        ADC_RegularChannelConfig(ADC_TEMP, ADC_CH_TEMP, 1, ADC_SampleTime_11Cycles);
        ADC_SoftwareStartConvCmd(ADC_TEMP, ENABLE);

        while (!ADC_GetFlagStatus(ADC_TEMP, ADC_FLAG_EOC))
        {
            vTaskDelay(TICK_SENSOR);
        }
        thVal = ADC_GetConversionValue(ADC_TEMP);
        if (lastVal != thVal)
        {
            lastVal = thVal;
        }
        vTaskDelay(TICK_SENSOR);
    }
}

/// @brief
/// @param xTask
/// @param pcTaskName
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    printf("\r\nStack overflow in task: %s\r\n", pcTaskName);
    // ReSharper disable once CppDFAEndlessLoop
    for (;;);
}

/// @brief
/// @param
void vApplicationMallocFailedHook(void)
{
    printf("\r\nMALLOC FAILED! Free Heap: %u\r\n", xPortGetFreeHeapSize());
    // ReSharper disable once CppDFAEndlessLoop
    for (;;);
}

/*********************************************************************
 * @fn      task2_task
 *
 * @brief   task2 program.
 *
 * @param  pvParameters - Parameters point of task2
 *
 * @return  none
 */
void task2_task(void* pvParameters)
{
    printf("FreeHeap:%u\r\n", xPortGetFreeHeapSize());
    vTaskDelay(pdMS_TO_TICKS(1000));
    vTaskDelete(NULL);
}

void task_hello(void* pvParameters)
{
    hc595Data = 0x1;
    for (u8 i = 0; i < 7; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(200));
        hc595Data = hc595Data << 1;
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    hc595Data = 0xFF;
    HandlerHello = NULL;
    vTaskDelete(NULL);
}

/// @brief UART Command handler task
/// @param pvParameters
void taskUARTCommand(void* pvParameters)
{
    PRINT("UART CMD Handler Task\r\n");
    UART_SendString(">>> ");

    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
    {
        UART_ProcessReceived();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();

    // Delay_Init();
    USART_Printf_Init(115200);
    printf("SystemClk:%lu\r\n", SystemCoreClock);
    printf("ChipID:%08x\r\n", (u8)DBGMCU_GetCHIPID());
    printf("FreeRTOS Kernel Version:%s\r\n", tskKERNEL_VERSION_NUMBER);
    UART2_CFG(115200);

    GPIO_CFG();
    ADC_CFG();
    EXTI_CFG();

    xZeroSemaphore = xSemaphoreCreateBinary();
    xCounterMutex = xSemaphoreCreateMutex();
    xPhLenMutex = xSemaphoreCreateMutex();
    // xTaskCreate(task2_task, "task2", TASK_STK_SIZE2, NULL, PRIO_DISPLAY,
    //             &Task2Task_Handler);

    xTaskCreate(taskHC595Display, "595dis", TASK_STK_SIZE1, NULL, PRIO_DISPLAY,
                &Handler595DIS);

    xTaskCreate(taskControl, "ctl", TASK_STK_SIZE1, NULL, PRIO_INPUT,
                &HandlerControl);
    xTaskCreate(taskHC165Input, "165in", TASK_STK_SIZE1, NULL, PRIO_INPUT,
                &Handler165IN);

    xTaskCreate(taskCrossZero, "x0", TASK_STK_SIZE2, NULL, PRIO_SENSOR,
                &HandlerCrossZero);
    xTaskCreate(taskReadTH, "r_th", TASK_STK_SIZE2, NULL, PRIO_SENSOR,
                &HandlerReadTH);
    xTaskCreate(task_hello, "owo",TASK_STK_SIZE4,NULL,PRIO_DISPLAY, &HandlerHello);

    xTaskCreate(taskUARTCommand, "uart_cmd", TASK_STK_SIZE2, NULL, PRIO_INPUT,
                NULL);

    vTaskStartScheduler();
    // ReSharper disable once CppDFAEndlessLoop
    for (;;)
        assert(1);
}
