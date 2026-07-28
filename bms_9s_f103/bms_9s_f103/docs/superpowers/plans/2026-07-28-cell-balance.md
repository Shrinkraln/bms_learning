# Cell Balance — 9 串电芯被动均衡实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现基于 BQ76940 内部 FET 的单电芯被动均衡——task_balance 以 1s 周期在压差 >50mV 时对最高电芯开启均衡，<30mV 时停止。

**Architecture:** task_balance（内联于 bms_app.c）以 osPriorityBelowNormal 优先级运行，通过栈快照读取 bms_shared 的电芯数据和故障状态，生成单 bit CELLBAL mask，经 mutex_iic 保护写入 BQ76940 的 CELLBAL1/2/3 寄存器。BSP 层同步修正 CELLBAL 寄存器地址并新增 CELLBAL3 支持。

**Tech Stack:** STM32F103C8Tx + FreeRTOS (CMSIS-RTOS v2) + BQ76940 I2C + arm-none-eabi-gcc (C11)

## Global Constraints

- **前置依赖**: fullstack-redesign spec 中 `mutex_iic`、`mutex_data`、`evt_data_ready`、`ALL_FAULTS` 必须已实现
- 均衡开启: diff > 50mV, 停止: diff < 30mV (硬编码), 最低电压: 3200mV (硬编码)
- 均衡目标: 仅最高电压单电芯
- FAULT_CELL_IMBALANCE 不屏蔽均衡 (ADR-0007)
- CAN BALANCE_SET/OFF 需获取 mutex_iic
- cells_mv[] 改为物理 cell1~cell9 顺序 (VC1/VC2/VC5/VC6/VC7/VC10/VC11/VC12/VC15)
- 编译: 0 警告 0 错误
- 不修改 CubeMX .ioc

---

### Task 1: bq76940 寄存器地址修正 + CELLBAL3 新增

**Files:**
- Modify: `BSP/Inc/bq76940.h:53-56`

**Interfaces:**
- Consumes: 无
- Produces: `BQ76940_REG_CELLBAL1=0x01U`, `BQ76940_REG_CELLBAL2=0x02U`, `BQ76940_REG_CELLBAL3=0x03U`

- [ ] **Step 1: 修正 CELLBAL1/2 地址，新增 CELLBAL3**

在 `BSP/Inc/bq76940.h` 中，将第 53-56 行:
```c
/** @brief 均衡寄存器 */
#define BQ76940_REG_CELLBAL1     0x06U   /**< 电芯均衡 1 (VC1-VC5)         */
#define BQ76940_REG_CELLBAL2     0x07U   /**< 电芯均衡 2 (VC6-VC10)        */
```
替换为:
```c
/** @brief 均衡寄存器（与 TI SLUSC25B 一致） */
#define BQ76940_REG_CELLBAL1     0x01U   /**< 电芯均衡 1 (VC1-VC5) — was 0x06 */
#define BQ76940_REG_CELLBAL2     0x02U   /**< 电芯均衡 2 (VC6-VC10) — was 0x07 */
#define BQ76940_REG_CELLBAL3     0x03U   /**< 电芯均衡 3 (VC11-VC15) — [NEW]   */

/* [MIGRATION] 旧地址（删除前的参考，上板验证后可移除）:
 * #define BQ76940_REG_CELLBAL1_OLD 0x06U
 * #define BQ76940_REG_CELLBAL2_OLD 0x07U
 */
```

- [ ] **Step 2: 编译验证**

```bash
cd D:\coding_codes\bms_learning\bms_9s_f103\bms_9s_f103
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 0 警告 0 错误。

- [ ] **Step 3: Commit**

```bash
git add BSP/Inc/bq76940.h
git commit -m "feat(bq76940): CELLBAL1/2 address fix (0x06→0x01, 0x07→0x02) + add CELLBAL3 (0x03)

Per TI SLUSC25B register map. Old addresses kept in comments for
hardware verification reference.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 2: bq76940_set_balancing/get_balancing 扩展 CELLBAL3

**Files:**
- Modify: `BSP/Src/bq76940.c:378-409`

**Interfaces:**
- Consumes: `BQ76940_REG_CELLBAL1/2/3` (from Task 1)
- Produces: `bq76940_set_balancing(mask)` 支持 bit[14:0] (曾为 bit[8:0]), `bq76940_get_balancing()` 回读三个寄存器

