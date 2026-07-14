/******************** (C) BOBO   ********************************
 * 文件名  ：main.c
 * 描述    ：主要是完成BMS相关检测和保护
 * 库版本  ：V3.50
 * 作者    ：BOBO
 * 版本更新: 2019-04-12
 * 调试方式：J-Link
**********************************************************************************/

//头文件
#include "stm32f10x.h"
#include "led.h"
#include "wdg.h"
#include "SYSTICK.h"
#include "usart.h"
#include "usart2.h"
#include "i2c.h"
#include "i2c1.h"
#include "i2c2.h"
#include "BQ76930.h"
#include "IO_CTRL.h"
#include <stdio.h>
#include "math.h"
#include "timer.h"
#include "stm32f10x_it.h"
#include "w25qxx.h"
#include "spi.h"
#include "can.h"
/**
  * @file   main
  * @brief  Main program.
  * @param  None
  * @retval None
  */
	
/* 存储数据Flash页首地址 60K 为首地址，即留给程序代码的空间有60K大小，103C8T6的FLASH的大小有62K*，每个地址递增一页，也就是+0X802*/
    uint32_t   Battery_ADR     =  0x08007800 ;  
    uint32_t   Temp_up         =  0x08007C00 ; 		
		
 
		 
	void Write_Flash(uint32_t Add,unsigned int Data)
	{	
		  FLASH_Unlock();		/* 每次擦除Flash中数据时得先解锁 */
      FLASH_ErasePage(Add);		  /* 擦除页 */		
		  FLASH_ProgramWord(Add,Data);   /* 写16位半字 */
		  FLASH_Lock();							   /* 上锁 */		  
	}
	
unsigned int Read_Flash(uint32_t Add)
	{	
		unsigned int Data;
		Data=*(vu16*)(Add);	 /* 读取地址中的16位数据 */ 
		return Data;
	}		
unsigned char SEND_readparameter_TO_SHANGWEIJI[500]={0XAA,0X88};
	

extern unsigned char ucUSART1_ReceiveDataBuffer[];
unsigned char BMS_DATA_FLAG;
void RECEICE_DATA_DEAL(void)
{
	int i,n,t,u,r,Crc_result;
	unsigned char p_USART1_ReceiveDataBuffer[10];
	
if( Get_USART1_StopFlag() == USART1_STOP_TRUE)  //????????????
  {
		
		if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X02)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			BMS_DATA_FLAG=1;				
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X03)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			BMS_DATA_FLAG=0;				
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X04)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Open_DSG	();		
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X05)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Close_DSG();			
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X06)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Open_CHG	();		
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X07)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Close_CHG();			
		 }
		 
		 
//		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X10)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
//		 {
//       LEDXToggle(5);
//			 FUN_POWER_EN_ONOFF(1);			
//		 }
//		 
//		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X11)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
//		 {
//       LEDXToggle(5);
//			FUN_POWER_EN_ONOFF(0);			
//		 }
//		 
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X12)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
       Write_Flash(Battery_ADR ,(ucUSART1_ReceiveDataBuffer[3]<<8) +ucUSART1_ReceiveDataBuffer[4]);							
		 }
		 
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X13)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
       Write_Flash(Temp_up ,(ucUSART1_ReceiveDataBuffer[3]<<8) +ucUSART1_ReceiveDataBuffer[4]);							
		 }
		 
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X14)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
       SEND_readparameter_TO_SHANGWEIJI[2]=(Read_Flash(Battery_ADR)>>8);
       SEND_readparameter_TO_SHANGWEIJI[3]= Read_Flash(Battery_ADR)&0X00FF;
       SEND_readparameter_TO_SHANGWEIJI[4]=(Read_Flash(Temp_up)>>8);
       SEND_readparameter_TO_SHANGWEIJI[5]= Read_Flash(Temp_up)&0X00FF;				 
			 USART1_Printf( SEND_readparameter_TO_SHANGWEIJI,50, ASCII_CODE );								

		 }
		 
		 
		 
	}
			Set_USART1_StopFlag( USART1_STOP_FALSE );
	}
	
	u8 key,u;	
	u8 canbuf[8];
	
	const u8 TEXT_Buffer[]={0,1};
#define SIZE sizeof(TEXT_Buffer)
	u8 datatemp[SIZE]ACTIVATE_USART1,Temp_up_flag,OV_FLAG,UV_FLAG,OC_FLAG;
	u32 FLASH_SIZE = 16*1024*1024;
