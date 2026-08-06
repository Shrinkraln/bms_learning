/**
 * @file    bms_app.h
 * @brief   BMS 主应用 — 6 任务事件驱动架构
 *
 *          任务:
 *          ┌────────────────┬──────────────┬──────────┬────────┬──────────────────────┐
 *          │ Task           │ CMSIS Prio   │ Period   │ Stack  │ Trigger              │
 *          ├────────────────┼──────────────┼──────────┼────────┼──────────────────────┤
 *          │ task_protect   │ Realtime(48) │ event    │ 1024B  │ EventFlagsWait       │
 *          │ task_sample    │ AboveNormal  │ 100ms    │ 2048B  │ Semaphore ← TIM2 ISR │
 *          │ task_can_rx    │ Normal(24)   │ 50ms     │ 1024B  │ osDelay polling      │
 *          │ task_balance   │ BelowNormal  │ 500ms    │ 1024B  │ DATA_READY → Delay   │
 *          │ task_soc       │ BelowNormal  │ 1000ms   │ 2048B  │ DATA_READY → Delay   │
 *          │ task_can_tx    │ Low(8)       │ 100ms    │ 1024B  │ osDelay + wdg_kick   │
 *          └────────────────┴──────────────┴──────────┴────────┴──────────────────────┘
 */

#ifndef APP_BMS_APP_H
#define APP_BMS_APP_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 同步原语 — ISR/stm32f1xx_it.c 需要访问 */
extern osSemaphoreId_t sem_sample;

/* 故障掩码 */
#define ALL_FAULTS      0x0FFFU
#define PROT_RECOVERED  0x0001U

void bms_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BMS_APP_H */
