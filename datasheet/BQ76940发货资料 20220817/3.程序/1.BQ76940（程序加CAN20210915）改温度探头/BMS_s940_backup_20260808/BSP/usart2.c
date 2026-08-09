#include "stm32f10x_usart.h"
#include "usart2.h"





volatile unsigned char ucUSART1_StopReceive = USART1_STOP_FALSE;


void USART1_Config( void )
{
  // USART Init Structure definition
  USART_InitTypeDef USART_InitStructure;
  // Set word length 8 bits
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  // Set stop bit 1bit
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  // Disable parity
  USART_InitStructure.USART_Parity = USART_Parity_No;  //??????
  // Set mode transmit
  USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;//??????
  // Disable hardware flow control
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  // USART1 ---------------------------------------------------------------------------------------

 
  USART_Cmd( USART1, ENABLE );
  // Enable USART1 clock
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1, ENABLE );//APB2??
  // Set baud rate ?.
  USART_InitStructure.USART_BaudRate = 9600;//?FLASH????,?????
  // Initialize USART1
  USART_Init( USART1, &USART_InitStructure);      //???????????
  // Enable USART1 recieve not empty interruption
  USART_ITConfig( USART1, USART_IT_RXNE, ENABLE );//????1??
  // Enable USART1
  USART_Cmd( USART1, ENABLE );                    //????1
  // Waiting TX empty
  while( !( USART1->SR & USART_FLAG_TXE ) );      //??????
  // print \r
  USART1->DR = ( '\r' & ( unsigned short )0x01FF );//??“\r”
  // Waiting TX empty
  while( !( USART1->SR & USART_FLAG_TXE ) );
  // print \n
  USART1->DR = ( '\n' & ( unsigned short )0x01FF );//??“\n”,?????
 }
 
unsigned char USART1_Printf( unsigned char* const pucText, unsigned int uiLength, unsigned char ucUnit )
{
  unsigned int i;
  
  // assert text pointer
  if( pucText == ( unsigned char* )0u || uiLength == 0u )             
  {
    return 0;
  } // end if
  if( ucUnit == DECIMALISM )
  {
    for( i = 0u; i < uiLength; i++ )
    {
      if( *( pucText + i ) > 99u )
      {
        // Waiting TX empty
        while( !( USART1->SR & USART_FLAG_TXE ) );
        // Transmit Data
        USART1->DR = ( ( *( pucText + i ) / 100u + '0' ) & ( unsigned short )0x01FF );
      } // end if
      if( *( pucText + i ) > 9u )
      {
        // Waiting TX empty
        while( !( USART1->SR & USART_FLAG_TXE ) );
        // Transmit Data
        USART1->DR = ( ( *( pucText + i ) % 100u / 10u + '0' ) & ( unsigned short )0x01FF );
      } // end if
      // Waiting TX empty
      while( !( USART1->SR & USART_FLAG_TXE ) );
      // Transmit Data
      USART1->DR = ( ( *( pucText + i ) % 10u + '0' ) & ( unsigned short )0x01FF );
      // Waiting TX empty
      while( !( USART1->SR & USART_FLAG_TXE ) );
      USART1->DR = ( ' ' & ( unsigned short )0x01FF );
    } // end for
  }
  else if( ucUnit == HEXADECIMAL )
  {
    for( i = 0u; i < uiLength; i++ )
    {
      // Waiting TX empty
      while( !( USART1->SR & USART_FLAG_TXE ) );
      if( ( *( pucText + i ) >> 4u ) > 9u )
      {			
        // Transmit Data
        USART1->DR = ( ( ( *( pucText + i ) >> 4u ) - 10u + 'A' ) & ( unsigned short )0x01FF );
      }
      else
      {
        USART1->DR = ( ( ( *( pucText + i ) >> 4u ) + '0' ) & ( unsigned short )0x01FF );
      } // end if...else
      // Waiting TX empty
      while( !( USART1->SR & USART_FLAG_TXE ) );
      if( ( *( pucText + i ) & 0x0F ) > 9u )
      {			
        // Transmit Data
        USART1->DR = ( ( ( *( pucText + i ) & 0x0F ) - 10u + 'A' ) & ( unsigned short )0x01FF );
      }
      else
      {
        USART1->DR = ( ( ( *( pucText + i ) & 0x0F ) + '0' ) & ( unsigned short )0x01FF );
      } // end if...else
      // Waiting TX empty
      while( !( USART1->SR & USART_FLAG_TXE ) );
      USART1->DR = ( ' ' & ( unsigned short )0x01FF );
    } // end for
  }
  else
  {
    for( i = 0u; i < uiLength; i++ )
    {
      // Waiting TX empty
      while( !( USART1->SR & USART_FLAG_TXE ) );
      // Transmit Data
      USART1->DR = ( *( pucText + i ) & ( unsigned short )0x01FF );
    } // end for
  } 
	return 1;
}


