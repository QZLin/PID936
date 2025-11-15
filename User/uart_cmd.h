#pragma once

#include "stdint.h"

#define UART_RX_BUFFER_SIZE 128
#define UART_CMD_BUFFER_SIZE 64
#define MAX_ARGS 8

typedef struct
{
    uint8_t rxBuffer[UART_RX_BUFFER_SIZE];
    uint16_t rxIndex;
    uint8_t cmdBuffer[UART_CMD_BUFFER_SIZE];
    uint16_t cmdLength;
    int32_t storedValue;
} UART_Manager_t;

extern UART_Manager_t uartManager;

void UART_ReceiveInit(void);
void UART_ReceiveHandler(uint8_t byte);
void UART_ProcessReceived(void);
void UART_SendString(const char* str);
void UART_SendByte(uint8_t byte);
