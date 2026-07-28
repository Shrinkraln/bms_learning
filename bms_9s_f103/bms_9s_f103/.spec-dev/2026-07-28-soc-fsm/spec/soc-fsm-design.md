---
# —— spec-dev 漂移守卫锚点（机器可校验，勿删）——
spec_dev:
  version: 1
  feature: soc-fsm
  status: draft
  covers:
    - "App/Inc/soc_ocv.h"
    - "App/Src/soc_ocv.c"
    - "App/Src/bms_app.c"
  sync_commit: null
---

# SOC 状态机重构 — 充放电检测 + Q_max 自学习 + 安全余量映射

## 背景与目标

当前 `soc_ocv.c` 使用"增强型安时积分 + OCV 动态校正"算法——所有时刻都在做安时积分，通过静置检测动态调整 OCV 校正权重。这种无状态方法在持续充放电场景下精度尚可，但无法区分充/放/静置的工作模式，OCV 校正窗口不可控，且无 Q_max 老化学习能力。

本次重构将 SOC 模块改为**四状态机驱动**（IDLE → DISCHARGE/CHARGE → POLARIZATION → IDLE），在充放电期间做安时积分，结束后经极化消除后用 OCV 精确修正 SOC，支持 Q_max 自学习和 SOC 安全余量映射。

**成功标准**：
- 状态机正确识别充/放/极化/空闲四种状态
- 放电结束后 30min 极化消除，OCV 修正 SOC 误差 < ±3%
- Q_max 在深度放电（>50% Q_max）后自动学习更新
- 显示 SOC = (真实 SOC - 30‰) × 1000 / 970，底部保留 3% 安全余量
- 编译 0 警告 0 错误

## 非目标

- 电池内阻 (IR) 补偿或等效电路模型 (ECM)
- 温度对 OCV-SOC 曲线的修正（保持 25°C 单温度点查表）
- 充电状态下的 CC/CV 阶段细分
- Q_max 的非易失存储（掉电丢失，每次上电从默认值开始学习）

## 术语表

| 术语 | 定义 | Avoid |
|------|------|-------|
| 极化消除 | 充放电停止后电池内部 Li+ 浓度梯度逐渐消失、端电压趋近 OCV 的过程 | relaxation、去极化 |
| Q_max | 电池当前实际满容量 (mAh)，随老化衰减 | SOH、满容量、capacity |
| Q_passed | 一次充放周期内累计通过的电量 (mAh) | 累计电量、积分电量 |
| SOC 显示值 | 映射到 [0,1000] 的用户可见 SOC，底部 3% 隐藏 | display_soc、用户 SOC |
| SOC 真实值 | 安时积分直接计算的 SOC (0-1000)，未经映射 | real_soc、实际 SOC |
| IDLE 基准电压 | POLARIZATION→IDLE 转换时记录的最低电芯电压，用于充电检测。仅在状态转换时设置一次，不在 IDLE 循环中更新 | baseline、参考电压 |

## 影响面

| 模块 | 影响 | 改动量 |
|------|------|--------|
| `soc_ocv.h` | 重写：`soc_ctx_t` 扩展为状态机上下文 + `soc_state_t` 枚举 + API 增补 | ~60 行 |
| `soc_ocv.c` | 重写：状态机全部逻辑 + Q_max 学习 + SOC 映射 + 保留 OCV 查表 | ~220 行 |
| `bms_app.c` | `soc_task` 调用方式不变（接口兼容），仅适配新 API | ~5 行 |

