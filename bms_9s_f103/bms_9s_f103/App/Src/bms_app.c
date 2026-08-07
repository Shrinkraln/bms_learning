/**
 * @file    bms_app.c
 * @brief   BMS 主应用 — 6 任务 + 7 同步原语 + 1 ring_buf
 */

#include "bms_app.h"
#include "bms_shared.h"
#include "soc_ocv.h"
#include "can_cmd.h"
#include "systick.h"
#include "led.h"
#include "io_ctrl.h"
#include "i2c_sw.h"
#include "can_drv.h"
#include "timer.h"
#include "wdg.h"
#include "bq76940.h"
#include "protection.h"
#include "ring_buf.h"

/* ============================================================
 * 同步原语 (全局 — stm32f1xx_it.c 需要 sem_sample)
 * ============================================================ */

osSemaphoreId_t sem_sample      = NULL;
static osMutexId_t mutex_iic     = NULL;
static osMutexId_t mutex_data    = NULL;
static osMutexId_t mutex_settings = NULL;
static osMutexId_t mutex_can_tx = NULL;
static osEventFlagsId_t evt_protect = NULL;
static osEventFlagsId_t evt_data_ready = NULL;

/* CAN TX 环形缓冲区 */
#define Q_CAN_TX_DEPTH 24U
static uint8_t   q_can_tx_buf[Q_CAN_TX_DEPTH * sizeof(can_msg_t)];
static ring_buf_t q_can_tx;

/* 首次数据就绪标记 */
static uint8_t first_data_ready = 0U;
/* 上次故障状态 (用于检测 0→非0 和 非0→0) */
static uint16_t last_active_faults = 0x0000U;
/* 通信错误计数 */
static uint8_t comm_err_cnt = 0U;

/* FET 与均衡状态 (供 can_pub 读取) */
uint8_t g_fet_chg_on       = 1U;
uint8_t g_fet_dsg_on       = 1U;
uint8_t g_balancing_active = 0U;
uint8_t g_afe_online       = 0U;  /**< AFE 在线标志: 1=正常, 0=离线/I2C失败 */

/* ============================================================
 * 任务函数前向声明
 * ============================================================ */

static void task_sample_entry(void *arg);
static void task_protect_entry(void *arg);
static void task_can_rx_entry(void *arg);
static void task_balance_entry(void *arg);
static void task_soc_entry(void *arg);
static void task_can_tx_entry(void *arg);

/* ============================================================
 * bms_app_init — 初始化全部模块 + 创建任务
 * ============================================================ */

