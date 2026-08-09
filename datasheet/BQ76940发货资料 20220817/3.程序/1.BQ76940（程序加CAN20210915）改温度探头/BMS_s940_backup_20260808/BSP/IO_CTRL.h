#ifndef __IO_CTRL_H
#define __IO_CTRL_H

#include "stm32f10x.h"

#define GPIOA_RCC                 RCC_APB2Periph_GPIOA
#define GPIOA_PORT                GPIOA
#define MCU_KZ_QB_POWER           GPIO_Pin_0
#define MCU_KZ_QB_POWER_ONOFF(x)  GPIO_WriteBit(GPIOA_PORT ,MCU_KZ_QB_POWER,x);

#define GPIOA_RCC                 RCC_APB2Periph_GPIOA
#define GPIOA_PORT                GPIOA
#define MCU_D_POWER               GPIO_Pin_1
#define MCU_D_POWER_ONOFF(x)      GPIO_WriteBit(GPIOA_PORT ,MCU_D_POWER,x);

#define GPIOA_RCC                 RCC_APB2Periph_GPIOA
#define GPIOA_PORT                GPIOA
#define MCU_WAKE_BQ               GPIO_Pin_8
#define MCU_WAKE_BQ_ONOFF(x)      GPIO_WriteBit(GPIOA_PORT ,MCU_WAKE_BQ,x);

#define GPIOA_RCC                 RCC_APB2Periph_GPIOA
#define GPIOA_PORT                GPIOA
#define MCU_KZ_P2                 GPIO_Pin_12
#define MCU_KZ_P2_ONOFF(x)        GPIO_WriteBit(GPIOA_PORT ,MCU_KZ_P2,x);



#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define MCU_IN_STA1               GPIO_Pin_12
#define MCU_IN_STA1_ONOFF(x)      GPIO_WriteBit(GPIOB_PORT ,MCU_IN_STA1,x);

#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define STA_WAKE                  GPIO_Pin_1
#define STA_WAKE_ONOFF(x)         GPIO_WriteBit(GPIOB_PORT ,STA_WAKE,x);

#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define MCU_BQ1_ART               GPIO_Pin_2
#define MCU_BQ1_ART_ONOFF(x)      GPIO_WriteBit(GPIOB_PORT ,MCU_BQ1_ART,x);

#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define PB3                       GPIO_Pin_3
#define PB3_ONOFF(x)              GPIO_WriteBit(GPIOB_PORT ,PB3 ,x);

#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define PB4                       GPIO_Pin_4
#define PB4_ONOFF(x)              GPIO_WriteBit(GPIOB_PORT ,PB4 ,x);

#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define PB5                       GPIO_Pin_5
#define PB5_ONOFF(x)              GPIO_WriteBit(GPIOB_PORT ,PB5 ,x);



void IO_CTRL_Config(void);

#endif
