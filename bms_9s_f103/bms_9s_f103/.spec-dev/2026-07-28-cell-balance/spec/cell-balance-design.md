---
# —— spec-dev 漂移守卫锚点（机器可校验，勿删）——
spec_dev:
  version: 1
  feature: cell-balance
  status: draft
  covers:
    - "App/Src/bms_app.c"
    - "App/Inc/bms_app.h"
    - "BSP/Src/bq76940.c"
    - "BSP/Inc/bq76940.h"
    - "App/Src/bms_shared.c"
    - "App/Inc/bms_shared.h"
    - "App/Inc/protection.h"
    - "App/Src/can_cmd.c"
  sync_commit: null
---

# Cell Balance — 9 串电芯被动均衡设计

## 背景与目标

基于 BQ76940 内部 FET 实现 9 串电芯被动均衡。均衡任务以 1s 周期运行，当最高与最低电芯压差超过 50mV 时对最高电压电芯开启均衡，压差降至 30mV 以下时停止。均衡仅在无故障（CELL_IMBALANCE 除外）时执行。

**成功标准**：
- 电芯压差维护在 30mV 以内（均衡停止阈值）
- 故障条件下自动停止均衡，故障清除后自动恢复
- 单电芯均衡策略天然满足 BQ76940 的 VC 组内相邻位不得同时置 1 的硬件约束

## 非目标

- 外部均衡电路控制（仅使用 BQ76940 内部 FET，~5mA）
- 相邻电芯同时均衡的复杂调度（单电芯策略天然规避）
- 均衡时间限制或超时保护（被动均衡是慢维护过程，无需超时）
- 修改 CubeMX `.ioc` 配置

## 术语表

| 术语 | 定义 | Avoid |
|------|------|-------|
| CELLBAL 位 | BQ76940 CELLBAL1/2/3 寄存器中的单 bit，控制对应 VCx 通道的均衡 FET 开关 | 均衡位、balance bit |
| CELLBAL mask | 16-bit 位掩码，`bit(N-1)` = VCx 通道 N 的均衡开关，0=关/1=开 | 均衡掩码、balance_mask |
| 被动均衡 | 通过并联电阻消耗高电压电芯的多余能量，电流 ~5mA；区别于主动均衡（能量转移） | 耗散均衡、bypass |
| 均衡滞回 | 开启阈值 50mV 与停止阈值 30mV 之间的 20mV 死区，防止边界振荡 | hysteresis、回差 |

## 影响面

| 模块 | 影响 | 改动量 |
|------|------|--------|
| `bms_app.c` | 新增 `task_balance` 任务函数 + 静态 CELLBAL 位映射表；`bms_app_init` 中创建任务 | ~80 行 |
| `bms_app.h` | 新增均衡周期宏 `BALANCE_PERIOD_MS` + `FAULT_MASK_BLOCK_BALANCE` 宏 + task_balance 声明 | ~10 行 |
| `bq76940.h` | CELLBAL1/2 地址修正 (0x06→0x01, 0x07→0x02)；新增 CELLBAL3 (0x03)；`bq76940_set_balancing` mask 扩展至 15 bit | ~10 行 |
| `bq76940.c` | `set_balancing`/`get_balancing` 新增 CELLBAL3 读写；`read_cells` 重构为逐通道读取 9 个在用 VC | ~80 行 |
| `bms_shared.h/c` | `bms_settings_t` 移除 `balance_thresh_mv` / `balance_min_mv` 字段及默认值 | ~10 行 |
| `protection.h` | CELL_IMBALANCE 从均衡屏蔽故障掩码排除（详见 [ADR-0007](../../adr/0007-cell-balance-fault-imbalance-exclusion.md)） | 0 行（仅宏定义在 bms_app.h） |
| `can_cmd.c` | 移除 `balance_thresh_mv` / `balance_min_mv` 的 CAN 配置命令（0x05/0x06） | ~8 行 |