## 已确认的关键决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 状态机结构 | 四状态: IDLE → DISCHARGE/CHARGE → POLARIZATION → IDLE | IDLE 提供明确的 OCV 校正窗口；充/放分别检测；极化消除统一等待 |
| 放电检测 | `current_ma < -500mA`（负=放电） | 电流符号与现有代码约定一致（正=充电，负=放电） |
| 充电检测 | `cell_min > idle_baseline + 100mV`（放电条件不满足时） | 电压上升是充电的可靠标志，不受 CC 电流大小影响 |
| 极化消除时间 | 统一 30 分钟 | 18650 NMC 圆柱电芯极化时间常数 ~10-20min，30min 确保 95%+ 消除 |
| Q_max 学习 | EMA 平滑: `Q_max += (Q_new − Q_max) / 10` | α=0.1，约 10 次循环收敛，滤除单次异常波动 |
| SOC 映射 | `display = (real − 30) × 1000 / 970, clamp[0,1000]` | 底部 3% 不可见，顶部无截断（100% = 100%） |
| 充电检测基准 | 进入 IDLE 时的最低电芯电压 | 每次极化消除后刷新，适应不同充放电深度 |

## ADDED Requirements

### Requirement: soc_ocv_update SHALL 实现四状态机

`soc_ocv_update()` SHALL 内部维护四状态机，按以下规则转换：

| 当前状态 | 目标状态 | 条件 |
|---------|---------|------|
| IDLE | DISCHARGE | `current_ma < -500mA` |
| IDLE | CHARGE | `cell_min > idle_baseline + 100mV` 且放电条件不满足 |
| DISCHARGE | POLARIZATION | `\|current_ma\| < 300mA` 或 `cell_min < 3000mV` |
| DISCHARGE | CHARGE | `current_ma > +200mA` 持续 10 秒 (充电器接入) |
| CHARGE | POLARIZATION | 过去 5 分钟内 `max(cell_min) - min(cell_min) < 5mV` (电压平台) |
| CHARGE | DISCHARGE | `current_ma < -500mA` (负载接入) |
| POLARIZATION | DISCHARGE | `current_ma < -500mA` (中断极化) |
| POLARIZATION | CHARGE | `cell_min > 进入极化时的电压 + 100mV` (中断极化) |
| POLARIZATION | IDLE | 极化计时器 ≥ 30 分钟且无充/放条件 |

#### Scenario: 上电后进入 IDLE

- **GIVEN** 首次调用 `soc_ocv_update`, `ctx.initialized == 0`
- **WHEN** 函数执行
- **THEN** state = IDLE, 用 OCV 查表初始化 SOC, `idle_baseline = cell_min`, `initialized = 1`

#### Scenario: 强放电电流触发 DISCHARGE 转换

- **GIVEN** state = IDLE, `current_ma = -800mA`
- **WHEN** 状态机检查转换条件
- **THEN** state → DISCHARGE, 记录 `soc_at_entry`, 清零 `q_passed`

#### Scenario: 电压上升触发 CHARGE 转换

- **GIVEN** state = IDLE, `current_ma = +200mA`, `cell_min = 3650mV`, `idle_baseline = 3520mV`
- **WHEN** 状态机检查转换条件 (放电不满足, 3650-3520=130 > 100)
- **THEN** state → CHARGE, 记录 `soc_at_entry`, 清零 `q_passed`

#### Scenario: 放电电流降至阈值以下 → POLARIZATION

- **GIVEN** state = DISCHARGE, `current_ma = -200mA` (|I|=200 < 300)
- **WHEN** 状态机检查结束条件
- **THEN** state → POLARIZATION, 记录 `last_soc_end = soc_real`, `last_q_passed = q_passed`, 清零极化计时器

#### Scenario: 最低电芯电压低于安全值 → POLARIZATION

- **GIVEN** state = DISCHARGE, `cell_min = 2950mV` (< 3000mV)
- **WHEN** 状态机检查结束条件
- **THEN** state → POLARIZATION（即使电流仍 > 300mA，安全优先）

#### Scenario: 充电电压平台 → POLARIZATION

- **GIVEN** state = CHARGE, 过去 5 分钟内 Δcell_min = 3mV (< 5mV)
- **WHEN** 状态机检查结束条件
- **THEN** state → POLARIZATION

#### Scenario: 极化 30 分钟完成 → IDLE + OCV 修正

- **GIVEN** state = POLARIZATION, 极化计时器 = 30 分钟
- **WHEN** 状态机检查
- **THEN** state → IDLE, 用 `cell_min` OCV 查表更新 `soc_real` 和 `remaining_mah`, 设置 `idle_baseline = cell_min`, 触发 Q_max 学习检查

