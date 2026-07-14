#include "BQ76930.h"
#include "SYSTICK.h" 
#include "i2c.h"
#include "i2c1.h"
#include "i2c2.h"
#include "usart.h"
#include "math.h"
#include "string.h"
#include "IO_CTRL.h"
#include "usart2.h"
#include "can.h"
#include "led.h"
/*****************************
file:BQData.c
decription:
   1.采集BQ76930的14位ADC的增益和偏置电压:void get_offset(void)
   2.采集单体电压:void Get_Batteryx(void),其中x取值1-10；
   3.BQ76930初始化 void BQ_config(void)
   4.读取BQ76930寄存器值： void readbqstate(void)
   5.
******************************/
/***********************
fuction:void get_offset(void)
************************/
int ADC_offset,GAIN;
float ADC_GAIN = 0;

void Get_offset(void)
{
 unsigned char gain[2];
 
 gain[0]=IIC1_read_one_byte(ADCGAIN1);//ADC_GAIN1
 gain[1]=IIC1_read_one_byte(ADCGAIN2);//ADC_GAIN2
 ADC_GAIN = ((gain[0]&0x0c)<<1)+((gain[1]&0xe0)>>5);//12uV
 ADC_offset=IIC1_read_one_byte(ADCOFFSET);//45mV
 GAIN = 365+ADC_GAIN;//GAIN=377uV
}
/****************************************
fuction: void Get_Battery1(void)
description:获取第1号单体电池电压
Parameters:batterylval[0],battery1val[1];
******************************************/
void BQ76930_config(void)
{
	  WAKE_ALL_DEVICE(); 
    BQ_1_2_config();
	  Get_offset();
    OV_UV_1_2_PROTECT();
    OCD_SCD_PROTECT();  
	  //Open_DSG_CHG();  
	 
	  IIC1_write_one_byte_CRC(SYS_STAT,0xFF);  
}

void BQ_1_2_config(void)
{

    BQ_1_config();//
}

void OV_UV_1_2_PROTECT(void)
{
    OV_UV_1_PROTECT();
}

void WAKE_ALL_DEVICE(void)
{
	  MCU_WAKE_BQ_ONOFF(1);
	  delay_ms(100);
		MCU_WAKE_BQ_ONOFF(0);

}


void SHIP_ALL_DEVICE(void)
{
	IIC1_write_one_byte_CRC(SYS_STAT,0xFF);
}



 int Batteryval[50]={0};
unsigned char shang[500]={0xAA,0x01};
unsigned char shang1[50]={0xAA,0x02};
unsigned char shang2[50]={0xAA,0x03};
unsigned char shang3[50]={0xAA,0x04};

unsigned char can_buf1[8]={0xAA,0x01};
unsigned char can_buf2[8]={0xAA,0x02};
unsigned char can_buf3[8]={0xAA,0x03};
unsigned char can_buf4[8]={0xAA,0x04};
unsigned char can_buf5[8]={0xAA,0x05};
unsigned char can_buf6[8]={0xAA,0x06};
unsigned char can_buf7[8]={0xAA,0x07};


 void Get_Battery1(void)
{
  unsigned int readbattbuf[2];
  unsigned int battery1val[2];
  short batteryval1;
	
  readbattbuf[1] = IIC1_read_one_byte(0x0c);
  readbattbuf[0] = IIC1_read_one_byte(0x0d);
 
  batteryval1= readbattbuf[1];
  batteryval1= (batteryval1 << 8) |readbattbuf[0];
  batteryval1=((batteryval1*GAIN)/1000)+ADC_offset;//单体电压计算公式，第1串
  Batteryval[0]=batteryval1;
	
  battery1val[1]=(char)(batteryval1 >> 8);
  battery1val[0]=(char)(batteryval1 & 0x00FF);
	
	shang[2]=battery1val[1];
	shang[3]=battery1val[0];
	
	shang1[2]=battery1val[1];
	shang1[3]=battery1val[0];
	
	can_buf1[2]=battery1val[1];
	can_buf1[3]=battery1val[0];

}


