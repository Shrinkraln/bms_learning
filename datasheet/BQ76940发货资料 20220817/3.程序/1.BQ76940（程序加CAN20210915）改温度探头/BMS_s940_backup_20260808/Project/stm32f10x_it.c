/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "BQ76930.h"
#include "usart.h"
#include "wdg.h"
/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
u16 nTime;
void SysTick_Handler(void)
{
	if(nTime)
	nTime--;
}





/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 
//int count;
//void TIM2_IRQHandler(void)   //TIM2中断
//{   
//  if( TIM_GetITStatus( TIM2, TIM_IT_Update ) != RESET )	
//	{
//	  TIM_ClearITPendingBit(TIM2, TIM_IT_Update);  //清除TIMx更新中断标志
//		 	  count++;
//      if(count == 10)
//			  {
//			  ALERT_1_Recognition();				
//			  count =0;
//			  }          			
//	}  
//}

unsigned char ucUSART1_ReceiveDataBuffer[ 0x0400  ] = {0};
volatile unsigned short usUSART1ReceiveDataLength;
unsigned short usUSART1_ReceiveDataIndex = 0x0000;
unsigned short usUSART1_LastDataIndex = 0x0000;
 void USART1_IRQHandler( void )
{
  const unsigned short usValidDataLength1 =0x0400 - 0x0000;
  if( USART_GetITStatus( USART1, USART_IT_RXNE ) != RESET )
  {
    if( usUSART1_ReceiveDataIndex < usValidDataLength1 )
    {
      ucUSART1_ReceiveDataBuffer[ usUSART1_ReceiveDataIndex++ ] = ( unsigned char )USART1->DR;
			
    } 
  } 
}
int cnt3,cnt4,n,Heartbeat,cnt;
 u8 cnt1_flag =0 ;
 u8  Heartbeat_flag=0;
extern unsigned char Cell_voltage1[];
extern int Heart_big;
void TIM2_IRQHandler( void )
{
	  
  if( TIM_GetITStatus( TIM2, TIM_IT_Update ) != RESET )
  {
	  TIM_ClearITPendingBit( TIM2, TIM_IT_Update );
		
		
		
		if( usUSART1_ReceiveDataIndex <= DUMMY_BYTES || Get_USART1_StopFlag() == USART1_STOP_TRUE )
    {
      return;
    }
    else
    {
      
      if( usUSART1_ReceiveDataIndex == usUSART1_LastDataIndex )
      {
        // Set USART stop receive flag
        Set_USART1_StopFlag( USART1_STOP_TRUE );
        // Reset usUSART_ReceiveDataIndex
        usUSART1_ReceiveDataIndex = DUMMY_BYTES;
        // Reset usUSART_LastDataIndex
        usUSART1_LastDataIndex = 0x0000;
      }
      else
      {
        // Update usUSART_LastDataIndex
        usUSART1_LastDataIndex = usUSART1_ReceiveDataIndex;
      } // end if...else
    } // end if...else
  } // end if
}
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