## 已确认的关键决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 均衡目标 | 仅最高电压单电芯 | 单电芯策略天然满足 BQ76940 相邻位约束；被动均衡电流仅 5mA，多电芯分流效果差；实现最简单 |
| 阈值策略 | 硬编码 50mV 开启 / 30mV 停止 / 3200mV 最低电压 | 阈值在调试稳定后无需频繁修改；移除 settings 字段和 CAN 配置命令，节省 RAM 和代码 |
| CELL_IMBALANCE 与均衡 | CELL_IMBALANCE 不屏蔽均衡 | 均衡是电芯压差过大的修复手段，屏蔽会导致死锁。详见 [ADR-0007](../../adr/0007-cell-balance-fault-imbalance-exclusion.md) |
| 快照策略 | 栈快照（持锁→拷贝到栈→释放） | 均衡判断不使用实时数据（1s 周期不要求 µs 级一致性），栈快照持锁时间最短 |
| BSP 修改归属 | 均衡 spec 包含 BSP 寄存器地址修正 + CELLBAL3 新增 | 均衡是 CELLBAL 寄存器的唯一消费者，一并修改减少 spec 碎片化 |
| cells_mv[] 布局 | 改为按物理 cell1~cell9 顺序存放 9 个在用 VC 通道 | 均衡任务只需知道"第几个电芯最高"，用静态查找表一次映射到 CELLBAL 位 |

## ADDED Requirements

### Requirement: 系统 SHALL 创建 task_balance 周期性均衡任务

系统 SHALL 在 `bms_app_init()` 中创建 task_balance 任务（优先级 `osPriorityBelowNormal`，栈 1024B，周期 1000ms）。任务首次执行时 SHALL 通过 `osEventFlagsWait(evt_data_ready, 0x01, …, 5000ms)` 等待首次采集数据就绪；超时后 SHALL 进入主循环（无数据时所有电压为 0，diff=0，不触发均衡）。

#### Scenario: 任务创建成功

- **GIVEN** FreeRTOS 堆足够分配 1024B 栈 + TCB
- **WHEN** `bms_app_init()` 调用 `osThreadNew(task_balance, NULL, &attr_balance)`
- **THEN** 返回非 NULL 的任务句柄，task_balance 进入就绪态并阻塞在 `evt_data_ready` 上

#### Scenario: 首次数据就绪后进入主循环

- **GIVEN** task_balance 阻塞在 `evt_data_ready`，task_sample 完成首次采集
- **WHEN** task_sample 调用 `osEventFlagsSet(evt_data_ready, 0x01)`
- **THEN** task_balance 被唤醒，进入 1000ms 周期主循环

#### Scenario: evt_data_ready 超时

- **GIVEN** task_balance 阻塞在 `evt_data_ready`，BQ76940 通信故障导致 5s 内无数据
- **WHEN** `osEventFlagsWait` 返回超时
- **THEN** task_balance 打印调试信息后进入主循环，cells_mv 全零 → diff=0 → 不触发均衡，直到下次数据就绪

### Requirement: task_balance SHALL 以栈快照方式读取电芯数据

task_balance SHALL 通过 `osMutexAcquire(mutex_data, 10ms)` 获取数据锁，将 `cells_mv[9]` 和 `active_faults` 拷贝到栈上局部变量，然后立即释放锁。后续均衡判断 SHALL 全部基于栈快照。

#### Scenario: 正常获取快照

- **GIVEN** task_sample 已完成一次采集并持有 mutex_data 不超过 ~2ms
- **WHEN** task_balance 调用 `osMutexAcquire(mutex_data, 10ms)`
- **THEN** 在 10ms 内获取锁，拷贝数据，释放锁

#### Scenario: 数据锁超时

- **GIVEN** task_sample 持有 mutex_data 超过 10ms（异常情况）
- **WHEN** task_balance 调用 `osMutexAcquire(mutex_data, 10ms)`
- **THEN** 返回超时，跳过本轮均衡，`osDelay(1000)` 后重试