/****************************************
fuction: void Get_Battery2(void)
description:获取第2号单体电池电压
Parameters:battery2val[0],battery2val[1];
******************************************/
void Get_Battery2(void)
{
 
  char readbattbuf[2];
  int batteryval2;
  short battery2val[2];
	
  readbattbuf[1]=IIC1_read_one_byte(0x0e);
  readbattbuf[0]=IIC1_read_one_byte(0x0f);
 
  batteryval2= readbattbuf[1];
  batteryval2= (batteryval2 << 8) |readbattbuf[0];
   
  batteryval2=((batteryval2*GAIN)/1000)+ADC_offset;//????????,?2?
	Batteryval[1]=batteryval2;
	
  battery2val[1]=(char)(batteryval2 >> 8);
  battery2val[0]=(char)(batteryval2 & 0x00FF);
	
	shang[4]=battery2val[1];
	shang[5]=battery2val[0];
	shang1[4]=battery2val[1];
	shang1[5]=battery2val[0];
	
	can_buf1[4]=battery2val[1];
	can_buf1[5]=battery2val[0];

}
/****************************************
fuction: void Get_Battery3(void)
description:获取第3号单体电池电压
Parameters:battery3val[0],battery3val[1];
******************************************/
void Get_Battery3(void)
{
 
  char readbattbuf[2];
  int batteryval3 ;
  char battery3val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x10);
  readbattbuf[0]=IIC1_read_one_byte(0x11);

  batteryval3= readbattbuf[1];
  batteryval3= (batteryval3 << 8) |readbattbuf[0];
  batteryval3=((batteryval3*GAIN)/1000)+ADC_offset;//????????,?3?
  battery3val[1]=(char)(batteryval3 >> 8);
  battery3val[0]=(char)(batteryval3 >> 0);
	Batteryval[2]=batteryval3;
	
	shang[6]=battery3val[1];
	shang[7]=battery3val[0];
	
	
	shang1[6]=battery3val[1];
	shang1[7]=battery3val[0];
	
	can_buf1[6]=battery3val[1];
	can_buf1[7]=battery3val[0];
}
/****************************************
fuction: void Get_Battery4(void)
description:获取第4号单体电池电压
Parameters:battery4val[0],battery4val[1];
******************************************/
void Get_Battery4(void)
{
  char readbattbuf[2];
  int batteryval4 ;
  char battery4val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x12);
  readbattbuf[0]=IIC1_read_one_byte(0x13);
 
  batteryval4= readbattbuf[1];
  batteryval4= (batteryval4 << 8) |readbattbuf[0];
  batteryval4=((batteryval4*GAIN)/1000)+ADC_offset;//????????,?4?
  battery4val[1]=(char)(batteryval4 >> 8);
  battery4val[0]=(char)(batteryval4 >> 0);
	Batteryval[3]=batteryval4;
	
	shang[8]=battery4val[1];
	shang[9]=battery4val[0];

  shang1[8]=battery4val[1];
	shang1[9]=battery4val[0];
	
	can_buf2[2]=battery4val[1];
	can_buf2[3]=battery4val[0];

}
/****************************************
fuction: void Get_Battery5(void)
description:获取第5号单体电池电压
Parameters:battery5val[0],battery5val[1];
******************************************/
void Get_Battery5(void)
{
 char readbattbuf[2];
  short batteryval5 ;
 char battery5val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x14);
  readbattbuf[0]=IIC1_read_one_byte(0x15);
 
  batteryval5= readbattbuf[1];
  batteryval5= (batteryval5 << 8) |readbattbuf[0];
  batteryval5=((batteryval5*GAIN)/1000)+ADC_offset;//????????,?5?
	Batteryval[4]=batteryval5;
  battery5val[1]=(char)(batteryval5 >> 8);
  battery5val[0]=(char)(batteryval5 & 0x00FF);
	

	shang[10]=battery5val[1];
	shang[11]=battery5val[0];
	
	shang1[10]=battery5val[1];
	shang1[11]=battery5val[0];
	
	can_buf2[4]=battery5val[1];
	can_buf2[5]=battery5val[0];

}
/****************************************
fuction: void Get_Battery6(void)
description:获取第6号单体电池电压
Parameters:battery6val[0],battery6val[1];
******************************************/
void Get_Battery6(void)
{
 
  char readbattbuf[2];
  short batteryval6 ;
 char battery6val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x16);
  readbattbuf[0]=IIC1_read_one_byte(0x17);
 
  batteryval6= readbattbuf[1];
  batteryval6= (batteryval6 << 8) |readbattbuf[0];
  batteryval6=((batteryval6*GAIN)/1000)+ADC_offset;//????????,?6?
	Batteryval[5]=batteryval6;
  battery6val[1]=(char)(batteryval6 >> 8);
  battery6val[0]=(char)(batteryval6 & 0x00FF);
	
	shang[12]=battery6val[1];
	shang[13]=battery6val[0];

  shang1[12]=battery6val[1];
	shang1[13]=battery6val[0];
	
	can_buf2[6]=battery6val[1];
	can_buf2[7]=battery6val[0];

}
/****************************************
fuction: void Get_Battery7(void)
description:获取第7号单体电池电压
Parameters:battery7val[0],battery7val[1];
******************************************/
void Get_Battery7(void)
{
  char readbattbuf[2];
  short batteryval7 ;
  char battery7val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x18);
  readbattbuf[0]=IIC1_read_one_byte(0x19);
 
  batteryval7= readbattbuf[1];
  batteryval7= (batteryval7 << 8) |readbattbuf[0];
  batteryval7=((batteryval7*GAIN)/1000)+ADC_offset;//????????,?7?
	Batteryval[6]=batteryval7;
  battery7val[1]=(char)(batteryval7 >> 8);
  battery7val[0]=(char)(batteryval7 & 0x00FF);
	
	shang[14]=battery7val[1];
	shang[15]=battery7val[0];
	
	shang1[14]=battery7val[1];
	shang1[15]=battery7val[0];
	
	can_buf3[2]=battery7val[1];
	can_buf3[3]=battery7val[0];

}
/****************************************
fuction: void Get_Battery8(void)
description:获取第8号单体电池电压
Parameters:battery8val[0],battery8val[1];
******************************************/
void Get_Battery8(void)
{
  char readbattbuf[2];
  int batteryval8 ;
  char battery8val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x1a);
  readbattbuf[0]=IIC1_read_one_byte(0x1b);
 
  batteryval8= readbattbuf[1];
  batteryval8= (batteryval8 << 8) |readbattbuf[0];
  batteryval8=((batteryval8*GAIN)/1000)+ADC_offset;//????????,?8?
  battery8val[1]=(char)(batteryval8 >> 8);
  battery8val[0]=(char)(batteryval8 >> 0);
	Batteryval[7]=batteryval8;
	
	shang[16]=battery8val[1];
	shang[17]=battery8val[0];

  shang1[16]=battery8val[1];
	shang1[17]=battery8val[0];
	
	can_buf3[4]=battery8val[1];
	can_buf3[5]=battery8val[0];

}

