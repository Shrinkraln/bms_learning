#ifndef __USART_H
#define __USART_H
#include "stdio.h"

extern unsigned char USART_RX_BUF[64];     //接收缓冲,最大63个字节.末字节为换行符 
extern unsigned char USART_RX_STA;         //接收状态标记	
#define LENGTH  ( 0x0400 )
#define ASCII_CODE  ( 0x00 )
#define DECIMALISM  ( 0x01 )
#define HEXADECIMAL ( 0x02 )
#define DUMMY_BYTES ( 0x0000 )
#define USART1_STOP_TRUE  ( 0x01 )
#define USART1_STOP_FALSE ( 0x00 )

void uart_init(int bound);
void UartSend(char * databuf) ;
void CheckBusy(void)  ;
#endif