### Requirement: 均衡开启条件 SHALL 为压差 > 50mV 且最高电压 > 3200mV 且无屏蔽故障

系统 SHALL 在同时满足以下条件时对最高电压电芯开启均衡：
1. `(max_mv - min_mv) > 50mV`
2. `max_mv > 3200mV`
3. `(active_faults & FAULT_MASK_BLOCK_BALANCE) == 0`，其中 `FAULT_MASK_BLOCK_BALANCE = ALL_FAULTS & ~FAULT_CELL_IMBALANCE`

#### Scenario: 压差超过阈值 → 开启均衡

- **GIVEN** cells_mv = {3600, 3520, 3540, 3550, 3560, 3530, 3540, 3510, 3500}，max=3600(idx=0, VC1)，min=3500(idx=8)，diff=100mV，active_faults=0
- **WHEN** task_balance 执行均衡判断
- **THEN** balance_mask = (1 << 0) = 0x0001（CB1：CELLBAL1 bit 0），开启 VC1 均衡

#### Scenario: 压差低于停止阈值 → 关闭均衡

- **GIVEN** cells_mv = {3540, 3520, 3530, 3535, 3525, 3530, 3520, 3510, 3510}，max=3540，min=3510，diff=30mV，不大于 50mV，当前有均衡正在运行
- **WHEN** task_balance 执行均衡判断
- **THEN** balance_mask = 0x0000，关闭所有均衡

#### Scenario: 滞回区内 → 保持上一轮状态

- **GIVEN** 上一轮 mask = 0x0001（均衡中），本轮 diff = 40mV（在 30~50 滞回区内）
- **WHEN** task_balance 执行均衡判断
- **THEN** balance_mask 保持 0x0001，不反复开关

#### Scenario: 最高电压低于最低门槛 → 不均衡

- **GIVEN** cells_mv = {3180, 3120, 3150, ...}，max=3180mV < 3200mV，diff=60mV > 50mV
- **WHEN** task_balance 执行均衡判断
- **THEN** balance_mask = 0x0000，即使压差超阈值也不均衡（电池已近耗尽）

#### Scenario: CELL_OV 故障 → 不均衡

- **GIVEN** cells_mv diff = 80mV > 50mV，active_faults = FAULT_CELL_OV
- **WHEN** task_balance 检查 `(active_faults & FAULT_MASK_BLOCK_BALANCE)`
- **THEN** 结果非零，跳过均衡，balance_mask = 0x0000

#### Scenario: CELL_IMBALANCE 故障 → 仍均衡

- **GIVEN** cells_mv diff = 520mV > 500mV → 触发 FAULT_CELL_IMBALANCE，active_faults = FAULT_CELL_IMBALANCE
- **WHEN** task_balance 检查 `(active_faults & FAULT_MASK_BLOCK_BALANCE)`
- **THEN** `FAULT_CELL_IMBALANCE` 被排除 → 结果为 0 → 继续均衡（均衡是压差过大的修复手段）

### Requirement: 均衡任务 SHALL 通过 I2C 写 CELLBAL 寄存器控制均衡开关

task_balance SHALL 在 `balance_mask` 发生变化时，通过 `osMutexAcquire(mutex_iic, 10ms)` 获取 I2C 总线，调用 `bq76940_set_balancing(mask)` 写入 CELLBAL1/2/3 寄存器，之后释放 mutex_iic。mask 未变化时 SHALL 跳过 I2C 写。

#### Scenario: 均衡状态变化 → 写 CELLBAL 寄存器

- **GIVEN** 上一轮 mask=0x0000，本轮计算出 mask=0x0001（首次开启均衡）
- **WHEN** task_balance 判断 mask 已变化
- **THEN** 获取 mutex_iic → `bq76940_set_balancing(0x0001)` → 释放 mutex_iic。CELLBAL1[0]=1（CB1 开启），CELLBAL2=0，CELLBAL3=0