/****************************************
fuction: void Get_Battery9(void)
description:获取第9号单体电池电压
Parameters:battery9val[0],battery9val[1];
******************************************/
void Get_Battery9(void)
{
  char readbattbuf[2];
  short batteryval9 ;
  char battery9val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x1c);
  readbattbuf[0]=IIC1_read_one_byte(0x1d);
 
  batteryval9= readbattbuf[1];
  batteryval9= (batteryval9 << 8) |readbattbuf[0];
  batteryval9=((batteryval9*GAIN)/1000)+ADC_offset;//????????,?8?
  battery9val[1]=(char)(batteryval9 >> 8);
  battery9val[0]=(char)(batteryval9 & 0x00FF);
	Batteryval[8]=batteryval9;
	
	shang[18]=battery9val[1];
	shang[19]=battery9val[0];

  shang1[18]=battery9val[1];
	shang1[19]=battery9val[0];
	
	can_buf3[6]=battery9val[1];
	can_buf3[7]=battery9val[0];

}
/****************************************
fuction: void Get_Battery10(void)
description:获取第10号单体电池电压
Parameters:battery10val[0],battery10val[1];
******************************************/
void Get_Battery10(void)
{
  char readbattbuf[2];
  short batteryval10 ;
  char battery10val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x1e);
  readbattbuf[0]=IIC1_read_one_byte(0x1f);
 
  batteryval10= readbattbuf[1];
  batteryval10= (batteryval10 << 8) |readbattbuf[0];
  batteryval10=((batteryval10*GAIN)/1000)+ADC_offset;//????????,?8?
	Batteryval[9]=batteryval10;
  battery10val[1]=(char)(batteryval10 >> 8);
  battery10val[0]=(char)(batteryval10 & 0x00FF);
	
	shang[20]=battery10val[1];
	shang[21]=battery10val[0];

  shang2[2]=battery10val[1];
	shang2[3]=battery10val[0];
	can_buf4[2]=battery10val[1];
	can_buf4[3]=battery10val[0];


}