void bms_app_init(void)
{
    /* ---- 1. BSP 层初始化 ---- */
    bsp_tick_init();
    led_init();
    io_ctrl_init();
    i2c_sw_init();
    can_drv_init();
    timer_init();
    wdg_init(1000U);

    /* ---- 2. 创建同步原语 ---- */
    mutex_iic      = osMutexNew(NULL);
    mutex_data     = osMutexNew(NULL);
    mutex_settings = osMutexNew(NULL);
    mutex_can_tx   = osMutexNew(NULL);
    sem_sample     = osSemaphoreNew(1U, 0U, NULL);   /* binary, initially 0 */
    evt_protect    = osEventFlagsNew(NULL);
    evt_data_ready = osEventFlagsNew(NULL);

    /* ---- 3. CAN TX 环形缓冲区 ---- */
    ring_buf_init(&q_can_tx, q_can_tx_buf,
                  (uint16_t)sizeof(can_msg_t), Q_CAN_TX_DEPTH);

    /* ---- 4. App 层初始化 ---- */
    bms_shared_init();
    bms_shared_set_data_mutex(mutex_data);
    bms_shared_set_settings_mutex(mutex_settings);

    soc_ocv_init(0);

    bq76940_cfg_t bq_cfg = {
        .r_sense_mohm    = 4U,
        .cell_ov_mv      = 4250U,
        .cell_uv_mv      = 2800U,
        .discharge_oc_ma = 30000U,
        .charge_oc_ma    = 15000U,
    };
    (void)bq76940_init(&bq_cfg);

    protection_init();
    can_cmd_init();

    /* ---- 5. 创建 6 个任务 ---- */
    const osThreadAttr_t sample_attr = {
        .name = "sample", .stack_size = 512U, .priority = osPriorityAboveNormal };
    const osThreadAttr_t protect_attr = {
        .name = "protect", .stack_size = 256U, .priority = osPriorityRealtime };
    const osThreadAttr_t can_rx_attr = {
        .name = "canRx", .stack_size = 256U, .priority = osPriorityNormal };
    const osThreadAttr_t balance_attr = {
        .name = "balance", .stack_size = 256U, .priority = osPriorityBelowNormal };
    const osThreadAttr_t soc_attr = {
        .name = "soc", .stack_size = 512U, .priority = osPriorityBelowNormal };
    const osThreadAttr_t can_tx_attr = {
        .name = "canTx", .stack_size = 256U, .priority = osPriorityLow };

    (void)osThreadNew(task_protect_entry, NULL, &protect_attr);
    (void)osThreadNew(task_sample_entry,  NULL, &sample_attr);
    (void)osThreadNew(task_can_rx_entry,  NULL, &can_rx_attr);
    (void)osThreadNew(task_balance_entry, NULL, &balance_attr);
    (void)osThreadNew(task_soc_entry,     NULL, &soc_attr);
    (void)osThreadNew(task_can_tx_entry,  NULL, &can_tx_attr);

    led_on();
}

/* ============================================================
 * task_sample — 数据采集 + 保护预检 (100ms, prio 32)
 * ============================================================ */

static void task_sample_entry(void *arg)
{
    (void)arg;
    bq76940_data_t data;

    for (;;) {
        osSemaphoreAcquire(sem_sample, osWaitForever);  /* ← TIM2 ISR 唤醒 */

        /* ① 获取 I2C 总线 */
        osMutexAcquire(mutex_iic, osWaitForever);
        bq76940_status_t ret = bq76940_read_all(&data);

        if (ret != BQ76940_OK) {
            comm_err_cnt++;
            if (comm_err_cnt >= 3U) {
                osEventFlagsSet(evt_protect, FAULT_COMM_LOSS);
                g_afe_online = 0U;
            }
            osMutexRelease(mutex_iic);
            continue;
        }
        comm_err_cnt = 0U;
        g_afe_online = 1U;

        /* ② settings 快照 (零初始化: 锁超时时阈值为 0, 跳过所有检查) */
        bms_settings_t snapshot;
        (void)memset(&snapshot, 0, sizeof(snapshot));
        bms_settings_t *settings = bms_shared_settings_lock(10U);
        if (settings != NULL) {
            snapshot = *settings;
            bms_shared_settings_unlock();
        }

        /* ③ 获取 data 锁 (嵌套在 iic 内) */
        osMutexAcquire(mutex_data, osWaitForever);

        /* ④ 写入采集数据 */
        bms_shared_get_ptr()->battery_val = data;

        /* ⑤ 保护检查 */
        prot_result_t prot = protection_check(&data, &snapshot);

        /* ⑥ 写入保护结果 */
        bms_shared_get_ptr()->prot_level    = prot.level;
        bms_shared_get_ptr()->active_faults = prot.active_faults;

        /* ⑦ 故障通知 */
        uint16_t current_faults = prot.active_faults;
        if (current_faults != 0U && current_faults != last_active_faults) {
            /* 故障进入: set 对应故障位 */
            osEventFlagsSet(evt_protect, (uint32_t)current_faults);
        } else if (current_faults == 0U && last_active_faults != 0U) {
            /* 故障恢复 */
            osEventFlagsSet(evt_protect, PROT_RECOVERED);
        }
        last_active_faults = current_faults;

        /* ⑧ 首次数据就绪 */
        if (first_data_ready == 0U) {
            first_data_ready = 1U;
            osEventFlagsSet(evt_data_ready, 0x01U);
        }

        osMutexRelease(mutex_data);
        osMutexRelease(mutex_iic);
    }
}

