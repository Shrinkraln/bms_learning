#ifndef __BQ76930_H
#define __BQ76930_H

#include "stm32f10x.h"

#define SYS_STAT 0x00
#define CELLBAL1 0x01
#define CELLBAL2 0x02
#define CELLBAL3 0x03
#define SYS_CTRL1 0x04
#define SYS_CTRL2 0x05
#define PROTECT1 0x06
#define PROTECT2 0x07
#define PROTECT3 0x08
#define OV_TRIP 0x09
#define UV_TRIP 0x0A
#define CC_CFG  0x0B

#define OVPThreshold     4400  // 3150-4700
#define OV_Re_Threshold  4300

#define UVPThreshold  2400 //  1580-3100
#define UV_Re_Threshold  2500

#define OTPThreshold  60 //  
#define UTPThreshold  -20 //
#define TempPThreshold -50

#define VC1_HI_BYTE 0x0C
#define VC1_LO_BYTE 0x0D

#define VC2_HI_BYTE 0x0E
#define VC2_LO_BYTE 0x0F

#define VC3_HI_BYTE 0x10
#define VC3_LO_BYTE 0x11

#define VC4_HI_BYTE 0x12
#define VC4_LO_BYTE 0x13

#define VC5_HI_BYTE 0x14
#define VC5_LO_BYTE 0x15

#define VC6_HI_BYTE 0x16
#define VC6_LO_BYTE 0x17

#define VC7_HI_BYTE 0x18
#define VC7_LO_BYTE 0x19

#define VC8_HI_BYTE 0x1A
#define VC8_LO_BYTE 0x1B

#define VC9_HI_BYTE 0x1C
#define VC9_LO_BYTE 0x1D

#define VC10_HI_BYTE 0x1E
#define VC10_LO_BYTE 0x1F

#define BAT_HI_BYTE 0x2A
#define BAT_LO_BYTE 0x2B

#define TS1_HI_BYTE 0x2C
#define TS1_LO_BYTE 0x2D

#define TS2_HI_BYTE 0x2E
#define TS2_LO_BYTE 0x2F

#define CC_HI_BYTE 0x32
#define CC_LO_BYTE 0x33

#define ADCGAIN1 0x50
#define ADCOFFSET 0x51
#define ADCGAIN2 0x59

#define SCD_DELAY_50us		0x0
#define SCD_DELAY_100us		0x1
#define SCD_DEALY_200us		0x2
#define SCD_DELAY_400us		0x3

#define SCD_THRESH_44mV_22mV	0
#define SCD_THRESH_67mV_33mV	1
#define SCD_THRESH_89mV_44mV	2
#define SCD_THRESH_111mV_56mV	3
#define SCD_THRESH_133mV_67mV	4
#define SCD_TRHESH_155mV_68mV	5
#define SCD_THRESH_178mV_89mV	6
#define SCD_THRESH_200mV_100mV	7

#define OCD_DEALY_10ms		0x0
#define OCD_DELAY_20ms		0x1
#define OCD_DELAY_40ms		0x2
#define OCD_DELAY_80ms		0x3
#define OCD_DELAY_160ms		0x4
#define OCD_DELAY_320ms		0x5
#define OCD_DELAY_640ms		0x6
#define OCD_DELAY_1280ms	0x7

#define OCD_THRESH_17mV_8mV		0
#define OCD_THRESH_22mV_11mV	1
#define OCD_THRESH_28mV_14mV	2
#define OCD_THRESH_33mV_17mV	3
#define OCD_THRESH_39mV_19mV	4
#define OCD_THRESH_44mV_22mV	5
#define OCD_THRESH_50mV_25mV	6
#define OCD_THRESH_56mV_28MV	7
#define OCD_THRESH_61mV_31mV	8
#define OCD_THRESH_67mV_33mV	9
#define OCD_THRESH_72mV_36mV	0xA
#define OCD_THRESH_78mV_39mV	0xB
#define OCD_THRESH_83mV_42mV	0xC
#define OCD_THRESH_89mV_44mV	0xD
#define OCD_THRESH_94mV_47mV	0xE
#define OCD_THRESH_100mV_50mV	0xF

#define UV_DELAY_1s				0
#define UV_DELAY_4s				1
#define UV_DELAY_8s				2
#define UV_DELAY_16s			3

#define OV_DELAY_1s				0
#define OV_DELAY_2s				1
#define OV_DELAY_4s				2
#define OV_DELAY_8s				3

#define OV_THRESH_BASE			0x2008
#define UV_THRESH_BASE			0x1000
#define OV_STEP					0x10
#define UV_STEP					0x10

#define ADCGAIN_BASE			365

#define LOW_BYTE(Data)			(unsigned char)(0xff & Data)
#define HIGH_BYTE(Data)			(unsigned char)(0xff & (Data >> 8))

void Get_offset(void);
void Get_Battery1(void);
void Get_Battery2(void);
void Get_Battery3(void);
void Get_Battery4(void);
void Get_Battery5(void);
void Get_Battery6(void);
void Get_Battery7(void);
void Get_Battery8(void);
void Get_Battery9(void);
void Get_Battery10(void);
void Get_Battery11(void);
void Get_Battery12(void);
void Get_Battery13(void);
void Get_Battery14(void);
void Get_Battery15(void);
void Get_Battery16(void);
void Get_Battery17(void);
void Get_Battery18(void);
void Get_Battery19(void);
void Get_Battery20(void);
void Get_Update_Data(void);
void OV_UV_1_PROTECT(void);
void BQ_1_config(void);
void SHIP_1_BQ769(void);
void ALERT_1_Recognition(void );
void Temp_ALERT(void);
void Get_BQ_Current(void);
void Get_BQ1_1_Temp(void);
void Get_BQ1_2_Temp(void);
void Get_BQ2_1_Temp(void);
void Get_BQ2_2_Temp(void);
void BMS_STA(void);
unsigned char CRC8(unsigned char *ptr, unsigned char len,unsigned char key);


void OV_UV_2_PROTECT(void);
void BQ_2_config(void);
void SHIP_2_BQ769(void);
void ALERT_2_Recognition(void );
void SHIP_2_BQ769(void);
void BQ76930_config(void);
void Get_Update_ALL_BAT(void);
void BQ_1_2_config(void);
void OV_UV_1_2_PROTECT(void);
void WAKE_ALL_DEVICE(void);
void Only_Open_CHG(void);
void Only_Close_CHG(void);
void Only_Open_DSG(void);
void Only_Close_DSG(void);
void Open_DSG_CHG(void);
void Close_DSG_CHG(void);
void OCD_SCD_PROTECT(void);
void SHIP_ALL_DEVICE(void);
void Led_Alarm(void);
void Get_SOC(void);
#endif