/****************************************
fuction: void Get_Battery10(void)
description:获取第11号单体电池电压
Parameters:battery10val[0],battery10val[1];
******************************************/
void Get_Battery11(void)
{
  char readbattbuf[2];
  short batteryval11 ;
  char battery11val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x20);
  readbattbuf[0]=IIC1_read_one_byte(0x21);
 
  batteryval11= readbattbuf[1];
  batteryval11= (batteryval11 << 8) |readbattbuf[0];
  batteryval11=((batteryval11*GAIN)/1000)+ADC_offset;//????????,?8?
	Batteryval[10]=batteryval11;
  battery11val[1]=(char)(batteryval11 >> 8);
  battery11val[0]=(char)(batteryval11 & 0x00FF);
	
	shang[22]=battery11val[1];
	shang[23]=battery11val[0];

  shang2[4]=battery11val[1];
	shang2[5]=battery11val[0];
	
	can_buf4[4]=battery11val[1];
	can_buf4[5]=battery11val[0];

}
/****************************************
fuction: void Get_Battery10(void)
description:获取第12号单体电池电压
Parameters:battery10val[0],battery10val[1];
******************************************/
void Get_Battery12(void)
{
  char readbattbuf[2];
  short batteryval12 ;
  char battery12val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x22);
  readbattbuf[0]=IIC1_read_one_byte(0x23);
 
  batteryval12= readbattbuf[1];
  batteryval12= (batteryval12 << 8) |readbattbuf[0];
  batteryval12=((batteryval12*GAIN)/1000)+ADC_offset;//????????,?8?
	Batteryval[11]=batteryval12;
  battery12val[1]=(char)(batteryval12 >> 8);
  battery12val[0]=(char)(batteryval12 & 0x00FF);
	
	shang[24]=battery12val[1];
	shang[25]=battery12val[0];

  shang2[6]=battery12val[1];
	shang2[7]=battery12val[0];
	
	can_buf4[6]=battery12val[1];
	can_buf4[7]=battery12val[0];

}
/****************************************
fuction: void Get_Battery10(void)
description:获取第13号单体电池电压
Parameters:battery10val[0],battery10val[1];
******************************************/
void Get_Battery13(void)
{
  char readbattbuf[2];
  short batteryval13 ;
  char battery13val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x24);
  readbattbuf[0]=IIC1_read_one_byte(0x25);
 
  batteryval13= readbattbuf[1];
  batteryval13= (batteryval13 << 8) |readbattbuf[0];
  batteryval13=((batteryval13*GAIN)/1000)+ADC_offset;//????????,?8?
	Batteryval[12]=batteryval13;
  battery13val[1]=(char)(batteryval13 >> 8);
  battery13val[0]=(char)(batteryval13 & 0x00FF);
	
	shang[26]=battery13val[1];
	shang[27]=battery13val[0];

  shang2[8]=battery13val[1];
	shang2[9]=battery13val[0];
	
	can_buf5[2]=battery13val[1];
	can_buf5[3]=battery13val[0];

}
/****************************************
fuction: void Get_Battery10(void)
description:获取第14号单体电池电压
Parameters:battery10val[0],battery10val[1];
******************************************/
void Get_Battery14(void)
{
  char readbattbuf[2];
  short batteryval14 ;
  char battery14val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x26);
  readbattbuf[0]=IIC1_read_one_byte(0x27);
 
  batteryval14= readbattbuf[1];
  batteryval14= (batteryval14 << 8) |readbattbuf[0];
  batteryval14=((batteryval14*GAIN)/1000)+ADC_offset;//????????,?8?
	Batteryval[13]=batteryval14;
  battery14val[1]=(char)(batteryval14 >> 8);
  battery14val[0]=(char)(batteryval14 & 0x00FF);
	
	shang[28]=battery14val[1];
	shang[29]=battery14val[0];

  shang2[10]=battery14val[1];
	shang2[11]=battery14val[0];
	
	can_buf5[4]=battery14val[1];
	can_buf5[5]=battery14val[0];

}
/****************************************
fuction: void Get_Battery10(void)
description:获取第15号单体电池电压
Parameters:battery10val[0],battery10val[1];
******************************************/
void Get_Battery15(void)
{
  char readbattbuf[2];
  short batteryval15 ;
  char battery15val[2];
  readbattbuf[1]=IIC1_read_one_byte(0x28);
  readbattbuf[0]=IIC1_read_one_byte(0x29);
 
  batteryval15= readbattbuf[1];
  batteryval15= (batteryval15 << 8) |readbattbuf[0];
  batteryval15=((batteryval15*GAIN)/1000)+ADC_offset;//????????,?8?
	Batteryval[14]=batteryval15;
  battery15val[1]=(char)(batteryval15 >> 8);
  battery15val[0]=(char)(batteryval15 & 0x00FF);
	
	shang[30]=battery15val[1];
	shang[31]=battery15val[0];

  shang2[12]=battery15val[1];
	shang2[13]=battery15val[0];
	
	can_buf5[6]=battery15val[1];
	can_buf5[7]=battery15val[0];


}



/****************************
获取所有电池的总电压值并用串口1打印出来
 *****************************/

void Get_Update_ALL_Data(void)
{
	int i,Sum_val;
//  for(i=0;i<15;i++)
//	{
	   Sum_val= Batteryval[0]+Batteryval[1]+Batteryval[4]+Batteryval[5]+Batteryval[6]+Batteryval[9]+Batteryval[10]+Batteryval[11]+Batteryval[14];
	//}
	
	Batteryval[15] = Sum_val;
	
	shang[32]=(char)(Batteryval[15] >> 8);
  shang[33]=(char)(Batteryval[15] &0XFF);
	
  shang2[14]=(char)shang[32];
  shang2[15]=(char)shang[33];
	
	can_buf6[2]=(char)shang[32];
	can_buf6[3]=(char)shang[33];

}