void Set_USART1_StopFlag( unsigned char ucFlag1 )
{
  ucUSART1_StopReceive = ucFlag1;
} // end function Set_USART_StopFlag


/**************************************************************************************************
^_^ FunctionName    : Get_USART_StopFlag
^_^ Brief           : Getting USART stop receive flag.
^_^ Parameters      : none.
^_^ Return          : reurn ucUSART_StopReceive variable.
^_^ Hardware        : none.
^_^ Note            : Anyone must not use this function without permission. On the contrary, the 
                      user should read comments carefully before using this function.
**************************************************************************************************/
unsigned char Get_USART1_StopFlag( void )
{
  return ucUSART1_StopReceive;
}













volatile unsigned char ucUSART2_StopReceive = USART2_STOP_FALSE;
void USART2_Config( void )
{
	
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB2PeriphClockCmd(RCC_APB1Periph_USART2|RCC_APB2Periph_GPIOA|RCC_APB2Periph_AFIO, ENABLE);
     //USART1_TX   PA.9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
   
    //USART1_RX	  PA.10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);  

   //Usart1 NVIC ÅäÖÃ

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2 ;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		//

	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQÍ¨µÀÊ¹ÄÜ
	NVIC_Init(&NVIC_InitStructure);	//¸ù¾ÝNVIC_InitStructÖÐÖ¸¶¨µÄ²ÎÊý³õÊ¼»¯ÍâÉèNVIC¼Ä´æÆ÷USART1
	
	
  
  // Set word length 8 bits
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  // Set stop bit 1bit
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  // Disable parity
  USART_InitStructure.USART_Parity = USART_Parity_No;  //??????
  // Set mode transmit
  USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;//??????
  // Disable hardware flow control
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  // USART1 ---------------------------------------------------------------------------------------

 
  USART_Cmd( USART2, ENABLE );
  // Enable USART1 clock
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE );//APB2??
  // Set baud rate ?.
  USART_InitStructure.USART_BaudRate = 9600;//?FLASH????,?????
  // Initialize USART1
  USART_Init( USART2, &USART_InitStructure);      //???????????
  // Enable USART1 recieve not empty interruption
  USART_ITConfig( USART2, USART_IT_RXNE, ENABLE );//????1??
  // Enable USART1
  USART_Cmd( USART2, ENABLE );                    //????1
  // Waiting TX empty
  while( !( USART2->SR & USART_FLAG_TXE ) );      //??????
  // print \r
  USART2->DR = ( '\r' & ( unsigned short )0x01FF );//??“\r?
  // Waiting TX empty
  while( !( USART2->SR & USART_FLAG_TXE ) );
  // print \n
  USART2->DR = ( '\n' & ( unsigned short )0x01FF );//??“\n??????
 }
 