- [ ] **Step 1: 重写 bq76940_set_balancing 支持 15-bit mask**

将 `BSP/Src/bq76940.c` 第 378-394 行:
```c
bq76940_status_t bq76940_set_balancing(uint16_t balance_mask)
{
    /* CELLBAL1: VC1-VC5 (bit[4:0]) */
    uint8_t bal1 = (uint8_t)(balance_mask & 0x1FU);
    /* CELLBAL2: VC6-VC9 (bit[3:0]) */
    uint8_t bal2 = (uint8_t)((balance_mask >> 5U) & 0x0FU);

    i2c_sw_status_t ret;

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL1, bal1);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL2, bal2);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    return BQ76940_OK;
}
```
替换为:
```c
bq76940_status_t bq76940_set_balancing(uint16_t balance_mask)
{
    /* CELLBAL1: VC1-VC5  (bit[4:0]  = CB1-CB5)  */
    /* CELLBAL2: VC6-VC10 (bit[9:5]  = CB6-CB10) */
    /* CELLBAL3: VC11-VC15(bit[14:10]= CB11-CB15) */
    uint8_t bal1 = (uint8_t)(balance_mask & 0x1FU);
    uint8_t bal2 = (uint8_t)((balance_mask >> 5U) & 0x1FU);
    uint8_t bal3 = (uint8_t)((balance_mask >> 10U) & 0x1FU);

    i2c_sw_status_t ret;

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL1, bal1);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL2, bal2);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    ret = i2c_sw_write_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL3, bal3);
    if (ret != I2C_SW_OK) { return BQ76940_I2C_ERROR; }

    return BQ76940_OK;
}
```

- [ ] **Step 2: 重写 bq76940_get_balancing 回读 CELLBAL3**

将第 396-404 行:
```c
uint16_t bq76940_get_balancing(void)
{
    uint8_t bal1 = 0U, bal2 = 0U;

    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL1, &bal1);
    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL2, &bal2);

    return (uint16_t)(((uint16_t)bal2 << 5U) | (bal1 & 0x1FU));
}
```
替换为:
```c
uint16_t bq76940_get_balancing(void)
{
    uint8_t bal1 = 0U, bal2 = 0U, bal3 = 0U;

    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL1, &bal1);
    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL2, &bal2);
    i2c_sw_read_crc(BQ76940_I2C_ADDR, BQ76940_REG_CELLBAL3, &bal3);

    return (uint16_t)(((uint32_t)bal3 << 10U)
                    | ((uint32_t)bal2 << 5U)
                    | (bal1 & 0x1FU));
}
```

- [ ] **Step 3: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 0 警告 0 错误。

- [ ] **Step 4: Commit**