int SOC;
void Get_SOC(void)
{
	
  if(	Batteryval[15] >(4100*9))
       {SOC=100;}
	else if((	Batteryval[15] >(4100*9))&&(Batteryval[15]<(4150*9))){SOC=95;}
	else if((Batteryval[15]>(4050*9))&&(Batteryval[15]<(4100*9))){SOC=90;}
	else if((Batteryval[15]>(4000*9))&&(Batteryval[15]<(4050*9))){SOC=88;}
	else if((Batteryval[15]>(3950*9))&&(Batteryval[15]<(4000*9))){SOC=87;}
	else if((Batteryval[15]>(3900*9))&&(Batteryval[15]<(3950*9))){SOC=86;}
	else if((Batteryval[15]>(3900*9))&&(Batteryval[15]<(3950*9))){SOC=85;}
	else if((Batteryval[15]>(3850*9))&&(Batteryval[15]<(3900*9))){SOC=84;}
	else if((Batteryval[15]>(3800*9))&&(Batteryval[15]<(3850*9))){SOC=83;}
	else if((Batteryval[15]>(3750*9))&&(Batteryval[15]<(3800*9))){SOC=82;}
	else if((Batteryval[15]>(3700*9))&&(Batteryval[15]<(3750*9))){SOC=81;}
	else if((Batteryval[15]>(3650*9))&&(Batteryval[15]<(3700*9))){SOC=80;}
	else if((Batteryval[15]>(3600*9))&&(Batteryval[15]<(3650*9))){SOC=79;}
	else if((Batteryval[15]>(3550*9))&&(Batteryval[15]<(3600*9))){SOC=78;}
	else if((Batteryval[15]>(3500*9))&&(Batteryval[15]<(3550*9))){SOC=77;}
	
	else if((Batteryval[15]>(3450*9))&&(Batteryval[15]<(3500*9))){SOC=40;}
	else if((Batteryval[15]>(3400*9))&&(Batteryval[15]<(3500*9))){SOC=30;}
	else if((Batteryval[15]>(3300*9))&&(Batteryval[15]<(3400*9))){SOC=20;}
	else if((Batteryval[15]>(3200*9))&&(Batteryval[15]<(3300*9))){SOC=10;}
	else if((Batteryval[15]>(3100*9))&&(Batteryval[15]<(3200*9))){SOC=5;}
	Batteryval[16] = SOC;
	
  shang[34]=(char)(Batteryval[16] >> 8);
  shang[35]=(char)(Batteryval[16] &0XFF);

	
	shang2[16]=(char)shang[34];
  shang2[17]=(char)shang[35];
	
	can_buf6[4]=(char)shang[34];
	can_buf6[5]=(char)shang[35];

}






 /****************************
获取所有电池的电压值并用串口1打印出来
 *****************************/




/////////////////////////////////////////////////////////////////////////////////////////////////////////

void OV_UV_1_PROTECT(void)
{
  unsigned char OVTrip_Val,UVTrip_Val;
  float t = 0.377;
  OVTrip_Val = (unsigned char)((((unsigned int)((OVPThreshold - ADC_offset)/t + 0.5 ) )>>4) &0xFF);
  UVTrip_Val = (unsigned char)((((unsigned int)((UVPThreshold - ADC_offset)/t + 0.5 ) )>>4) &0xFF);
  IIC1_write_one_byte_CRC(OV_TRIP,OVTrip_Val);
  IIC1_write_one_byte_CRC(UV_TRIP,UVTrip_Val);
 }

 

void OCD_SCD_PROTECT(void) 
{

IIC1_write_one_byte_CRC(PROTECT1,0xFF);//设置短路电流66A(SRN-SRP33mV)，短路延时400us，RSNS=0.
IIC1_write_one_byte_CRC(PROTECT2,0xFF);//设置OSD为100A(SRN-SRP50mV)，延时设置最长为1280ms。

}


/****************************************
fuction: void BQ_1_config(void)
description:BQ76930初始化
Parameters: None
 //0x04寄存器0x19对应SCD延时70uS，放电短路电压33mV；
 //0x05寄存器设置检测电流为1-shot模式；
 //0x06寄存器0x39对应OCD的延时时间80mS，放电过流电压33mV；短路和过流对应电流都是60A.
******************************************/
unsigned char BQ769_INITAdd[12]={SYS_STAT,CELLBAL1,CELLBAL2,CELLBAL3,SYS_CTRL1,SYS_CTRL2,PROTECT1,PROTECT2,PROTECT3,OV_TRIP,UV_TRIP,CC_CFG};
unsigned char BQ769_INITdata[12]={0xFF,     0x00,    0x00,     0x00,     0x18,    0x43,      0x00,     0x00,    0x00,    0x00,   0x00,  0x19};
void BQ_1_config(void)
{
   char i;
   for(i=0;i<12;i++)
    {
			delay_ms(10);
			IIC1_write_one_byte_CRC(BQ769_INITAdd[i],BQ769_INITdata[i]);
    }
}