/* ============================================================
 * task_protect — 故障响应 (事件驱动, prio 48 = osPriorityRealtime)
 * ============================================================ */

static void task_protect_entry(void *arg)
{
    (void)arg;
    can_msg_t fault_frame;

    for (;;) {
        uint32_t flags = osEventFlagsWait(evt_protect,
                                           ALL_FAULTS | PROT_RECOVERED,
                                           osFlagsWaitAny, osWaitForever);

        /* ① 获取 I2C 总线 */
        osMutexAcquire(mutex_iic, osWaitForever);

        /* ② 读取保护状态 (无需持 mutex_data: sample 已写完并释放) */
        bms_shared_t *bms = bms_shared_get_ptr();
        uint16_t faults    = bms->active_faults;
        prot_level_t level = bms->prot_level;

        uint8_t ctrl2_mask = (uint8_t)(BQ76940_SYS_CTRL2_CHG_FET
                                       | BQ76940_SYS_CTRL2_DSG_FET);
        uint8_t ctrl2_val  = 0U;

        if (flags & PROT_RECOVERED) {
            /* ③ 故障恢复: 全开 FET */
            ctrl2_val = (uint8_t)(BQ76940_SYS_CTRL2_CHG_FET
                                  | BQ76940_SYS_CTRL2_DSG_FET);
        } else if (level >= PROT_LVL_FAULT) {
            /* ④ 严重故障: 全关 */
            ctrl2_val = 0U;
        } else if (level == PROT_LVL_ALERT) {
            if (faults & (FAULT_CELL_OV | FAULT_PACK_OV | FAULT_CHARGE_OC)) {
                ctrl2_val = BQ76940_SYS_CTRL2_DSG_FET;  /* 关充电, 保留放电 */
            } else if (faults & (FAULT_CELL_UV | FAULT_PACK_UV)) {
                ctrl2_val = BQ76940_SYS_CTRL2_CHG_FET;  /* 关放电, 保留充电 */
            }
        }

        bq76940_write_sys_ctrl2(ctrl2_mask, ctrl2_val);
        /* 同步 FET 状态到全局 (供 can_pub 的 0x120 ctrl 字节读取) */
        g_fet_chg_on = (ctrl2_val & BQ76940_SYS_CTRL2_CHG_FET) ? 1U : 0U;
        g_fet_dsg_on = (ctrl2_val & BQ76940_SYS_CTRL2_DSG_FET) ? 1U : 0U;
        osMutexRelease(mutex_iic);

        /* ⑤ 生成故障 CAN 帧 (新 layout: uint16 电压, 无电流) */
        fault_frame.id  = CAN_TX_BMS_FAULT;
        fault_frame.len = 8U;
        fault_frame.data[0] = (uint8_t)level;
        fault_frame.data[1] = (uint8_t)((faults >> 8U) & 0xFFU);
        fault_frame.data[2] = (uint8_t)(faults & 0xFFU);
        /* max_cell_mV — uint16 big-endian */
        {
            uint16_t max_mv = bms->battery_val.cells.max_mv;
            fault_frame.data[3] = (uint8_t)((max_mv >> 8U) & 0xFFU);
            fault_frame.data[4] = (uint8_t)(max_mv & 0xFFU);
        }
        /* min_cell_mV — uint16 big-endian */
        {
            uint16_t min_mv = bms->battery_val.cells.min_mv;
            fault_frame.data[5] = (uint8_t)((min_mv >> 8U) & 0xFFU);
            fault_frame.data[6] = (uint8_t)(min_mv & 0xFFU);
        }
        /* max_temp + 40°C offset (1°C resolution) */
        {
            int16_t max_temp = bms->battery_val.temps.ts_mdeg_c[0];
            for (uint8_t i = 1U; i < BQ76940_TS_COUNT; i++) {
                if (bms->battery_val.temps.ts_mdeg_c[i] > max_temp) {
                    max_temp = bms->battery_val.temps.ts_mdeg_c[i];
                }
            }
            fault_frame.data[7] = (uint8_t)((max_temp / 10) + 40);
        }

        /* ⑥ 头插优先发送 (持 mutex_can_tx 保护 ring_buf) */
        osMutexAcquire(mutex_can_tx, osWaitForever);
        ring_buf_put_front(&q_can_tx, &fault_frame);
        osMutexRelease(mutex_can_tx);
    }
}

