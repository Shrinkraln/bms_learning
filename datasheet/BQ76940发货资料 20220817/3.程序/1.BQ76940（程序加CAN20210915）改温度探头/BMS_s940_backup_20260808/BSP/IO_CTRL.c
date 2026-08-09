/******************** (C) COPYRIGHT  源地工作室 ********************************
 * 文件名  ：IO_CTRL.c
 * 描述    ：IO口引脚配置     
 * 作者    ：zhuoyingxingyu
 * 淘宝    ：源地工作室http://vcc-gnd.taobao.com/
 * 论坛地址：极客园地-嵌入式开发论坛http://vcc-gnd.com/
 * 版本更新: 2016-04-08
 * 硬件连接: D1->PC13;D2->PB0;D3->PB1
 * 调试方式：J-Link-OB
**********************************************************************************/	

//头文件
#include "IO_CTRL.h"

 /**
  * @file   GPIO_Config
  * @brief  IO口引脚配置
  * @param  无
  * @retval 无
  */
void IO_CTRL_Config(void)
{	
    //定义一个GPIO_InitTypeDef 类型的结构体
    GPIO_InitTypeDef  GPIO_InitStructure;	
    RCC_APB2PeriphClockCmd(GPIOA_RCC| GPIOB_RCC,ENABLE);//使能GPIO的外设时钟
	  
	  
    /*D1*/
    GPIO_InitStructure.GPIO_Pin = MCU_KZ_QB_POWER ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOA_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
    /*D2*/
    GPIO_InitStructure.GPIO_Pin =MCU_D_POWER ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 			 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA_PORT, &GPIO_InitStructure);
    /*D3*/
    GPIO_InitStructure.GPIO_Pin =MCU_WAKE_BQ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 			 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA_PORT, &GPIO_InitStructure);
	
		
		GPIO_InitStructure.GPIO_Pin = MCU_IN_STA1 ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOB_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
		MCU_IN_STA1_ONOFF(1);
		GPIO_InitStructure.GPIO_Pin = STA_WAKE  ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOB_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
		
		GPIO_InitStructure.GPIO_Pin = MCU_BQ1_ART  ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOB_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
		
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);//要先开时钟，再重映射；这句表示关闭jtag，使能swd。
		
		
		GPIO_InitStructure.GPIO_Pin = MCU_KZ_P2;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOA_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
		MCU_KZ_P2_ONOFF(0);
		
		GPIO_InitStructure.GPIO_Pin =  PB3  ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOB_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
		PB3_ONOFF(0);
		
		GPIO_InitStructure.GPIO_Pin =  PB4  ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOB_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
		PB4_ONOFF(0);
		
		GPIO_InitStructure.GPIO_Pin =  PB5  ;//选择要用的GPIO引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //设置引脚模式为推免输出模式						 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//设置引脚速度为50MHZ         
    GPIO_Init(GPIOB_PORT, &GPIO_InitStructure);//调用库函数，初始化GPIO
	  PB5_ONOFF(0);
		
			
}