/****************************************
fuction: 均衡的1、2、5、6、7、10分别表示电池组上第1到6节的均衡，
         并且需要注意相邻的电池串不能同时开启均衡。由于均衡的条件不同需要用户根据自己的需求开启。
description:
Parameters: None
******************************************/
void Battery1_Balance(void)
{
   IIC1_write_one_byte_CRC(CELLBAL1,0X01);
}

void Battery2_Balance(void)
{
   IIC1_write_one_byte_CRC(CELLBAL1,0X02);
}

void Battery5_Balance(void)
{
   IIC1_write_one_byte_CRC(CELLBAL1,0X10);
}

void Battery6_Balance(void)
{
   IIC1_write_one_byte_CRC(CELLBAL2,0X01);
}

void Battery7_Balance(void)
{
   IIC1_write_one_byte_CRC(CELLBAL2,0X02);
}

void Battery10_Balance(void)
{
   IIC1_write_one_byte_CRC(CELLBAL2,0X10);
}


/****************************************
fuction: void BQ_1_config(void)
description:BQ76930初始化
Parameters: None
******************************************/
void BQ_2_config(void)
{
   char i;
   for(i=0;i<11;i++)
    {
			delay_ms(50);
    }
}
/****************************************
fuction:void SHIP_1_BQ769(void)
description:BQ76930进入低功耗模式
Parameters: None
******************************************/
void SHIP_1_BQ769(void)
{
  IIC1_write_one_byte_CRC(SYS_CTRL1,0x19); //01
  delay_ms(20);
  IIC1_write_one_byte_CRC(SYS_CTRL1,0x1a); //10
 }

 


/****************************************
fuction:void SHIP_2_BQ769(void)
description:从BQ76930低功耗模式唤醒
Parameters: None
******************************************/
void WAKE_1_BQ769(void)
{
   MCU_WAKE_BQ_ONOFF(1);
	 delay_ms(50);
	 MCU_WAKE_BQ_ONOFF(0);
	 //NVIC_SystemReset();                       //重启
 }

 
 /****************************************
fuction: void Get_BQCurrent(void)
description:BQ76940电流测量，采样电阻按4mΩ/8计算
Parameters: None
******************************************/

 void Get_BQ_Current(void)
 {	 
   unsigned char readCurrentbuf[2];
	 unsigned int    Currentbufval;
	 float  Currentval;
   unsigned char Currentbuf[1];
	 
	 readCurrentbuf[1]=IIC1_read_one_byte(CC_HI_BYTE);
   readCurrentbuf[0]=IIC1_read_one_byte(CC_LO_BYTE);
   Currentbufval = readCurrentbuf[1];
   Currentbufval = (Currentbufval << 8)|readCurrentbuf[0];
	 if(  Currentbufval <= 0x7D00 )
	 {	 
	 Currentval = (Currentbufval*2.11); //单位mV；按4mΩ/8计算，单位mA；
   Batteryval[17]=Currentval;
	 shang[36]=Batteryval[17]>>8;
	 shang[37]=Batteryval[17]&0X00FF;
	 shang2[18]=shang[36];
	 shang2[19]=shang[37];
		 
	can_buf6[6]=(char)shang[36];
	can_buf6[7]=(char)shang[37];

	 
	 }
   else
	 {		  
		 Currentval = ((0XFFFF - Currentbufval )*2.11);
		 Batteryval[17]=Currentval;
    shang[36]=Batteryval[17]>>8;
	  shang[37]=Batteryval[17]&0X00FF;
    shang2[18]=shang[36];
	  shang2[19]=shang[37];		 
  can_buf6[6]=(char)shang[36];
	can_buf6[7]=(char)shang[37];
	 
	 }		 	 
 }
 /****************************************
fuction: void Get_BQ_1_Temp(void)
description:BQ76930 103AT温度测量
Parameters: None
******************************************/

 float  Tempval_1,Tempval_2,Tempval_3,Tempval_4;
 void Get_BQ1_1_Temp(void)
 {
	 float Rt=0;
   float Rp=10000;
   float T2=273.15+25;
   float Bx=3380;
   float Ka=273.15;
   unsigned char readTempbuf[2];
	 int  TempRes;
   unsigned char Tempbuf[1];
	 readTempbuf[1]=IIC1_read_one_byte(TS1_HI_BYTE);
   readTempbuf[0]=IIC1_read_one_byte(TS1_LO_BYTE);
   TempRes = (readTempbuf[1] << 8 ) | readTempbuf[0];
	 TempRes = (10000*(TempRes*382/1000))/(3300-(TempRes*382/1000)); 
   Tempval_1 = 1/(1/T2+(log(TempRes/Rp))/Bx)- Ka + 0.5;
	 Batteryval[18] = Tempval_1;
   	 	 
 }


 void Get_BQ1_2_Temp(void)
 {
   float Rt=0;
   float Rp=10000;
   float T2=273.15+25;
   float Bx=3380;
   float Ka=273.15;
   unsigned char readTempbuf[2];
	 int  TempRes;
   unsigned char Tempbuf[1];
	 readTempbuf[1]=IIC1_read_one_byte(TS1_HI_BYTE);
   readTempbuf[0]=IIC1_read_one_byte(TS1_LO_BYTE);
   TempRes = (readTempbuf[1] << 8 ) | readTempbuf[0];
	 TempRes = (10000*(TempRes*382/1000))/(3300-(TempRes*382/1000)); 
   Tempval_2 = 1/(1/T2+(log(TempRes/Rp))/Bx)- Ka + 0.5;
	 Batteryval[18] = Tempval_2;
   shang[38]=Batteryval[18];	 	 
	 shang3[2]=shang[38];	 
	can_buf7[2]=(char)shang[38];

 }
  
 unsigned char BMS_sta,DSG_STA,CHG_STA,DSG_STA_FLAG,CHG_STA_FLAG;	
