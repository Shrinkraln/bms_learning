#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

#define LED1                     1
#define LED2                     2
#define LED3                     3
#define LED4                     4

#define LED1_GPIO_RCC           RCC_APB2Periph_GPIOA
#define LED1_GPIO_PORT          GPIOA
#define LED1_GPIO_PIN      			GPIO_Pin_15
#define LED1_ONOFF(x)     			GPIO_WriteBit(LED1_GPIO_PORT,LED1_GPIO_PIN,x);

#define LED2_GPIO_RCC           RCC_APB2Periph_GPIOB
#define LED2_GPIO_PORT          GPIOB
#define LED2_GPIO_PIN      			GPIO_Pin_13
#define LED2_ONOFF(x)     			GPIO_WriteBit(LED2_GPIO_PORT,LED2_GPIO_PIN,x);

#define LED3_GPIO_RCC           RCC_APB2Periph_GPIOB
#define LED3_GPIO_PORT          GPIOB
#define LED3_GPIO_PIN      			GPIO_Pin_14
#define LED3_ONOFF(x)      			GPIO_WriteBit(LED3_GPIO_PORT,LED3_GPIO_PIN,x);

#define LED4_GPIO_RCC           RCC_APB2Periph_GPIOB
#define LED4_GPIO_PORT          GPIOB
#define LED4_GPIO_PIN      			GPIO_Pin_15
#define LED4_ONOFF(x)      			GPIO_WriteBit(LED4_GPIO_PORT,LED4_GPIO_PIN,x);


void LED_GPIO_Config(void);	
void LEDXToggle(uint8_t ledx);
#endif