#### Scenario: 极化中被强放电打断 → DISCHARGE

- **GIVEN** state = POLARIZATION, 极化计时器 = 5 分钟, `current_ma = -800mA`
- **WHEN** 状态机检查转换条件 (中断优先于计时)
- **THEN** state → DISCHARGE（放弃本次 OCV 修正，优先跟踪放电）

#### Scenario: DISCHARGE 中充电器接入 → CHARGE

- **GIVEN** state = DISCHARGE, `current_ma = +3000mA` 持续 10 秒
- **WHEN** 状态机检测到正电流 > +200mA 达 10 秒
- **THEN** state → CHARGE, `soc_at_entry = soc_real`, `q_passed = 0`

### Requirement: 充放电状态 SHALL 执行安时积分

DISCHARGE 和 CHARGE 状态下，`soc_ocv_update()` SHALL 每周期（~1s）执行安时积分。中间量 SHALL 使用 `int64_t` 避免 <3600mA 时整数截断为 0（与现有 `soc_ocv.c:269` 一致）：`delta_mah = (int64_t)I × (int64_t)dt_ms / 3600000L`。`q_passed += \|delta_mah\|`，`remaining_mah += delta_mah`（充电为正、放电为负），`soc_real = remaining_mah × 1000 / q_max`。

#### Scenario: 放电积分

- **GIVEN** state = DISCHARGE, `current_ma = -2000mA`, `dt_ms = 1000`, `q_max = 20000mAh`
- **WHEN** 执行安时积分
- **THEN** `q_passed += 2000 × 1000 / 3600000 = 0.555mAh`, `remaining_mah -= 0.555mAh`, `soc_real` 按比例下降

#### Scenario: 充电积分

- **GIVEN** state = CHARGE, `current_ma = +1500mA`, `dt_ms = 1000`, `remaining_mah = 15000`
- **WHEN** 执行安时积分
- **THEN** `q_passed += 0.416mAh`, `remaining_mah += 0.416mAh`, `soc_real` 按比例上升

### Requirement: Q_max SHALL 在深度放电后通过 EMA 自学习

系统 SHALL 仅在 DISCHARGE→POLARIZATION→IDLE 完整周期（即 `soc_at_entry > last_soc_end`，SOC 下降）且 `q_passed > q_max / 2`（放电量超过当前 Q_max 一半）时触发 Q_max 学习。学习公式：`Q_new = q_passed × 1000 / (soc_at_entry − last_soc_end)`。SHALL 先检查分母 > 0 再执行除法（防止除零 HardFault）。更新公式：`q_max += (Q_new − q_max) / 10`。充电周期不触发 Q_max 学习（`soc_at_entry < last_soc_end` → 分母为负，跳过）。

#### Scenario: 有效深度放电 → Q_max 更新

- **GIVEN** `q_max = 20000mAh`, `q_passed = 12000mAh` (>10000), `soc_at_entry = 800`, `last_soc_end = 200`
- **WHEN** POLARIZATION → IDLE 转换
- **THEN** `Q_new = 12000 × 1000 / 600 = 20000mAh`, `q_max = 20000 + (20000-20000)/10 = 20000` (无变化)

#### Scenario: 容量衰减 → Q_max 下调

- **GIVEN** `q_max = 20000`, `q_passed = 11000`, `soc_at_entry = 800`, `last_soc_end = 150`
- **WHEN** 计算 Q_new
- **THEN** `Q_new = 11000 × 1000 / 650 = 16923mAh`, `q_max = 20000 + (16923-20000)/10 = 19692mAh` (下调 ~1.5%)

#### Scenario: 放电不足一半 → 不学习

- **GIVEN** `q_passed = 8000mAh`, `q_max = 20000` (8000 < 10000)
- **WHEN** 检查学习条件
- **THEN** 跳过 Q_max 更新，保持当前值

### Requirement: SOC 显示值 SHALL 通过安全余量映射