/* ============================================================
 * task_can_rx — CAN 下行指令处理 (50ms 轮询, prio 24)
 * ============================================================ */

static void task_can_rx_entry(void *arg)
{
    (void)arg;
    can_msg_t msg;

    for (;;) {
        osDelay(50U);

        while (can_available() > 0U) {
            if (can_recv(&msg) != 0U) { continue; }

            bms_shared_t *bms = bms_shared_data_lock(10U);
            uint16_t faults = (bms != NULL) ? bms->active_faults : 0x0000U;
            if (bms != NULL) { bms_shared_data_unlock(); }

            can_msg_t resp_frame;
            (void)memset(&resp_frame, 0, sizeof(resp_frame));
            can_action_req_t req = can_cmd_dispatch(&msg, faults, &resp_frame);

            /* 查询响应帧立即发送 */
            if (resp_frame.len > 0U) {
                can_send(&resp_frame);
            }

            /* 执行 BSP 操作 (任务层是唯一有权调用 BSP 的代码) */
            switch (req.action) {
            case CAN_ACTION_CLEAR_FAULT:
                bq76940_clear_faults();
                break;
            case CAN_ACTION_FET_CHG_ON:
                bq76940_write_sys_ctrl2(BQ76940_SYS_CTRL2_CHG_FET,
                                         BQ76940_SYS_CTRL2_CHG_FET);
                g_fet_chg_on = 1U;
                break;
            case CAN_ACTION_FET_CHG_OFF:
                bq76940_write_sys_ctrl2(BQ76940_SYS_CTRL2_CHG_FET, 0U);
                g_fet_chg_on = 0U;
                break;
            case CAN_ACTION_FET_DSG_ON:
                bq76940_write_sys_ctrl2(BQ76940_SYS_CTRL2_DSG_FET,
                                         BQ76940_SYS_CTRL2_DSG_FET);
                g_fet_dsg_on = 1U;
                break;
            case CAN_ACTION_FET_DSG_OFF:
                bq76940_write_sys_ctrl2(BQ76940_SYS_CTRL2_DSG_FET, 0U);
                g_fet_dsg_on = 0U;
                break;
            case CAN_ACTION_BALANCE_SET:
                osMutexAcquire(mutex_iic, osWaitForever);
                bq76940_set_balancing(req.balance_mask);
                osMutexRelease(mutex_iic);
                g_balancing_active = (req.balance_mask != 0U) ? 1U : 0U;
                break;
            case CAN_ACTION_BALANCE_OFF:
                osMutexAcquire(mutex_iic, osWaitForever);
                bq76940_balance_off();
                osMutexRelease(mutex_iic);
                g_balancing_active = 0U;
                break;
            case CAN_ACTION_SHUTDOWN:
                bq76940_shutdown();
                break;
            case CAN_ACTION_NONE:
            default:
                break;
            }
        }
    }
}

/* ============================================================
 * task_balance — 电池均衡 (500ms, prio 16)
 * ============================================================ */

