#pragma once
#define PORT_SER595 GPIOA
#define PIN_SER595 GPIO_Pin_6
#define PORT_ST595 GPIOA
#define PIN_ST595 GPIO_Pin_7
#define PORT_SH595 GPIOB
#define PIN_SH595 GPIO_Pin_1

// #define PORT_165 GPIOA
#define PORT_LD165 GPIOA
#define PIN_LD165 GPIO_Pin_5
#define PORT_CLK165 GPIOA
#define PIN_CLK165 GPIO_Pin_4
#define PORT_SER165 GPIOC
#define PIN_SER165 GPIO_Pin_3

#define UART_PORT GPIOA
#define RX_PIN GPIO_Pin_3
#define TX_PIN GPIO_Pin_2

/// cross zero detection
#define PORT_X0 GPIOA
#define PIN_X0 GPIO_Pin_11
#define PORT_SRC_X0 GPIO_PortSourceGPIOA
#define PIN_SRC_X0 GPIO_PinSource11
#define LINE_X0 EXTI_Line11
#define IRQ_X0 EXTI15_8_IRQn

#define PORT_CONTROL GPIOA
#define PIN_CONTROL GPIO_Pin_9

#define PORT_DS18B20 GPIOA
#define PIN_DS18B20 GPIO_Pin_10

#define PORT_ADC_TEMP GPIOA
#define PIN_ADC_TEMP GPIO_Pin_0
#define ADC_TEMP ADC1
#define ADC_CH_TEMP ADC_Channel_0

#define PORT_ADC_W GPIOA
#define PIN_ADC_W GPIO_Pin_1
#define ADC_W ADC1
#define ADC_CH_W ADC_Channel_1

#define LED_TEMP  0b01000000
#define LED_PWR   0b10000000
#define LED_WARN  0b00000010
#define LED_READY 0b00000001

#define BTN_DBG 0b10000000
#define BTN_PWR 0b00000001
#define BTN_RST 0b00000010
#define BTN_SUB 0b00000100
#define BTN_ADD 0b00001000