void BMS_STA(void)
	{
   BMS_sta = IIC1_read_one_byte(SYS_CTRL2);
   DSG_STA = BMS_sta&0x02;
	 CHG_STA = BMS_sta&0x01;
	 shang[39]=DSG_STA;
	 shang[40]=CHG_STA;
		
	can_buf7[3]=(char)shang[39];
	can_buf7[4]=(char)shang[40];

	}
 
 void Only_Open_CHG(void)
	{
		BMS_STA();
		if(DSG_STA!=0)
		{
	    IIC1_write_one_byte_CRC(SYS_CTRL2,0X43);	
		}
		else {IIC1_write_one_byte_CRC(SYS_CTRL2,0X41);}
	}
 void Only_Close_CHG(void)
	{
		BMS_STA();
		if(DSG_STA!=0)
		{
	    IIC1_write_one_byte_CRC(SYS_CTRL2,0X42);	
		}
		else
		{IIC1_write_one_byte_CRC(SYS_CTRL2,0X40);	}
	}
	
	void Only_Open_DSG(void)
	{
		BMS_STA();
		if(CHG_STA!=0)
		{
	    IIC1_write_one_byte_CRC(SYS_CTRL2,0X43);	
		}
		else
		{IIC1_write_one_byte_CRC(SYS_CTRL2,0X42);}	
	}
  void Only_Close_DSG(void)
	{
		BMS_STA();
		if(CHG_STA!=0)
		{
	    IIC1_write_one_byte_CRC(SYS_CTRL2,0X41);	
		}
		else
		{IIC1_write_one_byte_CRC(SYS_CTRL2,0X40);	}
	}
	
	 void Open_DSG_CHG(void)
	{
	 IIC1_write_one_byte_CRC(SYS_CTRL2,0X43);
   		
	}
 void Close_DSG_CHG(void)
	{
	 IIC1_write_one_byte_CRC(SYS_CTRL2,0X40);	
	}
 
 
 
 
 void Update_val(void)
{
  char NEW[50]={0};
	int n;

	  for(n=0;n<50;n++)

		{
			USART_SendData(USART1, shang[n]); //发送一个字节数据
			delay_ms(2);
			
		}
		
	  USART2_Printf( shang1,20,ASCII_CODE);	
		delay_ms(100);
		USART2_Printf( shang2,20,ASCII_CODE);
		delay_ms(100);
		USART2_Printf( shang3,20,ASCII_CODE);

    Can_Send_Msg(can_buf1,8,0x0001);
		delay_ms(2);
		Can_Send_Msg(can_buf2,8,0x0002);
		delay_ms(2);
		Can_Send_Msg(can_buf3,8,0x0003);
		delay_ms(2);
		Can_Send_Msg(can_buf4,8,0x0004);
		delay_ms(2);
		Can_Send_Msg(can_buf5,8,0x0005);
		delay_ms(2);
		Can_Send_Msg(can_buf6,8,0x0006);
		delay_ms(2);
		Can_Send_Msg(can_buf7,8,0x0007);
    delay_ms(2);
		
  UartSend("CLR(61);\r\n");
  delay_ms(100);
	UartSend("CLR(61);\r\n");
  delay_ms(100);
  UartSend("DIR(1);\r\n");
		delay_ms(100);
	sprintf(NEW,"DCV16(0,0,'%s%d%s',3);\r\n","第一节电压:",Batteryval[0],"mV"); 
  UartSend(NEW); 
  
	delay_ms(100); 

	sprintf(NEW,"DCV16(0,20,'%s%d%s',3);\r\n","第二节电压:",Batteryval[1],"mV"); 
  UartSend(NEW); 
		
  delay_ms(100);
  
	
	sprintf(NEW,"DCV16(0,40,'%s%d%s',3);\r\n","第三节电压:",Batteryval[4],"mV"); 
  UartSend(NEW); 
  
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,60,'%s%d%s',3);\r\n","第四节电压:",Batteryval[5],"mV"); 
  UartSend(NEW); 
  
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,80,'%s%d%s',3);\r\n","第五节电压:",Batteryval[6],"mV"); 
  UartSend(NEW); 
  
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,100,'%s%d%s',3);\r\n","第六节电压:",Batteryval[9],"mV"); 
  UartSend(NEW); 
		delay_ms(1000);
	UartSend("CLR(61);\r\n");

  delay_ms(100); 
	sprintf(NEW,"DCV16(0,0,'%s%d%s',3);\r\n","第七节电压:",Batteryval[10],"mV"); 
  UartSend(NEW);
	
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,20,'%s%d%s',3);\r\n","第八节电压:",Batteryval[11],"mV"); 
  UartSend(NEW);
	
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,40,'%s%d%s',3);\r\n","第九节电压:",Batteryval[14],"mV"); 
  UartSend(NEW);
	
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,60,'%s%d%s',3);\r\n","总电压:",Batteryval[15],"mV");	
  UartSend(NEW); 
  
  delay_ms(100); 
	sprintf(NEW,"DCV16(0,80,'%s%d%s',3);\r\n","电池SOC为:",Batteryval[16],"%"); 
  UartSend(NEW); 
  
	delay_ms(100); 
	sprintf(NEW,"DCV16(0,100,'%s%.2f%s',3);\r\n","电池温度为:",Tempval_2,"℃"); 
  UartSend(NEW); 
		delay_ms(1000);
	UartSend("CLR(61);\r\n");

	delay_ms(100); 
	sprintf(NEW,"DCV16(0,00,'%s%d%s',3);\r\n","当前电流为:",Batteryval[17],"mA"); 
  UartSend(NEW); 
  
	delay_ms(100);	
	sprintf(NEW,"DCV16(0,20,'%s',3);\r\n","HWLLO WORLD ！"); 
  UartSend(NEW);
	
	delay_ms(1000);
	
	
	

}







 void Get_Update_Data(void)
{	  
    Get_Battery1();
    Get_Battery2();
    //Get_Battery3();
	 // Get_Battery4();
	  Get_Battery5();
	  Get_Battery6();
	  Get_Battery7();
	 // Get_Battery8();
	  //Get_Battery9();
	  Get_Battery10();
	  Get_Battery11();
    Get_Battery12();
    //Get_Battery13();
   // Get_Battery14();
    Get_Battery15(); 	
		Get_Update_ALL_Data();//总电压
	  Get_SOC();
		Get_BQ1_2_Temp();
    Get_BQ_Current();	
    BMS_STA();	
		Update_val();			
}

 
 
 
 