`soc_ocv_get_soc()` SHALL 返回映射后的显示 SOC：`display = (real − 30) × 1000 / 970`，clamp 到 [0, 1000]。真实 SOC 通过 `soc_ocv_get_real_soc()` 获取。映射公式 SHALL 使用整数运算，避免浮点。

#### Scenario: 真实 SOC=30‰ → 显示 0

- **GIVEN** `soc_real = 30`
- **WHEN** 计算显示 SOC
- **THEN** `display = (30-30) × 1000 / 970 = 0`

#### Scenario: 真实 SOC=1000‰ → 显示 1000

- **GIVEN** `soc_real = 1000`
- **WHEN** 计算显示 SOC
- **THEN** `display = (1000-30) × 1000 / 970 = 970000/970 = 1000`

#### Scenario: 真实 SOC=515‰ → 显示 500

- **GIVEN** `soc_real = 515`
- **WHEN** 计算显示 SOC
- **THEN** `display = (515-30) × 1000 / 970 = 485000/970 = 500`

#### Scenario: 真实 SOC < 30‰ → clamp 到 0

- **GIVEN** `soc_real = 20`
- **WHEN** 计算显示 SOC
- **THEN** `display = (20-30) × 1000 / 970 = -10 → clamp → 0`

### Requirement: IDLE 状态下 SHALL 用小电流 OCV 更新 SOC

IDLE 状态下，当 `|current_ma| < 200mA` 时，系统 SHALL 用 `cell_min` 经 OCV 查表更新 `soc_real` 和 `remaining_mah`。大于 200mA 时跳过 OCV 更新（IR drop 不可忽略）。

#### Scenario: 静置时 OCV 校准

- **GIVEN** state = IDLE, `|current_ma| = 50mA` (< 200mA), OCV 查表得 SOC=650
- **WHEN** IDLE 态逻辑执行
- **THEN** `soc_real = 650`, `remaining_mah = q_max × 650 / 1000`

#### Scenario: 带载时不校准

- **GIVEN** state = IDLE, `|current_ma| = 400mA` (≥ 200mA)
- **WHEN** IDLE 态逻辑执行
- **THEN** 跳过 OCV 更新，保持当前 `soc_real`

## MODIFIED Requirements

### MODIFIED: soc_ocv_init 默认标称容量

原 `soc_ocv_init(int32_t nominal_mah)` 中的 `nominal_mah` SHALL 被用作初始 `q_max`（Q_max 学习起点）。0 仍表示使用默认 20000mAh。所有状态机变量（state, idle_baseline, polarization_ms 等）SHALL 在 init 中清零。

#### Scenario: 使用默认容量

- **GIVEN** `soc_ocv_init(0)` 被调用
- **WHEN** 函数执行
- **THEN** `q_max = 20000mAh`, state = IDLE, 所有计时器和累计量清零

### MODIFIED: soc_ocv_get_soc 语义变更

原 `soc_ocv_get_soc()` 返回真实 SOC (0-1000)。修改后 SHALL 返回映射后的显示 SOC（底部 3% 安全余量）。新增 `soc_ocv_get_real_soc()` 返回真实 SOC。

#### Scenario: 旧调用者兼容

- **GIVEN** `bms_app.c::soc_task` 调用 `soc_ocv_get_soc()` 写入 `bms_shared_update_soc`
- **WHEN** 函数返回映射后 SOC
- **THEN** CAN 上报和外部显示使用带安全余量的 SOC 值（底部 3% 隐藏）

### MODIFIED: soc_ctx_t 数据结构扩展

原 `soc_ocv_ctx_t` (9 字段, 32B) SHALL 被扩展为包含状态机上下文（新增 state, idle_baseline_mv, soc_at_entry, q_max_mah, q_passed_mah, polarization_ms, last_soc_start/end, last_q_passed）。扩展后总大小 SHALL ≤ 64B。

#### Scenario: 内存占用可接受

- **GIVEN** 原 ctx 32B, 新增字段 ~28B
- **WHEN** 编译
- **THEN** 总 RAM 占用 ≤ 64B（相比 20KB SRAM 可忽略）

