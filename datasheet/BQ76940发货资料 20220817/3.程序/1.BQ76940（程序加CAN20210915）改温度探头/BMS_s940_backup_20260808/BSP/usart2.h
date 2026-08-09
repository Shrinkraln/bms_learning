/**************************************************************************************************
^_^ FileName : USART.h
^_^ Brief    : This file is USART header of current project.
^_^ Author   : Liang Zhen.
^_^ Version  : V1.0.0
^_^ Date     : 01-June-2016.
^_^ Note     : All copyright reserved. Anyone must not use any programs in this file without  
               permission. On the contrary, the user should read comments carefully before using 
               any codes in the file.
**************************************************************************************************/
#ifndef USART_H
#define USART_H

// include header file ----------------------------------------------------------------------------
#include <stdio.h>

// public macro definition ------------------------------------------------------------------------
#define ACTIVATE_USART1
#define ACTIVATE_USART3

// Print text unit
#define ASCII_CODE  ( 0x00 )
#define DECIMALISM  ( 0x01 )
#define HEXADECIMAL ( 0x02 )
#define DUMMY_BYTES ( 0x0004 )
#define USART1_DATA_TAG  ( 0x53 )
#define USART2_DATA_TAG  ( 0x53 )
#define USART3_DATA_TAG  ( 0x53 )
#define LENGTH  ( 0x0400 )
// USART stop receive macro
#define USART1_STOP_TRUE  ( 0x01 )
#define USART1_STOP_FALSE ( 0x00 )
#define USART2_STOP_TRUE  ( 0x01 )
#define USART2_STOP_FALSE ( 0x00 )
#define USART3_STOP_TRUE  ( 0x01 )
#define USART3_STOP_FALSE ( 0x00 )


// type definition --------------------------------------------------------------------------------


// public function declaration --------------------------------------------------------------------
void USART1_Config( void );

unsigned char USART1_Printf( unsigned char* const pucText, unsigned int uiLength, unsigned char ucUnit );

void Set_USART1_StopFlag( unsigned char ucFlag1 );

unsigned char Get_USART1_StopFlag( void );

void USART2_Config( void );

unsigned char USART2_Printf( unsigned char* const pucText, unsigned int uiLength, unsigned char ucUnit );

void Set_USART2_StopFlag( unsigned char ucFlag2 );

unsigned char Get_USART2_StopFlag( void );



void USART3_Config( void );

unsigned char USART3_Printf( unsigned char* const pucText, unsigned int uiLength, unsigned char ucUnit );

void Set_USART3_StopFlag( unsigned char ucFlag2 );

unsigned char Get_USART3_StopFlag( void );



#endif 

