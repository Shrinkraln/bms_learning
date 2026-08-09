#ifndef __I2C1_H
#define __I2C1_H			 
#include "stm32f10x.h"

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Private define ------------------------------------------------------------*/


#define I2C1_SCL_PIN      					 	GPIO_Pin_8   
#define I2C1_SCL_GPIO_PORT   	       	GPIOB  
#define I2C1_SCL_GPIO_RCC      			  RCC_APB2Periph_GPIOB  
 
#define I2C1_SDA_PIN      					 	GPIO_Pin_9   
#define I2C1_SDA_GPIO_PORT   	       	GPIOB  
#define I2C1_SDA_GPIO_RCC      			  RCC_APB2Periph_GPIOB  

#define SCL1_H         	 GPIO_SetBits(I2C1_SCL_GPIO_PORT , I2C1_SCL_PIN)   /*GPIOB->BSRR = GPIO_Pin_8*/
#define SCL1_L            GPIO_ResetBits(I2C1_SCL_GPIO_PORT , I2C1_SCL_PIN) /*GPIOB->BRR  = GPIO_Pin_8 */
   
#define SDA1_H         	 GPIO_SetBits(I2C1_SDA_GPIO_PORT , I2C1_SDA_PIN)   /*GPIOB->BSRR = GPIO_Pin_9*/
#define SDA1_L         	 GPIO_ResetBits(I2C1_SDA_GPIO_PORT , I2C1_SDA_PIN) /*GPIOB->BRR  = GPIO_Pin_9*/

#define SCL1_read       	GPIO_ReadInputDataBit(I2C1_SCL_GPIO_PORT , I2C1_SCL_PIN)/* GPIOB->IDR  & GPIO_Pin_8   */
#define SDA1_read       	GPIO_ReadInputDataBit(I2C1_SDA_GPIO_PORT , I2C1_SDA_PIN)/*GPIOB->IDR  & GPIO_Pin_9	  */


/* Private function prototypes -----------------------------------------------*/
void I2C1_Configuration(void);
FunctionalState I2C1_WriteByte(uint8_t SendByte, uint16_t WriteAddress, uint8_t DeviceAddress);
FunctionalState I2C1_ReadByte(uint8_t* pBuffer,   uint16_t length,   uint16_t ReadAddress,  uint8_t DeviceAddress);
u8 IIC1_read_one_byte(u16 ReadAddr);
void  IIC1_write_one_byte(u16 WriteAddr,u8 DataToWrite);
void IIC1_write_one_byte_CRC(u16 WriteAddr,u16 DataToWrite);
u8 IIC1_read_one_byte(u16 ReadAddr);
#endif 
/*********************************************************************************************************
      END FILE
*********************************************************************************************************/