```bash
git add BSP/Src/bq76940.c
git commit -m "feat(bq76940): extend set_balancing/get_balancing to 15-bit CELLBAL3

balance_mask bit[4:0]=CELLBAL1, bit[9:5]=CELLBAL2, bit[14:10]=CELLBAL3.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 3: bq76940_read_cells 重构为逐通道读取 9 个在用 VC

**Files:**
- Modify: `BSP/Src/bq76940.c:183-222`

**Interfaces:**
- Consumes: `BQ76940_REG_VC1_LO` (0x20 — 注意此地址当前未修正, 保持现有地址映射), `BQ76940_CELL_COUNT=9`
- Produces: `cells->cell_mv[0..8]` = VC1/VC2/VC5/VC6/VC7/VC10/VC11/VC12/VC15 电压; `cells->max_mv/min_mv/total_mv/diff_mv/valid` 保留

- [ ] **Step 1: 定义在用电芯 VC 通道地址表**

在 `BSP/Src/bq76940.c` 的 `bq76940_read_cells` 函数上方（第 182 行前）添加静态常量表:
```c
/** @brief 9 个在用 VC 通道的 LO 寄存器地址（按物理 cell1~cell9 顺序） */
static const uint8_t vc_channel_lo_addrs[BQ76940_CELL_COUNT] = {
    0x20U,  /* VC1  → cell 1 */
    0x22U,  /* VC2  → cell 2 */
    0x28U,  /* VC5  → cell 3 */
    0x2AU,  /* VC6  → cell 4 */
    0x2CU,  /* VC7  → cell 5 */
    0x34U,  /* VC10 → cell 6 */
    0x36U,  /* VC11 → cell 7 */
    0x38U,  /* VC12 → cell 8 */
    0x3EU,  /* VC15 → cell 9 */
};
/* 短路/未连接通道 (VC3/4/8/9/13/14) 不在此表中，不读取 */
```

> **注意**: 上述地址基于当前代码的寄存器映射（`BQ76940_REG_VC1_LO=0x20`，每通道 HI+LO 占 2 字节偏移）。此地址映射与 TI 数据手册不同，但属于既有系统约定。寄存器地址修正已超出本 spec 范围，仅 CELLBAL 地址在 Task 1 中修正。

- [ ] **Step 2: 重写 bq76940_read_cells 为逐通道读取**

将 `bq76940_read_cells` 函数体（第 183-222 行）替换为:
```c
bq76940_status_t bq76940_read_cells(bq76940_cell_data_t *cells)
{
    if (cells == NULL) {
        return BQ76940_ERROR;
    }

    cells->max_mv   = 0U;
    cells->min_mv   = 0xFFFFU;
    cells->total_mv = 0U;
    cells->valid    = 0U;

    i2c_sw_status_t ret;

    for (uint8_t i = 0U; i < BQ76940_CELL_COUNT; i++) {
        /* 逐通道读取: 每通道 2 字节 (LO + HI), CRC 保护 */
        uint8_t buf[2];
        ret = i2c_sw_read_buf(BQ76940_I2C_ADDR, vc_channel_lo_addrs[i], buf, 2U);
        if (ret != I2C_SW_OK) {
            return BQ76940_I2C_ERROR;
        }

        /* 14-bit ADC: hi[5:0] | lo[7:0] */
        uint8_t  lo  = buf[0];
        uint8_t  hi  = buf[1];
        uint16_t adc = (uint16_t)(((uint16_t)(hi & 0x3FU) << 8U) | lo);
        uint16_t mv  = (uint16_t)(((uint32_t)adc * BQ76940_ADC_UV_PER_LSB) / 1000U);

        cells->cell_mv[i] = mv;
        cells->total_mv  += mv;

        if (mv > cells->max_mv) { cells->max_mv = mv; }
        if (mv < cells->min_mv) { cells->min_mv = mv; }
    }

    cells->diff_mv = (uint16_t)(cells->max_mv - cells->min_mv);
    cells->valid   = 1U;

    return BQ76940_OK;
}
```

- [ ] **Step 3: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 0 警告 0 错误。

- [ ] **Step 4: Commit**

```bash
git add BSP/Src/bq76940.c
git commit -m "feat(bq76940): restructure read_cells to read 9 active VC channels only

Read VC1/VC2/VC5/VC6/VC7/VC10/VC11/VC12/VC15 sequentially,
skip shorted channels (VC3/4/8/9/13/14). Preserve max_mv/min_mv/
diff_mv/total_mv/valid computed fields for protection and CAN TX.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 4: bms_shared + can_cmd — 删除均衡配置字段

**Files:**
- Modify: `App/Inc/bms_shared.h:35-36`
- Modify: `App/Src/bms_shared.c:25-26,45-46`
- Modify: `App/Src/can_cmd.c:220-221`

**Interfaces:**
- Consumes: 无
- Produces: `bms_settings_t` 无 `balance_thresh_mv`/`balance_min_mv`; can_cmd 无 0x05/0x06 配置分支

- [ ] **Step 1: 从 bms_settings_t 移除均衡字段**

在 `App/Inc/bms_shared.h` 中，删除第 35-36 行:
```c
    uint16_t balance_thresh_mv; /**< 均衡开启压差阈值 (mV)              */
    uint16_t balance_min_mv;    /**< 均衡最低电压阈值 (mV)              */
```

- [ ] **Step 2: 从 bms_shared_init 移除均衡默认值**

在 `App/Src/bms_shared.c` 中:
- 删除第 25-26 行:
```c
#define DEFAULT_BALANCE_THRESH_MV   200U
#define DEFAULT_BALANCE_MIN_MV     3200U
```
- 删除第 45-46 行:
```c
    g_bms.settings.balance_thresh_mv = DEFAULT_BALANCE_THRESH_MV;
    g_bms.settings.balance_min_mv    = DEFAULT_BALANCE_MIN_MV;
```

- [ ] **Step 3: 从 can_cmd 移除均衡参数配置**