## REMOVED Requirements

### REMOVED: 动态 OCV 校正权重 + 静置计时器 + 零漂 EMA

原 `soc_ocv_update` 中的以下子算法 SHALL 被移除：
- `calc_correction_weight()` — 动态权重计算
- 静置计时器状态机 (`rest_timer_ms`, `rest_entry_cnt`)
- 电流零漂 EMA 估计 (`current_offset_ma`, `rest_exit_ms`)
- 温度补偿容量系数 (`temp_to_cap_coeff()`)

移除理由：状态机替代了静置检测（POLARIZATION = 强制静置窗口）；OCV 校正在 POLARIZATION→IDLE 时一次性执行；零漂估计的功能由 OCV 周期性修正覆盖。

### REMOVED: soc_ocv_update 的 temp_mdeg 参数

原签名 `soc_ocv_update(cell_min_mv, current_ma, temp_mdeg, dt_ms)` 中的 `temp_mdeg` 参数 SHALL 被移除。温度补偿容量不再需要——Q_max 自学习直接跟踪实际容量，含温度影响。

移除理由：Q_max 自学习自动覆盖温度对容量的影响（冬季容量低 → Q_max 自然学低），无需独立的温度系数查表。

---

## 方案设计

### 架构与组件

```
App/Layer:
  soc_ocv.c — 纯算法 (零 BSP 依赖, 零 RTOS 依赖)
    ├─ soc_ocv_update()     状态机调度 + 安时积分 + OCV 查表
    ├─ soc_ocv_get_soc()    映射后显示 SOC [0,1000]
    ├─ soc_ocv_get_real_soc() 真实 SOC [0,1000]
    ├─ soc_ocv_get_q_max()  当前学习容量 (调试)
    └─ 保留: soc_ocv_lookup() OCV→SOC 21 点查表

  bms_app.c::soc_task — 编排层
    锁定→读 cell_min/current → soc_ocv_update → 写回
```

### 状态机数据流

```
soc_ocv_update(cell_min, current, dt_ms)  [1s 周期]

  switch(state):

  case IDLE:
    if current < -500mA:
      → DISCHARGE, soc_at_entry = soc_real, q_passed = 0
    elif cell_min > idle_baseline + 100mV:
      → CHARGE,    soc_at_entry = soc_real, q_passed = 0
    elif |current| < 200mA:
      soc_real = soc_ocv_lookup(cell_min)  // OCV 静置更新
      remaining_mah = q_max * (int64_t)soc_real / 1000L
    // idle_baseline 不在此更新——仅在 POLARIZATION→IDLE 时设置

  case DISCHARGE:
    delta_mah = (int64_t)current * (int64_t)dt_ms / 3600000L  // 使用 int64_t
    q_passed      += (delta_mah < 0) ? (-delta_mah) : delta_mah  // |delta|
    remaining_mah += delta_mah
    soc_real       = (uint16_t)(remaining_mah * 1000L / q_max)
    // 交叉检测: 充电器突然接入
    if current > +200mA 持续 10 秒:
      → CHARGE, soc_at_entry = soc_real, q_passed = 0
    elif |current| < 300mA || cell_min < 3000mV:
      → POLARIZATION, 记录 last_soc_end, last_q_passed, 计时器=0
      from_discharge = true  // 标记为放电周期 (仅放电可触发 Q_max 学习)

  case CHARGE:
    delta_mah = (int64_t)current * (int64_t)dt_ms / 3600000L
    q_passed      += (delta_mah < 0) ? (-delta_mah) : delta_mah
    remaining_mah += delta_mah
    soc_real       = (uint16_t)(remaining_mah * 1000L / q_max)
    // 充电平台检测: 5min 滑动窗口, Δcell_min < 5mV
    更新 charge_peak_mv / charge_valley_mv (5min 窗口)
    // 交叉检测: 负载突然接入
    if current < -500mA:
      → DISCHARGE, soc_at_entry = soc_real, q_passed = 0
    elif 窗口满 5min 且 (charge_peak_mv - charge_valley_mv) < 5mV:
      → POLARIZATION, 记录 last_soc_end, last_q_passed, 计时器=0
      from_discharge = false  // 充电周期, 不触发 Q_max 学习

  case POLARIZATION:
    // 优先检查充/放中断条件 (避免 30min 盲区)
    if current < -500mA:
      → DISCHARGE, soc_at_entry = soc_real, q_passed = 0
    elif cell_min > pol_entry_mv + 100mV:  // pol_entry_mv = 进入极化时的电压
      → CHARGE, soc_at_entry = soc_real, q_passed = 0
    else:
      polarization_ms += dt_ms
      if polarization_ms >= 30min:
        // OCV 修正
        soc_real = soc_ocv_lookup(cell_min)
        remaining_mah = q_max * (int64_t)soc_real / 1000L
        // Q_max 学习 (仅放电周期, 除零保护)
        if from_discharge && last_q_passed > q_max / 2:
          uint16_t delta_soc = soc_at_entry - last_soc_end
          if delta_soc > 0:
            int32_t Q_new = last_q_passed * 1000L / (int32_t)delta_soc
            q_max += (Q_new - q_max) / 10
        → IDLE, idle_baseline = cell_min  // 仅在此时更新基准
```

