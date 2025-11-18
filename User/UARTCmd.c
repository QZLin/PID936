#include "UARTCmd.h"
#include "ch32x035.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

extern u16 thVal;
extern volatile u8 parseLen;
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
void UART_SendByte(uint8_t byte) {
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART2, byte);
}

/**
 * @fn      UART_SendString
 * @brief   Send a string via UART2
 * @param   str - string to send
 * @return  none
 */
void UART_SendString(const char *str) {
    while (*str) {
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
static void ParseCommand(const uint8_t *buffer, const uint16_t length) {
    char cmd[UART_CMD_BUFFER_SIZE];
    char *argv[MAX_ARGS];
    int argc = 0;

    // Copy to local buffer
    memcpy(cmd, buffer, length);
    cmd[length] = '\0';

    // Parse arguments
    char *ptr = cmd;

    while (*ptr && argc < MAX_ARGS) {
        // Skip spaces
        while (*ptr == ' ' || *ptr == '\t')
            ptr++;

        if (*ptr == '\0')
            break;

        argv[argc] = ptr;
        argc++;

        // Find end of argument
        while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r')
            ptr++;

        if (*ptr) {
            *ptr = '\0';
            ptr++;
        }
    }

    if (argc == 0)
        return;

    // Command: echo <message>
    if (strcmp(argv[0], "echo") == 0) {
        UART_SendString("ECHO: ");
        for (int i = 1; i < argc; i++) {
            UART_SendString(argv[i]);
            if (i < argc - 1)
                UART_SendString(" ");
        }
        UART_SendString("\r\n");
    }
    // Command: version
    else if (strcmp(argv[0], "version") == 0) {
        printf("Firmware Version: %s\r\n", FIRMWARE_VERSION);
        printf("Build Date: %s\r\n", FIRMWARE_DATE);
    }
    // Command: set-value <value>
    else if (strcmp(argv[0], "set") == 0) {
        if (argc > 1) {
            int32_t value = atoi(argv[1]);
            uartManager.storedValue = value;
            printf("Value set to: %ld\r\n", uartManager.storedValue);
        } else {
            UART_SendString("Error: set-value requires an argument\r\n");
        }
    }
    // Command: get-value
    else if (strcmp(argv[0], "get") == 0) {
        const char *key = argv[1];
        if (strcmp(key, "th") == 0) {
            printf("th:%u\r\n", thVal);
        } else if (strcmp(key, "target") == 0) {
            printf("target:%u\r\n", parseLen);
        } else {
            printf("keyError\r\n");
        }
    }
    // Command: help
    else if (strcmp(argv[0], "help") == 0) {
        UART_SendString("Available commands:\r\n");
        UART_SendString("  echo <message>    - Echo back the message\r\n");
        UART_SendString("  version           - Show firmware version\r\n");
        UART_SendString("  set-value <val>   - Set a stored value\r\n");
        UART_SendString("  get-value         - Get the stored value\r\n");
        UART_SendString("  help              - Show this help message\r\n");
    } else {
        printf("Unknown command: %s\r\n", argv[0]);
        UART_SendString("Type 'help' for available commands\r\n");
    }
}

/**
 * @fn      UART_ProcessReceived
 * @brief   Process received UART data
 * @return  none
 */
void UART_ProcessReceived(void) {
    // Check if we have a complete command (line ending with \r or \n)
    if (uartManager.rxIndex > 0) {
        uint8_t lastChar = uartManager.rxBuffer[uartManager.rxIndex - 1];

        if (lastChar == '\r' || lastChar == '\n') {
            // Find actual command end (remove \r\n)
            uint16_t cmdEnd = uartManager.rxIndex;
            while (cmdEnd > 0 && (uartManager.rxBuffer[cmdEnd - 1] == '\r' ||
                                  uartManager.rxBuffer[cmdEnd - 1] == '\n')) {
                cmdEnd--;
            }

            if (cmdEnd > 0) {
                // Echo the input
                UART_SendString("\r\n");

                // Parse and execute command
                ParseCommand(uartManager.rxBuffer, cmdEnd);

                // Prompt
                UART_SendString(">>> ");
            }

            // Clear buffer
            uartManager.rxIndex = 0;
        }
    }
}

/**
 * @fn      UART_ReceiveHandler
 * @brief   Handle received byte from UART
 * @param   byte - received byte
 * @return  none
 */
void UART_ReceiveHandler(uint8_t byte) {
    if (uartManager.rxIndex < UART_RX_BUFFER_SIZE) {
        uartManager.rxBuffer[uartManager.rxIndex++] = byte;

        // Echo the character
        UART_SendByte(byte);
    } else {
        // Buffer overflow, reset
        uartManager.rxIndex = 0;
        UART_SendString("\r\nBuffer overflow!\r\n>>> ");
    }
}
