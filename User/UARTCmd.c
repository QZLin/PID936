#include <stdio.h>
#include <string.h>
#include "UARTCmd.h"

#include <stdlib.h>

#include "ch32x035.h"
#include "Parameter.h"

extern volatile u16 thVal;
extern volatile u16 dstVal;

extern u16 x0Count;
extern volatile u8 triggerPeriod;

UART_Manager_t uartManager = {0};

// Version information
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_DATE "2025-11-13"

/**
 * @fn      UART_SendByte
 * @brief   Send a single byte via UART2
 * @param   byte - byte to send
 * @return  none
 */
void UART_SendByte(const uint8_t byte) {
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART2, byte);
}

/**
 * @fn      UART_SendString
 * @brief   Send a string via UART2
 * @param   str - string to send
 * @return  none
 */
void UART_SendString(const char* str) {
    while(*str) {
        UART_SendByte(*str++);
    }
}

/**
 * @fn      UART_ReceiveInit
 * @brief   Initialize UART2 receive interrupt
 * @return  none
 */
void UART_ReceiveInit(void) {
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    // Enable USART2 interrupt
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    // Configure NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // Clear buffer
    uartManager.rxIndex = 0;
    uartManager.cmdLength = 0;
}

/**
 * @fn      ParseCommand
 * @brief   Parse command from received buffer
 * @param   buffer - command string
 * @param   length - command length
 * @return  none
 */
static void ParseCommand(const uint8_t* buffer, const uint16_t length) {
    char cmd[UART_CMD_BUFFER_SIZE];
    char* argv[MAX_ARGS];
    int argc = 0;

    // Copy to local buffer
    memcpy(cmd, buffer, length);
    cmd[length] = '\0';

    // Parse arguments
    char* ptr = cmd;

    while(*ptr && argc < MAX_ARGS) {
        // Skip spaces
        while(*ptr == ' ' || *ptr == '\t')
            ptr++;

        if(*ptr == '\0')
            break;

        argv[argc] = ptr;
        argc++;

        // Find end of argument
        while(*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r')
            ptr++;

        if(*ptr) {
            *ptr = '\0';
            ptr++;
        }
    }

    if(argc == 0)
        return;

    // Command: echo <message>
    if(strcmp(argv[0], "echo") == 0) {
        for(int i = 1; i < argc; i++) {
            UART_SendString(argv[i]);
            if(i < argc - 1)
                UART_SendString(" ");
        }
        UART_SendString("\r\n");
    }
    // Command: version
    else if(strcmp(argv[0], "ver") == 0) {
        printf("ver: %s\r\n", FIRMWARE_VERSION);
        printf("date: %s\r\n", FIRMWARE_DATE);
    }
    // Command: set-value <value>
    else if(strcmp(argv[0], "set") == 0) {
        if(argc > 2) {
            const char* key = argv[1];
            char* unused_ptr;
            const int16_t value = (int16_t)strtol(argv[2], &unused_ptr, 10);
            printf("%s=%d", key, value);
        }
        else {
            printf("Err: set <key> <val>\r\n");
        }
    }
    // Command: get-value
    else if(strcmp(argv[0], "get") == 0) {
        const char* key = argv[1];
        if(strcmp(key, "th") == 0) {
            printf("th:%u\r\n", thVal);
        }
        else if(strcmp(key, "dst") == 0) {
            printf("dst:%u\r\n", dstVal);
        }
        else if(strcmp(key, "period") == 0) {
            printf("period:%u\r\n", triggerPeriod);
        }
        else if(strcmp(key, "x0") == 0) {
            printf("x0:%u\r\n", x0Count);
        }
        else {
            printf("keyErr\r\n");
        }
    }
    // Command: help
    else if(strcmp(argv[0], "help") == 0) {
        printf("help echo ver get set ");
    }
    else {
        printf("Unknown command: %s\r\n", argv[0]);
    }
}

/**
 * @fn      UART_ProcessReceived
 * @brief   Process received UART data
 * @return  none
 */
void UART_ProcessReceived(void) {
    // Check if we have a complete command (line ending with \r or \n)
    if(uartManager.rxIndex > 0) {
        const uint8_t lastChar = uartManager.rxBuffer[uartManager.rxIndex - 1];
        if(lastChar == '\r' || lastChar == '\n') {
            // Find actual command end (remove \r\n)
            uint16_t cmdEnd = uartManager.rxIndex;
            while(cmdEnd > 0 && (uartManager.rxBuffer[cmdEnd - 1] == '\r' ||
                uartManager.rxBuffer[cmdEnd - 1] == '\n')) {
                cmdEnd--;
            }

            if(cmdEnd > 0) {
                UART_SendString("\r\n"); // Echo the input
                ParseCommand(uartManager.rxBuffer, cmdEnd); // Parse and execute command
                UART_SendString(">>> "); // Prompt
            }
            uartManager.rxIndex = 0; // Clear buffer
        }
    }
}

/**
 * @fn      UART_ReceiveHandler
 * @brief   Handle received byte from UART
 * @param   byte - received byte
 * @return  none
 */
void UART_ReceiveHandler(const uint8_t byte) {
    if(uartManager.rxIndex < UART_RX_BUFFER_SIZE) {
        uartManager.rxBuffer[uartManager.rxIndex++] = byte;
        // Echo the character
        // UART_SendByte(byte);
    }
    else {
        // Buffer overflow, reset
        uartManager.rxIndex = 0;
        UART_SendString("\r\nBuffer overflow!\r\n>>> ");
    }
}