extern UV_Alarm_flag,OV_Alarm_flag,SCD_Alarm_flag,OCD_Alarm_flag,OT_Alarm_flag,UT_Alarm_flag;
extern int Batteryval[50];
int main(void)
{
    SYSTICK_Init(); //系统初始化，时钟配置；
	  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
    delay_ms(1000);
	  uart_init(115200);	 //串口初始化为115200   
	  USART2_Config();    //蓝牙串口初始化为9600
    LED_GPIO_Config();//电量显示，4个LED设置；
    IO_CTRL_Config(); //系统的一些IO口设置；	   
	  I2C1_Configuration();  //BQ76940_1的IIC配置；
	  CAN_Mode_Init(CAN_SJW_1tq,CAN_BS2_8tq,CAN_BS1_9tq,4,CAN_Mode_Normal);//CAN初始化环回模式,波特率500Kbps    

	  BQ76930_config();      //BQ76940的初始化，唤醒设备，OV,UV,SCD,OCD的配置；	
	  LED4_ONOFF(1) ;
   	TIM2_Config(99,7199);//100mS定时器中断
	  UartSend("MODE_CFG(1);DIR(1);FSIMG(2097152,0,0,220,176,0);\r\n");
	  delay_ms(1000);   	
    UartSend("CLR(61);\r\n");
	  IWDG_Init(6,1250);      //看门狗4S左右
    while (1)
    {	
	   IWDG_Feed();
		 LEDXToggle(1);
		key=Can_Receive_Msg(canbuf);
		if(key)//接收到有数据
		{			
 			for(u=0;u<key;u++)
			{
						 LEDXToggle(1);

 			}
		}
		 RECEICE_DATA_DEAL();
	   Get_Update_Data();
			if((Batteryval[0]>4200)||(Batteryval[1]>4200)||(Batteryval[4]>4200)||(Batteryval[5]>4200)||(Batteryval[6]>4200)||(Batteryval[9]>4200)||(Batteryval[10]>4200)||(Batteryval[11]>4200)||(Batteryval[14]>4200))
					{
						Only_Close_CHG();                        //
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						OV_FLAG=1;
					}
     if(OV_FLAG==1)
		 {
					if((Batteryval[0]<4100)&&(Batteryval[1]<4100)&&(Batteryval[4]<4100)&&(Batteryval[5]<4100)&&(Batteryval[6]<4100)&&(Batteryval[9]<4100)&&(Batteryval[10]<4100)&&(Batteryval[11]<4100)&&(Batteryval[14]<4100))
					{
						Only_Open_CHG();                         //
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						OV_FLAG=0;
					}
			}
		 
			
					if((Batteryval[0]<2800)||(Batteryval[1]<2800)||(Batteryval[4]<2800)||(Batteryval[5]<2800)||(Batteryval[6]<2800)||(Batteryval[9]<2800)||(Batteryval[10]<2800)||(Batteryval[11]<2800)||(Batteryval[14]<2800))
					{
						Only_Close_DSG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						UV_FLAG=1;
					}
          if(UV_FLAG==1)
					{
					if((Batteryval[0]>2800)&&(Batteryval[1]>2800)&&(Batteryval[4]>2800)&&(Batteryval[5]>2800)&&(Batteryval[6]>2800)&&(Batteryval[9]>2800)&&(Batteryval[10]>2800)&&(Batteryval[11]>2800)&&(Batteryval[14]>2800))
					{
						Only_Open_DSG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
            UV_FLAG=0;						
					}
		     }
			if(Batteryval[17]>2000)//如果电流大于2000ma，关闭充放电MOS管
			{
			      Close_DSG_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态	
				    OC_FLAG=1;
			}
			if( OC_FLAG==1)
			{
						if(Batteryval[17]<2000)//如果电流大于2000ma，关闭充放电MOS管
						{
									Open_DSG_CHG();
									IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态	
									OC_FLAG=0;
						}
						
			}
			
						if(Batteryval[18]>Read_Flash(Temp_up))//如果电流大于2000ma，关闭充放电MOS管
			{
			      Close_DSG_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态	

				    Temp_up_flag = 1;
			}
			if(Temp_up_flag == 1)
			{
			  if(Batteryval[18]<Read_Flash(Temp_up))
				{
					  Open_DSG_CHG();
											IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态	

				    Temp_up_flag = 0;
					
				}
			
			}

			
		}
}
/*********************************************************************************************************
      END FILE
*********************************************************************************************************/





