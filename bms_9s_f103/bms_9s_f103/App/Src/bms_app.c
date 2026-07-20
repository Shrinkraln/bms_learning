/**
 * @file    bms_app.c
 * @brief   BMS 主应用实现 — 初始化 + 所有任务函数
 */

#include "bms_app.h"
#include "bms_shared.h"
#include "soc_ocv.h"
#include "can_cmd.h"
#include "systick.h"
#include "led.h"
#include "io_ctrl.h"
#include "i2c_sw.h"
#include "usart_drv.h"
#include "can_drv.h"
#include "timer.h"
#include "wdg.h"
#include "bq76940.h"
#include "protection.h"
#include "data_report.h"

/* ============================================================
 * 任务句柄（供调试/监控使用）
 * ============================================================ */

static osThreadId_t task_acq_handle    = NULL;
static osThreadId_t task_prot_handle   = NULL;
static osThreadId_t task_soc_handle    = NULL;
static osThreadId_t task_can_tx_handle = NULL;
static osThreadId_t task_can_rx_handle = NULL;
static osThreadId_t task_wdg_handle    = NULL;

/* ============================================================
 * 任务属性
 * ============================================================ */

static const osThreadAttr_t acq_task_attr = {
    .name       = "acq",
    .stack_size = 512U * 4U,
    .priority   = osPriorityNormal,
};

static const osThreadAttr_t prot_task_attr = {
    .name       = "prot",
    .stack_size = 256U * 4U,
    .priority   = osPriorityAboveNormal,
};

static const osThreadAttr_t soc_task_attr = {
    .name       = "soc",
    .stack_size = 512U * 4U,
    .priority   = osPriorityNormal,
};

static const osThreadAttr_t can_tx_task_attr = {
    .name       = "canTx",
    .stack_size = 256U * 4U,
    .priority   = osPriorityNormal,
};

static const osThreadAttr_t can_rx_task_attr = {
    .name       = "canRx",
    .stack_size = 512U * 4U,
    .priority   = osPriorityHigh,
};

static const osThreadAttr_t wdg_task_attr = {
    .name       = "wdg",
    .stack_size = 128U * 4U,
    .priority   = osPriorityLow,
};

/* ============================================================
 * 任务函数前向声明
 * ============================================================ */

static void acquisition_task(void *arg);
static void protection_task(void *arg);
static void soc_task(void *arg);
static void watchdog_task(void *arg);

/* ============================================================
 * bms_app_init — 入口
 * ============================================================ */

void bms_app_init(void)
{
    /* ---- 1. BSP 层初始化 ---- */
    bsp_tick_init();
    led_init();
    io_ctrl_init();
    i2c_sw_init();
    usart_drv_init();
    can_drv_init();
    timer_init();

    /* 启动看门狗 (超时 1000ms) */
    wdg_init(1000U);

    /* ---- 2. 共享数据中心 ---- */
    bms_shared_init();

    /* ---- 3. SOC/OCV 模块 ---- */
    soc_ocv_init(0);  /* 使用默认 20000mAh */

    /* ---- 4. BQ76940 初始化 ---- */
    bq76940_status_t ret = bq76940_init(NULL);
    if (ret != BQ76940_OK) {
        /* BQ76940 通信失败 — 红色 LED 常亮 */
        led_on(LED_ID_2);
    }

    /* ---- 5. 保护模块 ---- */
    protection_init();

    /* ---- 6. 数据上报模块 ---- */
    data_report_init();

    /* ---- 7. CAN 指令模块（创建消息队列 + 注册回调） ---- */
    can_cmd_init();

    /* ---- 8. 创建 FreeRTOS 任务 ---- */
    task_can_rx_handle = osThreadNew(can_cmd_task_entry, NULL,
                                      &can_rx_task_attr);
    task_prot_handle   = osThreadNew(protection_task, NULL,
                                      &prot_task_attr);
    task_acq_handle    = osThreadNew(acquisition_task, NULL,
                                      &acq_task_attr);
    task_can_tx_handle = osThreadNew(can_tx_task_entry, NULL,
                                      &can_tx_task_attr);
    task_soc_handle    = osThreadNew(soc_task, NULL,
                                      &soc_task_attr);
    task_wdg_handle    = osThreadNew(watchdog_task, NULL,
                                      &wdg_task_attr);

    /* ---- 9. 绿色 LED 指示初始化完成 ---- */
    led_on(LED_ID_1);
}