#### Scenario: 均衡状态未变 → 跳过 I2C 写

- **GIVEN** 上一轮 mask=0x0001，本轮计算 mask=0x0001（diff 在滞回区内）
- **WHEN** task_balance 判断 mask 未变化
- **THEN** 不获取 mutex_iic，不调用 bq76940_set_balancing，减少 I2C 总线占用

#### Scenario: I2C 写失败 → 下轮重试

- **GIVEN** mask 已变化，I2C 总线故障（连续超时）
- **WHEN** `bq76940_set_balancing()` 返回 `BQ76940_I2C_ERROR`
- **THEN** 释放 mutex_iic，task_balance 不崩溃、不重试当轮。下一轮（1s 后）mask 仍为新值 → 重新尝试 I2C 写

### Requirement: bq76940_set_balancing SHALL 支持全部 15 个 CELLBAL 位

`bq76940_set_balancing(balance_mask)` SHALL 将 16-bit mask 的 bit[4:0] 写入 CELLBAL1 (0x01)、bit[9:5] 写入 CELLBAL2 (0x02)、bit[14:10] 写入 CELLBAL3 (0x03)。`bq76940_get_balancing()` SHALL 回读三个寄存器并组装为相同的 16-bit mask。

#### Scenario: 设置 VC15 均衡

- **GIVEN** balance_mask = 0x4000（bit 14 = CB15）
- **WHEN** `bq76940_set_balancing(0x4000)` 被调用
- **THEN** CELLBAL1=0x00, CELLBAL2=0x00, CELLBAL3=0x10（bit 4 = CB15）

#### Scenario: 回读均衡状态

- **GIVEN** CELLBAL1=0x01, CELLBAL2=0x00, CELLBAL3=0x00
- **WHEN** `bq76940_get_balancing()` 被调用
- **THEN** 返回 0x0001

### Requirement: bq76940_read_cells SHALL 按物理接线读取 9 个在用 VC 通道

`bq76940_read_cells()` SHALL 按 VC1、VC2、VC5、VC6、VC7、VC10、VC11、VC12、VC15 的顺序逐通道读取电芯电压，依次填入 `cells_mv[0..8]`。短路通道（VC3/4/8/9/13/14）SHALL 不被读取。`cells_mv[i]` 到 CELLBAL 位的映射 SHALL 由 task_balance 内的静态查找表 `cellbal_bit_map[9]` 维护。

#### Scenario: 读取全部 9 个在线电芯

- **GIVEN** 硬件 VC1/2/5/6/7/10/11/12/15 连接 9 个电芯，其余 VC 通道短路
- **WHEN** `bq76940_read_cells(&cells)` 被调用
- **THEN** `cells.cell_mv[0]`=VC1 电压, `cells.cell_mv[1]`=VC2, `cells.cell_mv[2]`=VC5, ..., `cells.cell_mv[8]`=VC15。读取过程中跳过 VC3/4/8/9/13/14

#### Scenario: 任意通道读取失败 → valid=0

- **GIVEN** I2C 总线故障导致某个 VC 通道读取返回错误
- **WHEN** `bq76940_read_cells()` 在任一通道上失败
- **THEN** `cells.valid = 0`，函数返回 `BQ76940_I2C_ERROR`

## MODIFIED Requirements

### MODIFIED: CELLBAL 寄存器地址修正

`BQ76940_REG_CELLBAL1` SHALL 从 0x06 改为 **0x01**；`BQ76940_REG_CELLBAL2` SHALL 从 0x07 改为 **0x02**；新增 `BQ76940_REG_CELLBAL3 = 0x03U`。上述地址 SHALL 与 TI BQ76940 数据手册 (SLUSC25B) 一致。