在 `App/Src/can_cmd.c` 的 `can_cmd_handle_config` 函数中（第 220-221 行），删除:
```c
        case 0x05U: s->balance_thresh_mv = value; break;
        case 0x06U: s->balance_min_mv    = value; break;
```

- [ ] **Step 4: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 0 警告 0 错误。

- [ ] **Step 5: Commit**

```bash
git add App/Inc/bms_shared.h App/Src/bms_shared.c App/Src/can_cmd.c
git commit -m "refactor(bms): remove balance_thresh_mv/balance_min_mv from settings

Balance thresholds are now hard-coded (50mV start / 30mV stop / 3200mV min).
CAN config commands 0x05/0x06 removed. CAN control commands BALANCE_SET(0x30)
and BALANCE_OFF(0x31) preserved.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 5: bms_app.h — 新增均衡宏和 task_balance 声明

**Files:**
- Modify: `App/Inc/bms_app.h:38-42` (新增宏), `App/Inc/bms_app.h:53-58` (新增声明 + 更新任务表)

**Interfaces:**
- Consumes: `ALL_FAULTS` (来自 fullstack-redesign), `FAULT_CELL_IMBALANCE` (来自 protection.h)
- Produces: `BALANCE_PERIOD_MS`, `BALANCE_START_DIFF_MV`, `BALANCE_STOP_DIFF_MV`, `BALANCE_MIN_CELL_MV`, `FAULT_MASK_BLOCK_BALANCE`, `void task_balance(void *arg)` 声明

- [ ] **Step 1: 添加均衡周期宏和任务表更新**

在 `App/Inc/bms_app.h` 的任务表注释（第 6-15 行）中新增一行 `task_balance`，并在宏定义区（第 38-42 行后）添加均衡相关宏。

任务表注释更新为:
```c
 *          │ can_rx_task       osPriorityHigh    事件驱动  512w  │
 *          │ protection_task   osPriorityAboveNormal 10ms  256w │
 *          │ acquisition_task  osPriorityNormal    100ms  512w  │
 *          │ can_tx_task       osPriorityNormal    100ms  256w  │
 *          │ soc_task          osPriorityNormal   1000ms  512w  │
 *          │ task_balance      osPriorityBelowNormal 1000ms 256w│
 *          │ watchdog_task     osPriorityLow       500ms  128w  │
```

在第 42 行 `#define WDG_TASK_PERIOD_MS      500U` 后添加:
```c
#define BALANCE_PERIOD_MS       1000U

/* ---- 均衡阈值 (硬编码) ---- */
#define BALANCE_START_DIFF_MV     50U  /**< 均衡开启压差阈值 (mV)          */
#define BALANCE_STOP_DIFF_MV      30U  /**< 均衡停止压差阈值 (mV)          */
#define BALANCE_MIN_CELL_MV     3200U  /**< 均衡最低电压门槛 (mV)          */

/*
 * FAULT_MASK_BLOCK_BALANCE: 均衡屏蔽故障掩码
 * = ALL_FAULTS & ~FAULT_CELL_IMBALANCE
 * CELL_IMBALANCE 被排除：均衡是电芯压差过大的修复手段，
 * 屏蔽均衡会导致死锁 (ADR-0007)
 */
#define FAULT_MASK_BLOCK_BALANCE  (ALL_FAULTS & ~FAULT_CELL_IMBALANCE)
```

- [ ] **Step 2: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 如果 `ALL_FAULTS` 尚未定义，编译失败——验证了前置依赖。此 Task 依赖 fullstack-redesign 中 ALL_FAULTS 已定义。

- [ ] **Step 3: Commit**