/****************************************
fuction:void readbqstate(void)
description:读取报警信号值
Parameters: UV_Alarm_flagOV_Alarm_flag
            SCD_Alarm_flag,OCD_Alarm_flag
******************************************/

 int UV_Alarm_flag,OV_Alarm_flag,SCD_Alarm_flag,OCD_Alarm_flag;
void ALERT_1_Recognition(void)
{
	
  unsigned char sys_stat_1,sys_stat_2,UV_1,OV_1,UV_2,OV_2,SCD,OCD;
  sys_stat_1 = IIC1_read_one_byte(SYS_STAT);//获取状态
  UV_1  =  sys_stat_1&0x08;  //取出UV位
  OV_1  =  sys_stat_1&0x04;
  SCD   =  sys_stat_1&0x02;
  OCD   =  sys_stat_1&0x01;
  if((UV_1 == 0x08)||(UV_2 == 0x08))
  {
      UV_Alarm_flag = 1;    //欠压报警
		  printf("pack UV\n");
  }	
   else
     UV_Alarm_flag = 0;     //没有欠压
     if((OV_1 == 0x04)||(OV_2 == 0x04))
      {
       OV_Alarm_flag = 1;			 
		   printf("pack OV\n");
      }
        else
           OV_Alarm_flag = 0;
           if(SCD == 0x02)
						{
							SCD_Alarm_flag = 1;
							printf("pack SCD\n");		
						}
							else
									SCD_Alarm_flag = 0;
 
									if(OCD == 0x01)
										{
											OCD_Alarm_flag = 1;
											printf("pack OCD\n");		
										}   
											else
													OCD_Alarm_flag = 0;	
}



unsigned char CRC8(unsigned char *ptr, unsigned char len,unsigned char key)
{
	unsigned char i;
	unsigned char crc=0;
	while(len--!=0)
	{
		for(i=0x80; i!=0; i/=2)
		{
			if((crc & 0x80) != 0)
			{
				crc *= 2;
				crc ^= key;
			}
			else
				crc *= 2;

			if((*ptr & i)!=0)
				crc ^= key;
		}
		ptr++;
	}
	return(crc);
}