/* ============================================================
 * 采集任务 (100ms)
 * ============================================================ */

static void acquisition_task(void *arg)
{
    (void)arg;

    /* 等待 BQ76940 初始化稳定 */
    osDelay(200U);

    bq76940_data_t data;

    for (;;) {
        /* 读取 BQ76940 全部数据 */
        bq76940_status_t ret = bq76940_read_all(&data);

        if (ret == BQ76940_OK) {
            /* 更新共享数据中心 */
            bms_shared_update_bq_data(&data);
        }
        /* 通信失败时不更新数据，保持旧值 */

        osDelay(ACQ_TASK_PERIOD_MS);
    }
}

/* ============================================================
 * 保护任务 (10ms — 高频检查)
 * ============================================================ */

static void protection_task(void *arg)
{
    (void)arg;
    osDelay(100U);  /* 等待首次采集完成 */

    uint8_t led_fault = 0U;

    for (;;) {
        /* 从共享数据获取最新采集 */
        bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
        if (bms == NULL) { osDelay(PROT_TASK_PERIOD_MS); continue; }

        bq76940_data_t data = bms->bq_data;  /* 本地拷贝 */
        bms_shared_data_unlock();

        /* 执行保护检查 */
        prot_level_t level = protection_check(&data);

        /* 更新保护状态 */
        const protection_ctx_t *ctx = protection_get_ctx();
        bms_shared_update_protection(ctx, level);

        /* LED 指示: 故障时红灯闪烁 */
        if (level >= PROT_LVL_ALERT) {
            led_fault = (uint8_t)(led_fault ^ 1U);
            if (led_fault) {
                led_on(LED_ID_2);
            } else {
                led_off(LED_ID_2);
            }
        } else if (level == PROT_LVL_NONE) {
            led_off(LED_ID_2);
        }

        osDelay(PROT_TASK_PERIOD_MS);
    }
}

/* ============================================================
 * SOC / OCV 任务 (1000ms)
 * ============================================================ */

static void soc_task(void *arg)
{
    (void)arg;
    osDelay(500U);  /* 等待采集稳定 */

    uint32_t last_ms = bsp_tick_get();

    for (;;) {
        bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
        if (bms == NULL) { osDelay(SOC_TASK_PERIOD_MS); continue; }

        uint16_t  cell_min  = bms->bq_data.cells.min_mv;
        int32_t   current   = bms->bq_data.current.current_ma;
        bms_shared_data_unlock();

        uint32_t now   = bsp_tick_get();
        uint32_t dt_ms = now - last_ms;
        last_ms = now;

        /* 更新 SOC/OCV */
        soc_ocv_update(cell_min, current, dt_ms);

        /* 写回共享数据 */
        bms_shared_update_soc(soc_ocv_get_soc(),
                              soc_ocv_get_ocv(),
                              soc_ocv_get_remaining_mah());

        osDelay(SOC_TASK_PERIOD_MS);
    }
}

/* ============================================================
 * 看门狗 + LED 心跳任务 (500ms)
 * ============================================================ */

static void watchdog_task(void *arg)
{
    (void)arg;

    uint8_t heartbeat = 0U;

    for (;;) {
        /* 喂狗 */
        wdg_kick();

        /* LED 心跳闪烁（正常: 慢闪, 故障: 保护任务控制红灯） */
        heartbeat = (uint8_t)(heartbeat ^ 1U);

        /* 仅在无故障时绿灯心跳 */
        bms_shared_t *bms = bms_shared_data_lock(100U);
        if (bms != NULL) {
            if (bms->prot_level == PROT_LVL_NONE) {
                if (heartbeat) {
                    led_on(LED_ID_1);
                } else {
                    led_off(LED_ID_1);
                }
            }
            bms_shared_data_unlock();
        }

        osDelay(WDG_TASK_PERIOD_MS);
    }
}