### 关键接口

```c
/* === soc_ocv.h === */

typedef enum {
    SOC_STATE_IDLE          = 0U,
    SOC_STATE_DISCHARGE     = 1U,
    SOC_STATE_CHARGE        = 2U,
    SOC_STATE_POLARIZATION  = 3U,
} soc_state_t;

typedef struct {
    /* 输出 */
    uint16_t     soc_display;       // 显示 SOC [0,1000]
    uint16_t     soc_real;          // 真实 SOC [0,1000]
    uint16_t     ocv_mv;            // OCV 参考值 (mV)
    int32_t      remaining_mah;

    /* Q_max */
    int32_t      q_max_mah;         // 学习到的满容量
    int32_t      q_passed_mah;      // 本周期累计电量

    /* 状态机 */
    soc_state_t  state;
    uint16_t     idle_baseline_mv;   // IDLE 基准电压 (仅 POL→IDLE 时更新)
    uint16_t     pol_entry_mv;       // 进入 POLARIZATION 时的电压
    uint16_t     soc_at_entry;       // 进入充/放时的 SOC
    uint32_t     polarization_ms;
    uint8_t      from_discharge;     // 0=充电周期, 1=放电周期 (Q_max 学习门控)

    /* Q_max 学习历史 */
    uint16_t     last_soc_end;
    int32_t      last_q_passed;

    /* 充电平台检测 (5min 滑动窗口) */
    uint16_t     charge_peak_mv;     // 窗口内最高电压
    uint16_t     charge_valley_mv;   // 窗口内最低电压
    uint32_t     charge_window_ms;   // 窗口累计时间

    uint8_t      initialized;
} soc_ctx_t;

/* API */
void          soc_ocv_init(int32_t nominal_mah);
void          soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma,
                              uint32_t dt_ms);           // 移除 temp_mdeg
uint16_t      soc_ocv_get_soc(void);                     // → 显示 SOC
uint16_t      soc_ocv_get_real_soc(void);                // [NEW] 真实 SOC
uint16_t      soc_ocv_get_ocv(void);
int32_t       soc_ocv_get_remaining_mah(void);
soc_state_t   soc_ocv_get_state(void);                   // [NEW]
int32_t       soc_ocv_get_q_max(void);                   // [NEW]
uint16_t      soc_ocv_lookup(uint16_t ocv_mv);           // 保留
const soc_ctx_t *soc_ocv_get_ctx(void);

/* 配置 */
#define SOC_DISPLAY_OFFSET      30    // 底部隐藏 3%
#define SOC_DISPLAY_SCALE       970   // 映射分母
#define Q_MAX_EMA_DIVISOR       10    // EMA 除数 (α=0.1)
#define POLARIZATION_TIME_MS       1800000UL  // 30 分钟
#define CHARGE_PLATEAU_DELTA_MV          5U   // 充电平台 ΔV 阈值 (mV)
#define CHARGE_PLATEAU_WINDOW_MS     300000UL  // 充电平台检测窗口 (5 分钟)
#define CHARGE_DETECT_CURRENT_MA        200    // 充电器接入检测电流 (mA)
#define CHARGE_DETECT_DEBOUNCE_S         10U   // 充电器接入去抖时间 (秒)
```