> **风险标注**：当前代码地址 (0x06/0x07) 与数据手册 (0x01/0x02) 冲突。如果当前代码已在真实硬件上验证通过，需确认芯片变体和实际寄存器映射。**本次修改在上板验证前应保持谨慎**，备选方案为仅新增 CELLBAL3 而保持旧地址不变。

#### Scenario: CELLBAL1 写入正确寄存器

- **GIVEN** BQ76940 I2C 地址 0x08（7-bit）
- **WHEN** `bq76940_set_balancing(0x0001)` 调用 `i2c_sw_write_crc(0x08, 0x01, 0x01)`
- **THEN** 数据写入地址 0x01（CELLBAL1），而非 0x06（原 PROTECT1 地址）

## REMOVED Requirements

### REMOVED: bms_settings_t 中的均衡阈值字段

`bms_settings_t` 中的 `balance_thresh_mv` 和 `balance_min_mv` 字段 SHALL 被移除。均衡阈值改为硬编码常量（`BALANCE_START_DIFF_MV=50`, `BALANCE_STOP_DIFF_MV=30`, `BALANCE_MIN_CELL_MV=3200`）。`bms_shared_init()` 中的对应默认值赋值 SHALL 被移除。

移除理由：阈值在调试稳定后无需运行时修改；移除 CAN 配置命令路径（can_cmd.c 的 0x05/0x06 case），简化代码并节省 RAM。

### REMOVED: CAN 配置命令中的均衡参数

`can_cmd.c` 中处理 CAN ID 0x202 参数 0x05（`balance_thresh_mv`）和 0x06（`balance_min_mv`）的 case 分支 SHALL 被移除。CAN 0x201 控制命令中的 `BALANCE_SET`（0x30）和 `BALANCE_OFF`（0x31）SHALL 保留。

移除理由：硬编码阈值后不再需要 CAN 配置读写。

---

## 方案设计

### 架构与组件

```
App 层:
  bms_app.c::task_balance()      任务编排：锁→快照→判断→I2C 写
  bms_app.h                      均衡周期宏 + FAULT_MASK_BLOCK_BALANCE 宏
  static cellbal_bit_map[9]      电芯索引(0-8)→CELLBAL 位号(0-14) 映射

BSP 层:
  bq76940_set_balancing(mask)    写 CELLBAL1/2/3 (mask bit[14:0] = CB1-CB15)
  bq76940_get_balancing()        读 CELLBAL1/2/3 组装 16-bit mask
  bq76940_read_cells()           逐通道读 9 个在用 VC
  bms_shared_t                   移除 balance_thresh_mv / balance_min_mv
```

### 数据流

```
task_balance (prio osPriorityBelowNormal, 1000ms)
  │
  ├─[首次] osEventFlagsWait(evt_data_ready)  ← 阻塞至首次 data_ready 或 5s 超时
  │
  └─[循环]
      osMutexAcquire(mutex_data, 10ms)
      ├─ 栈快照: cells_mv[9], active_faults
      osMutexRelease(mutex_data)
      │
      ├─ (active_faults & FAULT_MASK_BLOCK_BALANCE) != 0? ─→ 跳过, mask=0
      │
      ├─ 遍历 cells_mv[0..8]: argmax(max_mv), min(min_mv)
      │   diff = max_mv - min_mv
      │
      ├─ diff > 50mV 且 max_mv > 3200mV? ─→ mask = 1 << cellbal_bit_map[max_idx]
      ├─ diff < 30mV?                      ─→ mask = 0
      └─ 30 ≤ diff ≤ 50?                   ─→ mask 保持上一轮值
      │
      ├─ mask != prev_mask?
      │   osMutexAcquire(mutex_iic, 10ms)
      │   bq76940_set_balancing(mask)
      │   osMutexRelease(mutex_iic)
      │   prev_mask = mask
      │
      osDelay(1000)
```

### 关键接口

#### bms_app.h — 新增宏