static void task_balance_entry(void *arg)
{
    (void)arg;

    /* 首次阻塞等待数据就绪 */
    osEventFlagsWait(evt_data_ready, 0x01U, osFlagsWaitAny, 5000U);

    for (;;) {
        osDelay(500U);

        bms_shared_t *bms = bms_shared_data_lock(10U);
        if (bms == NULL) { continue; }

        /* 仅无故障时执行均衡 */
        if (bms->prot_level < PROT_LVL_FAULT) {
            const bq76940_cell_data_t *cells = &bms->battery_val.cells;
            uint16_t thresh_mv = bms->settings.balance_thresh_mv;

            if (thresh_mv > 0U && cells->diff_mv > thresh_mv) {
                /* 压差超阈值: 开启高电压电芯均衡 */
                uint16_t mask = 0U;
                for (uint8_t i = 0U; i < 9U; i++) {
                    if (cells->cell_mv[i] > (cells->min_mv + thresh_mv)) {
                        mask |= (uint16_t)(1U << i);
                    }
                }
                bms_shared_data_unlock();

                osMutexAcquire(mutex_iic, osWaitForever);
                bq76940_set_balancing(mask);
                osMutexRelease(mutex_iic);
                g_balancing_active = (mask != 0U) ? 1U : 0U;
            } else if (cells->diff_mv <= (thresh_mv / 2U)) {
                /* 压差已缩小: 关闭均衡 */
                bms_shared_data_unlock();

                osMutexAcquire(mutex_iic, osWaitForever);
                bq76940_balance_off();
                osMutexRelease(mutex_iic);
                g_balancing_active = 0U;
            } else {
                bms_shared_data_unlock();
            }
        } else {
            bms_shared_data_unlock();
        }
    }
}

/* ============================================================
 * task_soc — SOC/OCV 更新 (1000ms, prio 16)
 * ============================================================ */

static void task_soc_entry(void *arg)
{
    (void)arg;

    /* 首次阻塞等待数据就绪 */
    osEventFlagsWait(evt_data_ready, 0x01U, osFlagsWaitAny, 5000U);

    uint32_t last_ms = bsp_tick_get();

    for (;;) {
        osDelay(1000U);

        bms_shared_t *bms = bms_shared_data_lock(10U);
        if (bms == NULL) { continue; }

        uint16_t cell_min  = bms->battery_val.cells.min_mv;
        int32_t  current   = bms->battery_val.current.current_ma;

        bms_shared_data_unlock();

        uint32_t now   = bsp_tick_get();
        uint32_t dt_ms = now - last_ms;
        last_ms = now;

        /* SOC/OCV 更新 (3 参数, 无 temp_mdeg) */
        soc_ocv_update(cell_min, current, dt_ms);

        /* 写回共享数据 */
        bms_shared_update_soc(soc_ocv_get_soc(),
                              soc_ocv_get_ocv(),
                              soc_ocv_get_remaining_mah());
    }
}

/* ============================================================
 * task_can_tx — CAN 上报 + 喂狗 (100ms, prio 8)
 * ============================================================ */

static void task_can_tx_entry(void *arg)
{
    (void)arg;

    for (;;) {
        osDelay(100U);

        /* 喂狗 (最低优先级任务兼) */
        wdg_kick();

        /* 快照拷贝共享数据 */
        bms_shared_t *bms = bms_shared_data_lock(10U);
        if (bms != NULL) {
            can_msg_t frames[5];
            uint8_t count = can_pub(bms, frames);

            /* 周期帧尾插入队列 (持 mutex_can_tx 保护) */
            for (uint8_t i = 0U; i < count; i++) {
                osMutexAcquire(mutex_can_tx, osWaitForever);
                ring_buf_put(&q_can_tx, &frames[i]);
                osMutexRelease(mutex_can_tx);
            }
            bms_shared_data_unlock();
        }

        /* 发送队列中所有帧 (每帧独立持锁, 允许 protect 帧头插优先) */
        can_msg_t frame;
        for (;;) {
            osMutexAcquire(mutex_can_tx, osWaitForever);
            uint16_t avail = ring_buf_available(&q_can_tx);
            uint8_t  ret   = 1U;
            if (avail > 0U) {
                ret = ring_buf_get(&q_can_tx, &frame);
            }
            osMutexRelease(mutex_can_tx);

            if (avail == 0U || ret != 0U) { break; }
            can_send(&frame);
        }
    }
}
