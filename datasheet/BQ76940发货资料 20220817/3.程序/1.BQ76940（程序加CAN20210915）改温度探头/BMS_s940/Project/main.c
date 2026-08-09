/******************** CAN Test Program ********************************
 * File     : main.c
 * Function : CAN communication test - stripped down from BMS project
 *            Sends test CAN frames to host computer (上位机) and
 *            listens for incoming CAN messages
 * Version  : CAN_Test_V1.0
 * Author   : (based on original BMS project)
 * Date     : 2026-08-08
 *
 * CAN Config:
 *   - Baud rate: 500kbps (36MHz / ((9+8+1)*4))
 *   - Extended ID (29-bit)
 *   - Normal mode
 *   - PA11 = CAN_RX, PA12 = CAN_TX
 *
 * Protocol (matching original):
 *   - 7 frames with IDs 0x0001 ~ 0x0007
 *   - Each frame: 8 bytes
 *   - Frame format: [0xAA, SeqNum, CounterH, CounterL, 0x55, 0xAA, ID_L, ID_H]
 *
 * LED Indicators:
 *   - LED1 (PA15): Toggles on each CAN send burst (~1Hz)
 *   - LED2 (PB13): Toggles when CAN message is received
 *   - LED3 (PB14): ON when CAN init OK
 *   - LED4 (PB15): Error indicator (CAN send failure)
**********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "led.h"
#include "systick.h"
#include "can.h"

/* Private variables ---------------------------------------------------------*/
static u32 g_send_count = 0;    /* CAN send burst counter */
static u32 g_recv_count = 0;    /* CAN receive counter */
static u32 g_send_fail = 0;     /* CAN send failure counter */

/**
  * @brief  Main program - CAN communication test
  * @param  None
  * @retval int
  */
int main(void)
{
    u8  can_rx_buf[8];
    u8  can_tx_buf[8];
    u8  msg_idx;
    u8  rx_len;
    u32 tick_counter = 0;

    /* ---- System Initialization ---- */
    SYSTICK_Init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_ms(500);

    /* ---- LED Initialization ---- */
    LED_GPIO_Config();
    LED4_ONOFF(1);   /* LED4 ON during init */

    /* ---- CAN Initialization ----
       Parameters: SJW=1tq, BS2=8tq, BS1=9tq, BRP=4, Mode=Normal
       Baud Rate = Fpclk1 / ((BS1+BS2+1) * BRP)
                = 36MHz / ((9+8+1) * 4)
                = 500kbps
    */
    if (CAN_Mode_Init(CAN_SJW_1tq, CAN_BS2_8tq, CAN_BS1_9tq, 4, CAN_Mode_Normal) == 0)
    {
        /* CAN init OK - LED3 ON, LED4 OFF */
        LED3_ONOFF(1);
        LED4_ONOFF(0);
    }
    else
    {
        /* CAN init FAIL - LED3 OFF, LED4 stays ON */
        LED3_ONOFF(0);
        while (1)
        {
            /* Blink LED4 fast to indicate init error */
            LEDXToggle(4);
            delay_ms(100);
        }
    }

    /* ---- Startup indication: blink LED1 3 times ---- */
    for (int i = 0; i < 3; i++)
    {
        LEDXToggle(1);
        delay_ms(200);
    }

    /* ======================== Main Loop ======================== */
    while (1)
    {
        /* ----- Check for received CAN messages ----- */
        rx_len = Can_Receive_Msg(can_rx_buf);
        if (rx_len > 0)
        {
            g_recv_count++;
            LEDXToggle(2);   /* Toggle LED2 on each received CAN frame */

            /* Echo received data back on CAN with ID 0x0100 offset */
            /* This allows the host to verify bidirectional communication */
            Can_Send_Msg(can_rx_buf, rx_len, 0x0100 + (can_rx_buf[1] & 0x07));
            delay_ms(1);
        }

        /* ----- Send 7 CAN test frames every ~500ms ----- */
        tick_counter++;
        if (tick_counter >= 50)   /* 50 * 10ms = 500ms */
        {
            tick_counter = 0;

            for (msg_idx = 0; msg_idx < 7; msg_idx++)
            {
                /* Build CAN test frame */
                can_tx_buf[0] = 0xAA;                           /* Header: Start byte */
                can_tx_buf[1] = 0x01 + msg_idx;                 /* Sequence: 0x01~0x07 */
                can_tx_buf[2] = (u8)(g_send_count >> 8);        /* Counter High byte */
                can_tx_buf[3] = (u8)(g_send_count & 0x00FF);    /* Counter Low byte */
                can_tx_buf[4] = 0x55;                           /* Test pattern */
                can_tx_buf[5] = 0xAA;                           /* Test pattern */
                can_tx_buf[6] = (u8)(g_recv_count >> 8);        /* RX count High */
                can_tx_buf[7] = (u8)(g_recv_count & 0x00FF);    /* RX count Low */

                /* Send via CAN (Extended ID: 0x0001 ~ 0x0007) */
                if (Can_Send_Msg(can_tx_buf, 8, 0x0001 + msg_idx) != 0)
                {
                    g_send_fail++;
                }
                delay_ms(2);
            }

            g_send_count++;
            LEDXToggle(1);   /* Toggle LED1 on each send burst */

            /* If too many send failures, indicate error on LED4 */
            if (g_send_fail > 10)
            {
                LEDXToggle(4);
                g_send_fail = 0;
            }
        }

        delay_ms(10);   /* Loop period: 10ms */
    }
}

/*******************************************************************************
      END FILE
*******************************************************************************/
