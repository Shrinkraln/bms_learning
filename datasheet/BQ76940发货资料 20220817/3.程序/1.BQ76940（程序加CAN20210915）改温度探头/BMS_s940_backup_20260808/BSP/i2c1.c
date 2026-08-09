/******************** (C) COPYRIGHT  源地工作室 ********************************
 * 文件名  ：i2c.c
 * 描述    ：初始化IIC及一些基本的操作 
 * 作者    ：zhuoyingxingyu
 * 淘宝    ：源地工作室http://vcc-gnd.taobao.com/
 * 论坛地址：极客园地-嵌入式开发论坛http://vcc-gnd.com/
 * 版本更新: 2016-04-08
 * 硬件连接: PB6-I2C1_SCL、PB7-I2C1_SDA
 * 调试方式：J-Link-OB
**********************************************************************************/

//头文件
#include "i2c1.h"
#include "usart.h"
#include "SYSTICK.h"
 

/**
  * @file   I2C1_Configuration
  * @brief  I2C1管脚配置
  * @param  无
  * @retval 无
  */
void I2C1_Configuration(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure; 
  /* Configure I2C2 pins: PB8->I2C1_SCL and PB9->I2C1_SDA */
  RCC_APB2PeriphClockCmd(I2C1_SCL_GPIO_RCC, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin =  I2C1_SCL_PIN ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;  
	GPIO_Init(I2C1_SCL_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin =  I2C1_SDA_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;  
	GPIO_Init(I2C1_SDA_GPIO_PORT, &GPIO_InitStructure);
}


 /**
  * @file   I2C_delay
  * @brief  延迟时间
  * @param  无
  * @retval 无
  */
static void I2C_delay(void)
{	
   uint8_t i=50; /* 这里可以优化速度,经测试最低到5还能写入 */
   while(i) 
   { 
     i--; 
   } 
}

 /**
  * @file   I2C_Start
  * @brief  起始信号
  * @param  无
  * @retval 无
  */
static FunctionalState I2C1_Start(void)
{
	SDA1_H;
	SCL1_H;
	I2C_delay();
	if(!SDA1_read)return DISABLE;	/* SDA线为低电平则总线忙,退出 */
	SDA1_L;
	I2C_delay();
	if(SDA1_read) return DISABLE;	/* SDA线为高电平则总线出错,退出 */
	SDA1_L;
	I2C_delay();
	return ENABLE;
}

 /**
  * @file   I2C_Stop
  * @brief  停止信号
  * @param  无
  * @retval 无
  */
static void I2C1_Stop(void)
{
	SCL1_L;
	I2C_delay();
	SDA1_L;
	I2C_delay();
	SCL1_H;
	I2C_delay();
	SDA1_H;
	I2C_delay();
}

 /**
  * @file   I2C_Ack
  * @brief  应答信号
  * @param  无
  * @retval 无
  */
static void I2C1_Ack(void)
{	
	SCL1_L;
	I2C_delay();
	SDA1_L;
	I2C_delay();
	SCL1_H;
	I2C_delay();
	SCL1_L;
	I2C_delay();
}

 /**
  * @file   I2C_NoAck
  * @brief  无应答信号
  * @param  无
  * @retval 无
  */
static void I2C1_NoAck(void)
{	
	SCL1_L;
	I2C_delay();
	SDA1_H;
	I2C_delay();
	SCL1_H;
	I2C_delay();
	SCL1_L;
	I2C_delay();
}

 /**
  * @file   I2C_WaitAck
  * @brief  等待Ack
  * @param  无
  * @retval 返回为:=1有ACK,=0无ACK
  */
static FunctionalState I2C1_WaitAck(void) 	
{
	SCL1_L;
	I2C_delay();
	SDA1_H;			
	I2C_delay();
	SCL1_H;
	I2C_delay();
	if(SDA1_read)
	{
      SCL1_L;
      return DISABLE;
	}
	SCL1_L;
	return ENABLE;
}

 /**
  * @file   I2C_SendByte
  * @brief  数据从高位到低位
  * @param  - SendByte: 发送的数据
  * @retval 无
  */
static void I2C1_SendByte(uint8_t SendByte) 
{
    uint8_t i=8;
    while(i--)
    {
        SCL1_L;
        I2C_delay();
      if(SendByte&0x80)
        SDA1_H;  
      else 
        SDA1_L;   
        SendByte<<=1;
        I2C_delay();
		SCL1_H;
        I2C_delay();
    }
    SCL1_L;
}


 /**
  * @file   I2C_ReceiveByte
  * @brief  数据从高位到低位
  * @param  无
  * @retval I2C总线返回的数据
  */
static uint8_t I2C1_ReceiveByte(void)  
{ 
    uint8_t i=8;
    uint8_t ReceiveByte=0;

    SDA1_H;				
    while(i--)
    {
      ReceiveByte<<=1;      
      SCL1_L;
      I2C_delay();
	  SCL1_H;
      I2C_delay();	
      if(SDA1_read)
      {
        ReceiveByte|=0x01;
      }
    }
    SCL1_L;
    return ReceiveByte;
}
  


u32 IIC1_read_len_byte(u16 ReadAddr,u8 Len)
{
  u8 t;
	u32 temp=0;
	for(t=0;t<Len;t++)
	{
		temp<<=8;
		temp+=IIC1_read_one_byte(ReadAddr+Len-t-1); 	 				   
	}
	return temp;

}


void  IIC1_write_one_byte(u16 WriteAddr,u8 DataToWrite)
{			
	
    I2C1_Start();  
	
		I2C1_SendByte(0X10+((WriteAddr/256)<<1));   //发送器件地址0XA0,写数据 
		 
	  I2C1_WaitAck();	   
    I2C1_SendByte(WriteAddr%256);   //发送低地址
	  I2C1_WaitAck(); 										  		   
	  I2C1_SendByte(DataToWrite);     //发送字节							   
    I2C1_WaitAck();	    	   
    I2C1_Stop();//产生一个停止条件 
	  delay_ms(10);
  
}

void  IIC1_write_len_byte(u16 WriteAddr,u8 DataToWrite,u8 Len)
{
  u8 t;
	for(t=0;t<Len;t++)
	{
		IIC1_write_one_byte(WriteAddr+t,(DataToWrite>>(8*t))&0xff);
	}												    

}




void IIC1_write_one_byte_CRC(u16 WriteAddr,u16 DataToWrite)
{
		
  
  unsigned char DataBuffer[4];
	
  DataBuffer[0] = 0X08 << 1;
	DataBuffer[1] = WriteAddr;
	DataBuffer[2] = DataToWrite;
	DataBuffer[3] = CRC8(DataBuffer, 3, 7);
	
    I2C1_Start();  	
		I2C1_SendByte( DataBuffer[0]);   //发送器件地址0XA0,写数据 		 
	  I2C1_WaitAck();	   
    I2C1_SendByte(DataBuffer[1]);   //发送地址
	  I2C1_WaitAck(); 										  		   
	  I2C1_SendByte(DataBuffer[2]);     //发送字节							   
    I2C1_WaitAck();	
	  I2C1_SendByte(DataBuffer[3]);     //发送CRC
	  I2C1_WaitAck();
    I2C1_Stop();//产生一个停止条件 	  
	  delay_ms(10);
}


u8 IIC1_read_one_byte(u16 ReadAddr)
{				  
	u8 temp=0;		  	    																 
  I2C1_Start();  
	I2C1_SendByte(0x08 << 1) ;   //发送器件地址0XA0,写数据 	 
	I2C1_WaitAck();
  I2C1_SendByte(ReadAddr);   //发送低地址
	I2C1_WaitAck();    
	I2C1_Start();  	 	   
	I2C1_SendByte(0X11);           //进入接收模式			   
	I2C1_WaitAck(); 
    temp=I2C1_ReceiveByte();		   
    I2C1_Stop();//产生一个停止条件
    return temp;
}

 /**
  * @file   I2C_WriteByte
  * @brief  写一字节数据
  * @param  
	*          - SendByte: 待写入数据
	*          - WriteAddress: 待写入地址
	*          - DeviceAddress: 器件类型(24c16或SD2403)
  * @retval 返回为:=1成功写入,=0失败
  */
FunctionalState I2C1_WriteByte(uint8_t SendByte, uint16_t WriteAddress, uint8_t DeviceAddress)
{		
    if(!I2C1_Start())return DISABLE;
    I2C1_SendByte((((WriteAddress & 0x0700) >>7) | DeviceAddress) & 0xFFFE); /*设置高起始地址+器件地址 */
    if(!I2C1_WaitAck()){I2C1_Stop(); return DISABLE;}
    I2C1_SendByte((uint8_t)(WriteAddress & 0x00FF));   /* 设置低起始地址 */      
    I2C1_WaitAck();	
    I2C1_SendByte(SendByte);
    I2C1_WaitAck();   
    I2C1_Stop(); 
    return ENABLE;
}									 

 /**
  * @file   I2C_ReadByte
  * @brief  读取一串数据
  * @param  
	*					- pBuffer: 存放读出数据
	*     	  - length: 待读出长度
	*         - ReadAddress: 待读出地址
	*         - DeviceAddress: 器件类型(24c16或SD2403)
  * @retval 返回为:=1成功读入,=0失败
  */
FunctionalState I2C1_ReadByte(uint8_t* pBuffer,   uint16_t length,   uint16_t ReadAddress,  uint8_t DeviceAddress)
{		
    if(!I2C1_Start())return DISABLE;
    I2C1_SendByte((((ReadAddress & 0x0700) >>7) | DeviceAddress) & 0xFFFE); /* 设置高起始地址+器件地址 */ 
    if(!I2C1_WaitAck()){I2C1_Stop(); return DISABLE;}
    I2C1_SendByte((uint8_t)(ReadAddress & 0x00FF));   /* 设置低起始地址 */      
    I2C1_WaitAck();
    I2C1_Start();
    I2C1_SendByte(((ReadAddress & 0x0700) >>7) | DeviceAddress | 0x0001);
    I2C1_WaitAck();
    while(length)
    {
      *pBuffer = I2C1_ReceiveByte();
      if(length == 1)I2C1_NoAck();
      else I2C1_Ack(); 
      pBuffer++;
      length--;
    }
    I2C1_Stop();
    return ENABLE;
}






/*********************************************************************************************************
      END FILE
*********************************************************************************************************/