### 错误处理

| 路径 | 处理 |
|------|------|
| IDLE 中 OCV 查表超出范围 | clamp 到 [0, 1000] |
| 安时积分导致 remaining_mah < 0 | clamp 到 0 |
| 安时积分导致 remaining_mah > q_max | clamp 到 q_max |
| Q_max 学习时分母为 0 (soc_at_entry == last_soc_end) | 跳过本次学习 |
| 极化计时器溢出 (uint32_t @ 1s 增量) | ~136 年溢出，不会发生 |
| 充电 5min 平台检测窗口首次填充 | 窗口未满 5min 前不判断 |

### 测试策略

| Scenario | 维度 | 验证 |
|----------|------|------|
| 上电 OCV 初始化 | unit | state=IDLE, soc_real 从查表获得 |
| IDLE→DISCHARGE (I=-800mA) | unit | state=DISCHARGE |
| IDLE→CHARGE (V+130mV) | unit | state=CHARGE |
| DISCHARGE→POLARIZATION (I=-200mA) | unit | state=POLARIZATION |
| DISCHARGE→POLARIZATION (cell<3000mV) | unit | state=POLARIZATION (安全优先) |
| CHARGE→POLARIZATION (5min ΔV<5mV) | unit | state=POLARIZATION |
| POLARIZATION→IDLE (30min) + OCV 修正 | unit | state=IDLE, soc 被 OCV 覆盖 |
| 安时积分 1s 精度 | unit | 输入固定 I/dt, 手算验证 |
| Q_max 学习 (Q_passed>50%) | unit | q_max EMA 更新 |
| Q_max 不学习 (Q_passed<50%) | unit | q_max 不变 |
| SOC 映射: 30→0, 1000→1000, 515→500 | unit | 三个关键点 |
| SOC 映射: 20→0 (clamp) | unit | 底部 clamp |
| IDLE OCV 更新 (I<200mA) | unit | soc_real 被 OCV 覆盖 |
| IDLE 不更新 (I≥200mA) | unit | soc_real 保持不变 |

## 风险与边缘情况

| 风险 | 缓解 |
|------|------|
| 上电时电池正在充放电（非 IDLE 态） | init 后首次 update 强制进入 IDLE，用 OCV 查表初始化。若实际在放电，下一周期立即检测到 I<-500mA 并跳转到 DISCHARGE |
| 极化 30min 内被新充放电打断 | POLARIZATION 中仍检查转换条件——若检测到放电/充电条件，立即跳转（牺牲本次 OCV 修正，优先跟踪） |
| Q_max 初始值 20000mAh 与实际电池严重不符 | 首次深度放电后 Q_max 会学习到接近真实值。建议上电后尽快完成一次 >50% 深度放电 |
| 充电结束条件（5min ΔV<5mV）在 CC 阶段末期可能误触发 | 5min 窗口和 5mV 阈值联合保证仅在 CV 平台末期触发。CC 阶段电压持续上升，ΔV 远超 5mV |
| `soc_ocv_get_soc()` 语义变更影响现有消费者 | `bms_shared_update_soc` 和 CAN 上报路径自动使用映射后 SOC。如需真实 SOC 调试，使用 `soc_ocv_get_real_soc()` |

## 开放问题

- 极化 30min 计时是否需要持久化（掉电后恢复）？当前设计掉电丢失，重新上电从头开始
- 充电平台检测的 5min/5mV 参数是否需要根据具体电芯型号调优？18650 NMC 的 CV 阶段通常在 4.15-4.20V 平台
- `temp_mdeg` 参数移除后，`soc_task` 不再需要读取温度。是否保留温度读取供未来扩展使用？