unsigned char USART2_Printf( unsigned char* const pucText, unsigned int uiLength, unsigned char ucUnit )
{
  unsigned int i;
  
  // assert text pointer
  if( pucText == ( unsigned char* )0u || uiLength == 0u )             
  {
    return 0;
  } // end if
  if( ucUnit == DECIMALISM )
  {
    for( i = 0u; i < uiLength; i++ )
    {
      if( *( pucText + i ) > 99u )
      {
        // Waiting TX empty
        while( !( USART2->SR & USART_FLAG_TXE ) );
        // Transmit Data
        USART2->DR = ( ( *( pucText + i ) / 100u + '0' ) & ( unsigned short )0x01FF );
      } // end if
      if( *( pucText + i ) > 9u )
      {
        // Waiting TX empty
        while( !( USART2->SR & USART_FLAG_TXE ) );
        // Transmit Data
        USART2->DR = ( ( *( pucText + i ) % 100u / 10u + '0' ) & ( unsigned short )0x01FF );
      } // end if
      // Waiting TX empty
      while( !( USART2->SR & USART_FLAG_TXE ) );
      // Transmit Data
      USART2->DR = ( ( *( pucText + i ) % 10u + '0' ) & ( unsigned short )0x01FF );
      // Waiting TX empty
      while( !( USART2->SR & USART_FLAG_TXE ) );
      USART2->DR = ( ' ' & ( unsigned short )0x01FF );
    } // end for
  }
  else if( ucUnit == HEXADECIMAL )
  {
    for( i = 0u; i < uiLength; i++ )
    {
      // Waiting TX empty
      while( !( USART2->SR & USART_FLAG_TXE ) );
      if( ( *( pucText + i ) >> 4u ) > 9u )
      {			
        // Transmit Data
        USART2->DR = ( ( ( *( pucText + i ) >> 4u ) - 10u + 'A' ) & ( unsigned short )0x01FF );
      }
      else
      {
        USART2->DR = ( ( ( *( pucText + i ) >> 4u ) + '0' ) & ( unsigned short )0x01FF );
      } // end if...else
      // Waiting TX empty
      while( !( USART2->SR & USART_FLAG_TXE ) );
      if( ( *( pucText + i ) & 0x0F ) > 9u )
      {			
        // Transmit Data
        USART2->DR = ( ( ( *( pucText + i ) & 0x0F ) - 10u + 'A' ) & ( unsigned short )0x01FF );
      }
      else
      {
        USART2->DR = ( ( ( *( pucText + i ) & 0x0F ) + '0' ) & ( unsigned short )0x01FF );
      } // end if...else
      // Waiting TX empty
      while( !( USART2->SR & USART_FLAG_TXE ) );
      USART2->DR = ( ' ' & ( unsigned short )0x01FF );
    } // end for
  }
  else
  {
    for( i = 0u; i < uiLength; i++ )
    {
      // Waiting TX empty
      while( !( USART2->SR & USART_FLAG_TXE ) );
      // Transmit Data
      USART2->DR = ( *( pucText + i ) & ( unsigned short )0x01FF );
    } // end for
  } 
	return 1;
}


void Set_USART2_StopFlag( unsigned char ucFlag2 )
{
  ucUSART2_StopReceive = ucFlag2;
} // end function Set_USART_StopFlag


/**************************************************************************************************
^_^ FunctionName    : Get_USART_StopFlag
^_^ Brief           : Getting USART stop receive flag.
^_^ Parameters      : none.
^_^ Return          : reurn ucUSART_StopReceive variable.
^_^ Hardware        : none.
^_^ Note            : Anyone must not use this function without permission. On the contrary, the 
                      user should read comments carefully before using this function.
**************************************************************************************************/
unsigned char Get_USART2_StopFlag( void )
{
  return ucUSART2_StopReceive;
}
























/*´®¿Ú3ÅäÖÃ */

volatile unsigned char ucUSART3_StopReceive = USART3_STOP_FALSE;

