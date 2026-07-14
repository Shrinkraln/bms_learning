#ifndef __I2C_H
#define __I2C_H			 
#include "stm32f10x.h"

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* Private define ------------------------------------------------------------*/

#define I2C2_SCL_PIN      					 	GPIO_Pin_10   
#define I2C2_SCL_GPIO_PORT   	       	GPIOB  
#define I2C2_SCL_GPIO_RCC      			  RCC_APB2Periph_GPIOB  
 
#define I2C2_SDA_PIN      					 	GPIO_Pin_11   
#define I2C2_SDA_GPIO_PORT   	       	GPIOB  
#define I2C2_SDA_GPIO_RCC      			  RCC_APB2Periph_GPIOB  

#define SCL2_H         	 GPIO_SetBits(I2C2_SCL_GPIO_PORT , I2C2_SCL_PIN)   /*GPIOB->BSRR = GPIO_Pin_8*/
#define SCL2_L            GPIO_ResetBits(I2C2_SCL_GPIO_PORT , I2C2_SCL_PIN) /*GPIOB->BRR  = GPIO_Pin_8 */
   
#define SDA2_H         	 GPIO_SetBits(I2C2_SDA_GPIO_PORT , I2C2_SDA_PIN)   /*GPIOB->BSRR = GPIO_Pin_9*/
#define SDA2_L         	 GPIO_ResetBits(I2C2_SDA_GPIO_PORT , I2C2_SDA_PIN) /*GPIOB->BRR  = GPIO_Pin_9*/

#define SCL2_read       	GPIO_ReadInputDataBit(I2C2_SCL_GPIO_PORT , I2C2_SCL_PIN)/* GPIOB->IDR  & GPIO_Pin_8   */
#define SDA2_read       	GPIO_ReadInputDataBit(I2C2_SDA_GPIO_PORT , I2C2_SDA_PIN)/*GPIOB->IDR  & GPIO_Pin_9	  */


/* Private function prototypes -----------------------------------------------*/
void I2C2_Configuration(void);
FunctionalState I2C2_WriteByte(uint8_t SendByte, uint16_t WriteAddress, uint8_t DeviceAddress);
FunctionalState I2C2_ReadByte(uint8_t* pBuffer,   uint16_t length,   uint16_t ReadAddress,  uint8_t DeviceAddress);
u8    IIC2_read_one_byte(u16 ReadAddr);
void  IIC2_write_one_byte(u16 WriteAddr,u8 DataToWrite);
void IIC2_write_one_byte_CRC(u16 WriteAddr,u16 DataToWrite);
#endif 
/*********************************************************************************************************
      END FILE
*********************************************************************************************************/