```c
#define BALANCE_PERIOD_MS           1000U  // 均衡任务周期

// 均衡屏蔽故障掩码：所有故障中排除 CELL_IMBALANCE
// (均衡是压差过大的修复手段，CELL_IMBALANCE 不应阻止均衡)
#define FAULT_MASK_BLOCK_BALANCE   (ALL_FAULTS & ~FAULT_CELL_IMBALANCE)
```

#### bms_app.c — 静态映射表

```c
/** @brief cells_mv[] 索引 → CELLBAL 位号映射
 *  cells_mv[0]=VC1→CB1, [1]=VC2→CB2, [2]=VC5→CB5,
 *  [3]=VC6→CB6, [4]=VC7→CB7, [5]=VC10→CB10,
 *  [6]=VC11→CB11, [7]=VC12→CB12, [8]=VC15→CB15
 */
static const uint8_t cellbal_bit_map[9] = {
    0U,   // VC1  → CB1  (CELLBAL1 bit 0)
    1U,   // VC2  → CB2  (CELLBAL1 bit 1)
    4U,   // VC5  → CB5  (CELLBAL1 bit 4)
    5U,   // VC6  → CB6  (CELLBAL2 bit 0)
    6U,   // VC7  → CB7  (CELLBAL2 bit 1)
    9U,   // VC10 → CB10 (CELLBAL2 bit 4)
    10U,  // VC11 → CB11 (CELLBAL3 bit 0)
    11U,  // VC12 → CB12 (CELLBAL3 bit 1)
    14U,  // VC15 → CB15 (CELLBAL3 bit 4)
};
```

#### bq76940.h — 寄存器地址修正

```c
// CELLBAL 寄存器（与 TI SLUSC25B 一致）
#define BQ76940_REG_CELLBAL1     0x01U   // was 0x06: VC1-VC5
#define BQ76940_REG_CELLBAL2     0x02U   // was 0x07: VC6-VC10
#define BQ76940_REG_CELLBAL3     0x03U   // [NEW] VC11-VC15

// [DEPRECATED] 旧地址不再使用，保留注释供迁移参考：
// #define BQ76940_REG_CELLBAL1_OLD 0x06U
// #define BQ76940_REG_CELLBAL2_OLD 0x07U

// bq76940_set_balancing mask 更新为 15-bit:
//   bit[4:0]  → CELLBAL1 = CB1-CB5
//   bit[9:5]  → CELLBAL2 = CB6-CB10
//   bit[14:10]→ CELLBAL3 = CB11-CB15
```

#### bq76940.c — CELLBAL3 扩展

```c
bq76940_status_t bq76940_set_balancing(uint16_t balance_mask)
{
    uint8_t bal1 = (uint8_t)(balance_mask & 0x1FU);         // CB1-CB5
    uint8_t bal2 = (uint8_t)((balance_mask >> 5U) & 0x1FU); // CB6-CB10
    uint8_t bal3 = (uint8_t)((balance_mask >> 10U) & 0x1FU); // CB11-CB15 [NEW]

    i2c_sw_write_crc(…, BQ76940_REG_CELLBAL1, bal1);
    i2c_sw_write_crc(…, BQ76940_REG_CELLBAL2, bal2);
    i2c_sw_write_crc(…, BQ76940_REG_CELLBAL3, bal3);        // [NEW]
}
```

### 错误处理

| 错误路径 | 处理 |
|----------|------|
| mutex_data 10ms 超时 | 跳过本轮，`osDelay(1000)` 后重试 |
| mutex_iic 10ms 超时 | 跳过本轮 I2C 写，mask 未更新，下轮重新判断 |
| bq76940_set_balancing 返回 I2C_ERROR | 不崩溃，不重试当轮。下轮 (1s 后) 如 mask 仍需变化则重试 |
| evt_data_ready 5s 超时 | 进入主循环，cells_mv 全零 → diff=0 → 不均衡 |
| CELLBAL 被 DEVICE_XREADY 自动清除 | 每 1s 重写，最多 1s 无均衡 |
| 最高电压 < 3200mV | 不均衡，等待充电恢复 |
| 两个电芯电压并列最高 | 选低索引（确定性） |

