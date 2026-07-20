/**
 * @file    bms_app.c
 * @brief   BMS 主应用实现
 * @note    系统初始化顺序:
 *          1. HAL 初始化 (HAL_Init → SystemClock_Config)
 *          2. 外设初始化 (MX_GPIO, MX_CAN, MX_I2C, MX_USART, ...)
 *          3. bms_app_init() — BSP + App 模块初始化
 *          4. FreeRTOS 初始化 (osKernelInitialize → MX_FREERTOS_Init)
 *          5. 启动调度器 (osKernelStart)
 *          6. bms_task_entry() 开始运行
 */

#include "bms_app.h"
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
 * 配置
 * ============================================================ */

/** @brief BMS 主循环周期 (ms) */
#define BMS_LOOP_PERIOD_MS      10U

/** @brief 看门狗超时 (ms) — 需大于主循环周期 */
#define BMS_WDG_TIMEOUT_MS      500U

/** @brief 数据上报周期 (循环次数 = 上报周期/主循环周期) */
#define REPORT_FAST_CYCLES      (REPORT_PERIOD_FAST / BMS_LOOP_PERIOD_MS)
#define REPORT_SLOW_CYCLES      (REPORT_PERIOD_SLOW / BMS_LOOP_PERIOD_MS)

/** @brief LED 闪烁周期 (ms) */
#define LED_BLINK_NORMAL_MS     500U
#define LED_BLINK_FAULT_MS      100U
#define LED_BLINK_FAST_MS       50U

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief BMS 全局上下文 */
static bms_ctx_t bms_ctx;

/* ============================================================
 * 状态字符串
 * ============================================================ */

const char *bms_state_str(bms_state_t state)
{
    switch (state) {
        case BMS_STATE_INIT:       return "INIT";
        case BMS_STATE_IDLE:       return "IDLE";
        case BMS_STATE_CHARGE:     return "CHARGE";
        case BMS_STATE_DISCHARGE:  return "DISCHARGE";
        case BMS_STATE_FAULT:      return "FAULT";
        case BMS_STATE_SHUTDOWN:   return "SHUTDOWN";
        default:                   return "UNKNOWN";
    }
}

/* ============================================================
 * 初始化
 * ============================================================ */

void bms_app_init(void)
{
    /* ---- BSP 层初始化 ---- */
    bsp_tick_init();
    led_init();
    io_ctrl_init();
    i2c_sw_init();
    usart_drv_init();
    can_drv_init();
    timer_init();

    /* ---- 启动看门狗 ---- */
    wdg_init(BMS_WDG_TIMEOUT_MS);

    /* ---- BQ76940 初始化（使用默认保护阈值） ---- */
    bq76940_status_t ret = bq76940_init(NULL);
    if (ret != BQ76940_OK) {
        /* BQ76940 初始化失败 — 通过 LED 指示错误 */
        led_on(LED_ID_2);
        /* 继续运行，后续循环会重试通信 */
    }

    /* ---- App 层初始化 ---- */
    protection_init();
    data_report_init();

    /* ---- 系统状态 ---- */
    bms_ctx.state      = BMS_STATE_INIT;
    bms_ctx.loop_count = 0U;
    bms_ctx.uptime_ms  = 0U;

    /* 指示初始化完成 */
    led_on(LED_ID_1);
}

/* ============================================================
 * 主任务
 * ============================================================ */

void bms_task_entry(void *argument)
{
    (void)argument;

    /* 过渡到 IDLE 状态 */
    bms_ctx.state = BMS_STATE_IDLE;

    uint32_t last_led_toggle = bsp_tick_get();
    uint8_t  led_val = 1U;

    for (;;) {
        /* ---- 喂狗 ---- */
        wdg_kick();

        /* ---- 采集数据 ---- */
        bq76940_status_t ret = bq76940_read_all(&bms_ctx.data);

        if (ret != BQ76940_OK) {
            /* 通信失败 — 使用上次数据，增加失败计数 */
            bms_ctx.data.cells.valid = 0U;
            bms_ctx.data.temps.valid = 0U;
        }

        /* ---- 保护检查 ---- */
        prot_level_t prot_level = protection_check(&bms_ctx.data);

        /* 同步保护上下文 */
        const protection_ctx_t *prot = protection_get_ctx();
        if (prot != NULL) {
            bms_ctx.prot = *prot;
        }

        /* ---- 状态机 ---- */
        switch (bms_ctx.state) {
            case BMS_STATE_IDLE:
            case BMS_STATE_CHARGE:
            case BMS_STATE_DISCHARGE:
                /* 判断充放电状态 */
                if (bms_ctx.data.current.current_ma > 500) {
                    bms_ctx.state = BMS_STATE_CHARGE;
                } else if (bms_ctx.data.current.current_ma < -500) {
                    bms_ctx.state = BMS_STATE_DISCHARGE;
                } else {
                    bms_ctx.state = BMS_STATE_IDLE;
                }

                /* 进入故障 */
                if (prot_level >= PROT_LVL_FAULT) {
                    bms_ctx.state = BMS_STATE_FAULT;
                }
                break;

            case BMS_STATE_FAULT:
                /* 故障锁存：需手动或外部命令清除 */
                if (prot_level == PROT_LVL_NONE) {
                    /* 故障已清除，但需要显式复位锁存 */
                    /* 这里保持 FAULT 直到外部清除 */
                }
                break;

            case BMS_STATE_SHUTDOWN:
                /* 关机 — 不再喂狗，系统将复位 */
                break;

            default:
                bms_ctx.state = BMS_STATE_IDLE;
                break;
        }

        /* ---- 数据上报 ---- */
        uint16_t cycle = (uint16_t)(bms_ctx.loop_count % REPORT_SLOW_CYCLES);

        if (cycle == 0U) {
            /* 慢周期：全量上报 */
            data_report_all(&bms_ctx.data, &bms_ctx.prot);
        } else if ((cycle % (REPORT_FAST_CYCLES)) == 0U) {
            /* 快周期：CAN 上报 */
            data_report_can(&bms_ctx.data, &bms_ctx.prot);
        }

        /* ---- LED 指示 ---- */
        uint32_t now = bsp_tick_get();
        uint32_t blink_period;

        switch (prot_level) {
            case PROT_LVL_FAULT:  blink_period = LED_BLINK_FAST_MS;  break;
            case PROT_LVL_ALERT:  blink_period = LED_BLINK_FAULT_MS;  break;
            default:              blink_period = LED_BLINK_NORMAL_MS; break;
        }

        if (bsp_tick_elapsed(last_led_toggle) >= blink_period) {
            last_led_toggle = now;
            led_val = (uint8_t)(led_val ^ 1U);
            if (led_val) {
                led_on(LED_ID_1);
            } else {
                led_off(LED_ID_1);
            }
        }

        /* ---- 更新计数 ---- */
        bms_ctx.loop_count++;
        bms_ctx.uptime_ms += BMS_LOOP_PERIOD_MS;

        /* ---- 延时 ---- */
        osDelay(BMS_LOOP_PERIOD_MS);
    }
}

/* ============================================================
 * 上下文查询
 * ============================================================ */

const bms_ctx_t *bms_get_ctx(void)
{
    return &bms_ctx;
}
