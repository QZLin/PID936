#pragma once

// #define portTICK_PERIOD_US (portTICK_PERIOD_MS / 1000)
// #define pdUS_TO_TICKS(xTimeInUs) (portTICK_PERIOD_US * xTimeInUs)
#define pdUS_TO_TICKS(xTimeInUs) (xTimeInUs)
#define TICK_SENSOR (portTICK_PERIOD_MS*1)
#define TICK_DISPLAY 10
#define TICK_INPUT 10

#define PRIO_SENSOR 6
#define PRIO_INPUT 6
#define PRIO_DISPLAY 5

#define STK_SIZE_BASE 64
#define TASK_STK_SIZE1 (STK_SIZE_BASE*5)
#define TASK_STK_SIZE2 (STK_SIZE_BASE*3)
#define TASK_STK_SIZE3 (STK_SIZE_BASE*2)
#define TASK_STK_SIZE4 STK_SIZE_BASE

#define TASK1_STK_SIZE 256
#define TASK1_TASK_PRIO 5
#define TASK2_STK_SIZE 256
#define TASK2_TASK_PRIO 5
