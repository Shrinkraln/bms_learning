#ifndef __TIMER_H
#define __TIMER_H
#include "stm32f10x.h"

void TIM2_Config(u16 arr,u16 psc);
void TIM3_Config(u16 arr,u16 psc);
void Timer_2Nvic_Config(void);
void Timer_3Nvic_Config(void);
void TIM2_IRQHandler(void);
#endif