void USART3_Config( void )
{	
	// USART Init Structure definition
  USART_InitTypeDef USART_InitStructure;
	
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART3, ENABLE );//APB2??
	
  // Set word length 8 bits
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  // Set stop bit 1bit
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  // Disable parity
  USART_InitStructure.USART_Parity = USART_Parity_No;  //??????
  // Set mode transmit
  USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;//??????
  // Disable hardware flow control
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  // USART2 ---------------------------------------------------------------------------------------

  // Set baud rate ?.
  USART_InitStructure.USART_BaudRate = 9600;//?FLASH????,?????
  // Initialize USART1
  USART_Init( USART3, &USART_InitStructure);      //???????????
	
  // Enable USART1 recieve not empty interruption
  USART_ITConfig( USART3, USART_IT_RXNE, ENABLE );//????1??
  // Enable USART1
  USART_Cmd( USART3, ENABLE );                    //????1
  // Waiting TX empty
  while( !( USART3->SR & USART_FLAG_TXE ) );      //??????
  // print \r
  USART3->DR = ( '\r' & ( unsigned short )0x01FF );//??“\r?
  // Waiting TX empty
  while( !( USART3->SR & USART_FLAG_TXE ) );
  // print \n
  USART3->DR = ( '\n' & ( unsigned short )0x01FF );//??“\n??????
 }
 
unsigned char USART3_Printf( unsigned char* const pucText, unsigned int uiLength, unsigned char ucUnit )
{
  unsigned int i;
  
  // assert text pointer
  if( pucText == ( unsigned char* )0u || uiLength == 0u )             
  {
    return 0;
  } // end if
  if( ucUnit == DECIMALISM )
  {
    for( i = 0u; i < uiLength; i++ )
    {
      if( *( pucText + i ) > 99u )
      {
        // Waiting TX empty
        while( !( USART3->SR & USART_FLAG_TXE ) );
        // Transmit Data
        USART3->DR = ( ( *( pucText + i ) / 100u + '0' ) & ( unsigned short )0x01FF );
      } // end if
      if( *( pucText + i ) > 9u )
      {
        // Waiting TX empty
        while( !( USART3->SR & USART_FLAG_TXE ) );
        // Transmit Data
        USART3->DR = ( ( *( pucText + i ) % 100u / 10u + '0' ) & ( unsigned short )0x01FF );
      } // end if
      // Waiting TX empty
      while( !( USART3->SR & USART_FLAG_TXE ) );
      // Transmit Data
      USART3->DR = ( ( *( pucText + i ) % 10u + '0' ) & ( unsigned short )0x01FF );
      // Waiting TX empty
      while( !( USART3->SR & USART_FLAG_TXE ) );
      USART3->DR = ( ' ' & ( unsigned short )0x01FF );
    } // end for
  }
  else if( ucUnit == HEXADECIMAL )
  {
    for( i = 0u; i < uiLength; i++ )
    {
      // Waiting TX empty
      while( !( USART3->SR & USART_FLAG_TXE ) );
      if( ( *( pucText + i ) >> 4u ) > 9u )
      {			
        // Transmit Data
        USART3->DR = ( ( ( *( pucText + i ) >> 4u ) - 10u + 'A' ) & ( unsigned short )0x01FF );
      }
      else
      {
        USART3->DR = ( ( ( *( pucText + i ) >> 4u ) + '0' ) & ( unsigned short )0x01FF );
      } // end if...else
      // Waiting TX empty
      while( !( USART3->SR & USART_FLAG_TXE ) );
      if( ( *( pucText + i ) & 0x0F ) > 9u )
      {			
        // Transmit Data
        USART3->DR = ( ( ( *( pucText + i ) & 0x0F ) - 10u + 'A' ) & ( unsigned short )0x01FF );
      }
      else
      {
        USART3->DR = ( ( ( *( pucText + i ) & 0x0F ) + '0' ) & ( unsigned short )0x01FF );
      } // end if...else
      // Waiting TX empty
      while( !( USART3->SR & USART_FLAG_TXE ) );
      USART3->DR = ( ' ' & ( unsigned short )0x01FF );
    } // end for
  }
  else
  {
    for( i = 0u; i < uiLength; i++ )
    {
      // Waiting TX empty
      while( !( USART3->SR & USART_FLAG_TXE ) );
      // Transmit Data
      USART3->DR = ( *( pucText + i ) & ( unsigned short )0x01FF );
    } // end for
  } 
	return 1;
}


void Set_USART3_StopFlag( unsigned char ucFlag3 )
{
  ucUSART3_StopReceive = ucFlag3;
} // end function Set_USART_StopFlag

unsigned char Get_USART3_StopFlag( void )
{
  return ucUSART3_StopReceive;
}