### 测试与验收策略

| Scenario / 检查项 | 维度 | 执行方式 | 验收证据 |
|-------------------|------|---------|---------|
| task_balance 创建成功 | integration | 任务内 TDD | osThreadNew 返回非 NULL |
| 首次 evt_data_ready 阻塞 | integration | 任务内 TDD | 任务在 data_ready 到达前不执行均衡逻辑 |
| 压差 100mV → 开启均衡 (mask != 0) | unit | 任务内 TDD | 栈快照输入 + active_faults=0 → mask 为正确的单 bit |
| 压差 20mV → 关闭均衡 (mask == 0) | unit | 任务内 TDD | diff < 30mV → mask = 0 |
| 滞回区 40mV → mask 不变 | unit | 任务内 TDD | 连续两轮相同输入 → mask 不变 |
| max_cell < 3200mV → 不均衡 | unit | 任务内 TDD | max_mv=3180, diff=80mV → mask=0 |
| CELL_OV 故障 → 不均衡 | unit | 任务内 TDD | active_faults 含 FAULT_CELL_OV → mask=0 |
| CELL_IMBALANCE 故障 → 仍均衡 | unit | 任务内 TDD | active_faults 仅 CELL_IMBALANCE → mask != 0 |
| 最高并列 → 选低索引 | unit | 任务内 TDD | 两个相同 max → 选 idx 更小者 |
| I2C 写 CELLBAL3 | integration | 任务内 TDD | mask 含 bit[14:10] → i2c_sw_write_crc 调用到 0x03 |
| bq76940_get_balancing 回读 CELLBAL3 | integration | 任务内 TDD | 写后回读 mask 一致 |
| read_cells 仅读 9 个在用通道 | integration | 任务内 TDD | cells_mv[0..8] = VC1/2/5/6/7/10/11/12/15 电压 |

## 风险与边缘情况

| 风险 | 缓解 |
|------|------|
| 寄存器地址修正可能破坏硬件兼容性 | 上板前对比代码地址与硬件实测；备选：仅新增 CELLBAL3 不改旧地址（代码注释中保留旧地址定义） |
| 单电芯均衡收敛极慢（5mA 被动均衡） | 此乃被动均衡固有特性。均衡是维护性功能（维持已平衡状态），50mV 启动阈值保证仅在需要时介入 |
| DEVICE_XREADY 清除 CELLBAL 后 1s 真空 | 1s 对均衡效果影响可忽略（均衡是持续数分钟至数小时的过程） |
| task_balance 持 mutex_iic 阻塞 task_protect | balance 优先级 BelowNormal 且持锁 < 1ms（仅 I2C 写 3 字节），不会显著阻塞 |
| 未注册的 VC 通道（VC3/4/8/9/13/14）读到噪声 | 直接跳过不读，不在 cells_mv 中出现 |
| I2C 频繁超时导致均衡无法执行 | 通信恢复后自动恢复均衡；COMM_LOSS 故障阻止均衡（保护通信丢失时的确定性行为） |

## 开放问题

- `BQ76940_REG_CELLBAL1/2` 地址修正需硬件实测确认：当前代码地址 (0x06/0x07) 是否针对特定芯片变体？若硬件验证确认旧地址有效，则保留旧地址、仅新增 CELLBAL3（0x03 或对应地址）
- `read_cells` 重构为逐通道读取后的 I2C 吞吐量：9 次独立 Read 事务 vs 原来 1 次 18 字节批量读，I2C 总线占用增加约 8 倍（每次事务有 Start/Stop 开销）。如果 task_sample 的 100ms 周期不能容纳，备选方案为保持批量读但用位掩码提取有效通道