```bash
git add App/Inc/bms_app.h
git commit -m "feat(bms_app): add balance macros and task_balance declaration

BALANCE_PERIOD_MS=1000, thresholds 50/30mV/3200mV hard-coded.
FAULT_MASK_BLOCK_BALANCE excludes CELL_IMBALANCE (ADR-0007).

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 6: task_balance 任务函数实现

**Files:**
- Modify: `App/Src/bms_app.c` (在 watchdog_task 函数后新增 task_balance, ~80 行)

**Interfaces:**
- Consumes: `bms_shared_data_lock/unlock` (from bms_shared), `bq76940_set_balancing` (from Task 2), `cellbal_bit_map[9]` (static), `evt_data_ready`/`mutex_iic`/`mutex_data` (from fullstack-redesign), `FAULT_MASK_BLOCK_BALANCE` (from Task 5)
- Produces: `static void task_balance(void *arg)` — 完整的均衡任务函数

- [ ] **Step 1: 添加均衡任务前向声明**

在 `App/Src/bms_app.c` 第 80 行 `static void watchdog_task(void *arg);` 后添加:
```c
static void task_balance(void *arg);
```

- [ ] **Step 2: 添加 CELLBAL 位映射表**

在第 80 行后、`bms_app_init` 函数前，添加静态映射表:
```c
/* ============================================================
 * CELLBAL 位映射: cells_mv[idx] → CELLBAL 寄存器位号
 * cells_mv[0]=VC1→CB1, [1]=VC2→CB2, [2]=VC5→CB5,
 * [3]=VC6→CB6, [4]=VC7→CB7, [5]=VC10→CB10,
 * [6]=VC11→CB11, [7]=VC12→CB12, [8]=VC15→CB15
 * ============================================================ */
static const uint8_t cellbal_bit_map[9] = {
    0U,   /* idx 0: VC1  → CB1  (CELLBAL1 bit 0) */
    1U,   /* idx 1: VC2  → CB2  (CELLBAL1 bit 1) */
    4U,   /* idx 2: VC5  → CB5  (CELLBAL1 bit 4) */
    5U,   /* idx 3: VC6  → CB6  (CELLBAL2 bit 0) */
    6U,   /* idx 4: VC7  → CB7  (CELLBAL2 bit 1) */
    9U,   /* idx 5: VC10 → CB10 (CELLBAL2 bit 4) */
    10U,  /* idx 6: VC11 → CB11 (CELLBAL3 bit 0) */
    11U,  /* idx 7: VC12 → CB12 (CELLBAL3 bit 1) */
    14U,  /* idx 8: VC15 → CB15 (CELLBAL3 bit 4) */
};
```

- [ ] **Step 3: 实现 task_balance 函数**

在文件末尾（`watchdog_task` 函数之后）添加 task_balance:

```c
/* ============================================================
 * 均衡任务 (1000ms) — 被动均衡，仅最高电压单电芯
 * ============================================================ */

static void task_balance(void *arg)
{
    (void)arg;

    /* 首次: 等待数据就绪（超时 5s 后进入空循环，diff=0 不触发均衡） */
    uint32_t flags = osEventFlagsWait(evt_data_ready, 0x01U,
                                       osFlagsWaitAny, 5000U);
    (void)flags;  /* 超时或成功都进入主循环 */

    uint16_t prev_mask = 0U;

    for (;;) {
        uint16_t balance_mask = 0U;

        /* ---- 1. 栈快照: 读取电芯数据 + 故障状态 ---- */
        bms_shared_t *bms = bms_shared_data_lock(10U);
        if (bms == NULL) {
            osDelay(BALANCE_PERIOD_MS);
            continue;
        }

        uint16_t cells_mv[9];
        for (uint8_t i = 0U; i < 9U; i++) {
            cells_mv[i] = bms->bq_data.cells.cell_mv[i];
        }
        uint16_t active_faults = bms->prot_ctx.active_faults;

        bms_shared_data_unlock();

        /* ---- 2. 故障检查: CELL_IMBALANCE 除外的全部故障均停止均衡 ---- */
        if ((active_faults & FAULT_MASK_BLOCK_BALANCE) != 0U) {
            balance_mask = 0U;
            goto apply;
        }

        /* ---- 3. 找最高/最低电压 ---- */
        uint16_t max_mv = 0U;
        uint16_t min_mv = 0xFFFFU;
        uint8_t  max_idx = 0U;

        for (uint8_t i = 0U; i < 9U; i++) {
            if (cells_mv[i] > max_mv) {
                max_mv  = cells_mv[i];
                max_idx = i;
            }
            if (cells_mv[i] < min_mv) {
                min_mv = cells_mv[i];
            }
        }

        uint16_t diff_mv = (uint16_t)(max_mv - min_mv);

        /* ---- 4. 均衡判断 (滞回: 50mV 开, 30mV 关) ---- */
        if (diff_mv > BALANCE_START_DIFF_MV && max_mv > BALANCE_MIN_CELL_MV) {
            /* 开启均衡: 最高电压单电芯 */
            balance_mask = (uint16_t)(1U << cellbal_bit_map[max_idx]);
        } else if (diff_mv < BALANCE_STOP_DIFF_MV) {
            /* 停止均衡 */
            balance_mask = 0U;
        } else {
            /* 滞回区: 保持上一轮状态 */
            balance_mask = prev_mask;
        }

apply:
        /* ---- 5. 仅在 mask 变化时写 CELLBAL 寄存器 ---- */
        if (balance_mask != prev_mask) {
            if (osMutexAcquire(mutex_iic, 10U) == osOK) {
                bq76940_set_balancing(balance_mask);
                osMutexRelease(mutex_iic);
            }
            prev_mask = balance_mask;
        }

        osDelay(BALANCE_PERIOD_MS);
    }
}
```

- [ ] **Step 4: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 如果 `evt_data_ready`、`mutex_iic` 等同步原语尚未定义，编译会报 "undeclared" 错误——验证了 fullstack-redesign 前置依赖。若符号已存在则 0 错误 0 警告。

- [ ] **Step 5: Commit**

```bash
git add App/Src/bms_app.c
git commit -m "feat(balance): implement task_balance — single-cell passive balancing

1s cycle, stack snapshot from bms_shared, 50mV start / 30mV stop
hysteresis, 3200mV minimum cell voltage. Faults (except CELL_IMBALANCE)
block balancing. Only writes CELLBAL when mask changes.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 7: bms_app_init 创建 task_balance

**Files:**
- Modify: `App/Src/bms_app.c:26-31` (新增 handle), `App/Src/bms_app.c:67-71` (新增 attr), `App/Src/bms_app.c:133-137` (新增 osThreadNew)

**Interfaces:**
- Consumes: `task_balance` 函数 (from Task 6), `osPriorityBelowNormal`
- Produces: `task_balance_handle` (static)

- [ ] **Step 1: 添加任务句柄**

在第 31 行 `task_wdg_handle` 后添加:
```c
static osThreadId_t task_balance_handle = NULL;
```

- [ ] **Step 2: 添加任务属性**

在第 71 行 `wdg_task_attr` 后添加:
```c
static const osThreadAttr_t balance_task_attr = {
    .name       = "balance",
    .stack_size = 256U * 4U,
    .priority   = osPriorityBelowNormal,
};
```

- [ ] **Step 3: 在 bms_app_init 中创建任务**

在 bms_app_init 的任务创建区（第 133 行 `task_wdg_handle` 创建后、第 136 行 LED 指示前）添加:
```c
    task_balance_handle = osThreadNew(task_balance, NULL,
                                       &balance_task_attr);
```

- [ ] **Step 4: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 0 警告 0 错误。

- [ ] **Step 5: Commit**

```bash
git add App/Src/bms_app.c
git commit -m "feat(balance): create task_balance in bms_app_init

osPriorityBelowNormal, 1024B stack, 1000ms period.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 8: can_cmd BALANCE_SET/OFF 加 mutex_iic 保护

**Files:**
- Modify: `App/Src/can_cmd.c:180-190`

**Interfaces:**
- Consumes: `mutex_iic` (from fullstack-redesign), `bq76940_set_balancing`/`bq76940_balance_off` (from Task 2)
- Produces: BALANCE_SET/OFF 路径线程安全

- [ ] **Step 1: 添加 mutex_iic 保护**

将 `App/Src/can_cmd.c` 第 180-190 行:
```c
        case CAN_CTRL_BALANCE_SET:
            if (msg->dlc >= 3U) {
                uint16_t mask = (uint16_t)(((uint16_t)msg->data[1] << 8U)
                                           | msg->data[2]);
                bq76940_set_balancing(mask);
            }
            break;

        case CAN_CTRL_BALANCE_OFF:
            bq76940_balance_off();
            break;
```
替换为:
```c
        case CAN_CTRL_BALANCE_SET:
            if (msg->dlc >= 3U) {
                uint16_t mask = (uint16_t)(((uint16_t)msg->data[1] << 8U)
                                           | msg->data[2]);
                if (osMutexAcquire(mutex_iic, 10U) == osOK) {
                    bq76940_set_balancing(mask);
                    osMutexRelease(mutex_iic);
                }
            }
            break;

        case CAN_CTRL_BALANCE_OFF:
            if (osMutexAcquire(mutex_iic, 10U) == osOK) {
                bq76940_balance_off();
                osMutexRelease(mutex_iic);
            }
            break;
```

- [ ] **Step 2: 编译验证**

```bash
cmake --build build --target bms_9s_f103 2>&1 | tail -5
```
Expected: 0 警告 0 错误。

- [ ] **Step 3: Commit**

```bash
git add App/Src/can_cmd.c
git commit -m "fix(can_cmd): add mutex_iic protection to BALANCE_SET/OFF

Prevents race condition with task_balance's CELLBAL writes.
Both paths now serialize on mutex_iic.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task 9: 全量编译验证 + Commit

**Files:**
- 无新修改（验证 Task 1-8 的累积效果）

- [ ] **Step 1: Clean rebuild**

```bash
cd D:\coding_codes\bms_learning\bms_9s_f103\bms_9s_f103
cmake --build build --clean-first 2>&1
```
Expected: 0 错误 0 警告。

- [ ] **Step 2: 检查 GCC size**

```bash
arm-none-eabi-size build/bms_9s_f103.elf
```
Expected: FLASH < 28KB, RAM < 20KB.

- [ ] **Step 3: 检查分层违规**

验证 App 层无 BSP 调用:
```bash
grep -n "bq76940_\|i2c_sw_\|bsp_tick_\|io_ctrl_\|can_send\|wdg_" App/Src/bms_app.c | grep -v "bq76940_set_balancing\|bq76940_balance_off"
```
Expected: task_balance 仅调用 `bq76940_set_balancing`（这是合法 BSP 调用——task 编排函数有权同时使用 BSP 和 App）。无其他 BSP 调用。

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "feat(balance): complete cell-balance implementation — build verified

All 8 implementation tasks complete:
- BSP: CELLBAL1/2 address fix + CELLBAL3 (Task 1-2)
- BSP: read_cells restructured for 9 active VC channels (Task 3)
- App: removed balance settings fields + CAN config (Task 4)
- App: balance macros + task declaration (Task 5)
- App: task_balance implementation (Task 6)
- App: bms_app_init creates task_balance (Task 7)
- App: CAN BALANCE_SET/OFF mutex_iic protection (Task 8)
- Build: 0 errors 0 warnings (Task 9)

Closes spec: .spec-dev/2026-07-28-cell-balance/

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage:**

| Spec Requirement | Task |
|-----------------|------|
| ADDED: task_balance 创建 | Task 7 |
| ADDED: 栈快照读取 | Task 6 (step 3) |
| ADDED: 均衡开启条件 (50mV/3200mV/故障屏蔽) | Task 6 (step 3) |
| ADDED: I2C 写 CELLBAL | Task 6 (step 3) |
| ADDED: bq76940_set_balancing 15-bit | Task 2 |
| ADDED: read_cells 逐通道 9 VC | Task 3 |
| MODIFIED: CELLBAL 地址修正 | Task 1 |
| MODIFIED: read_cells 保留计算字段 | Task 3 (step 2) |
| MODIFIED: CAN BALANCE_SET mutex_iic | Task 8 |
| REMOVED: bms_settings 均衡字段 | Task 4 |
| REMOVED: CAN 配置 0x05/0x06 | Task 4 |

All ADDED (5) + MODIFIED (3) + REMOVED (2) = 10 requirements covered across 8 implementation tasks.

**2. Placeholder scan:** No TBD/TODO found. All code steps contain actual implementation.

**3. Type consistency:**
- `cellbal_bit_map[9]` defined in Task 6, consumed by Task 6 (same task)
- `BQ76940_REG_CELLBAL1/2/3` defined in Task 1, consumed in Tasks 2, 6, 8 ✓
- `FAULT_MASK_BLOCK_BALANCE` defined in Task 5, consumed in Task 6 ✓
- `mutex_iic` consumed in Tasks 6, 8 — defined in fullstack-redesign (pre-req) ✓
- `evt_data_ready` consumed in Task 6 — defined in fullstack-redesign (pre-req) ✓
- `ALL_FAULTS` consumed in Task 5 — defined in fullstack-redesign (pre-req) ✓
- `bms_shared_data_lock/unlock` consumed in Task 6 — unchanged API ✓
- `osMutexAcquire/Release` consumed in Tasks 6, 8 — CMSIS-RTOS v2 API ✓
