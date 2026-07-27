---
# —— spec-dev 漂移守卫锚点（机器可校验，勿删）——
spec_dev:
  version: 1
  feature: comm-loss
  status: draft
  covers:
    - BSP/Inc/i2c_sw.h
    - BSP/Src/i2c_sw.c
    - BSP/Inc/bq76940.h
    - BSP/Src/bq76940.c
    - App/Inc/protection.h
    - App/Src/protection.c
    - App/Inc/can_cmd.h
    - App/Src/can_cmd.c
    - App/Inc/bms_app.h
    - App/Src/bms_app.c
  sync_commit: null
---

# COMM_LOSS — BQ76940 I2C 通信丢失检测与处理

## 背景与目标

> **架构依赖**: 本 spec 假定 `bms-fullstack-redesign`（`.spec-dev/2026-07-26-bms-fullstack-redesign/`）中的 7 任务事件驱动架构、同步原语（`mutex_iic`、`q_can_tx`、`ring_buf_put_front`、`evt_protect`）及 `protection_check` 新签名均已实现。实施计划需将 fullstack-redesign 排在 comm-loss 之前。

BQ76940 通过 I2C 与 MCU 通信。I2C 总线可能因电磁干扰、连接器松动、芯片异常复位等原因中断。当前代码 (`protection_check`) 仅在数据 `valid` 标志无效时静态检查——无超时检测、无误计数、无独立响应路径，且 `FAULT_COMM_LOSS` 在故障分级中未被任何分支匹配（bug：通信丢失静默落入 `PROT_LVL_NONE`）。

本次变更在 i2c_sw 层增加位带超时、在 bq76940 层计连续超时次数、新增 `task_iic_error`（优先级 5）做独立 CAN 紧急上报，并修复 `protection_check` 中 COMM_LOSS 的遗留缺陷。

**成功标准**：
- I2C 总线任意位带操作超时可被检测（不再死等）
- 连续 2 次 I2C 超时后 ≤ 1 个 RTOS tick 内将 CAN 紧急帧头插入发送队列（实际 CAN 总线传输由 task_can_tx 在下一个 100ms 周期执行）
- 通信恢复后立即发送恢复帧
- BQ76940 硬件保护在通信中断期间持续自治运行

## 非目标

- I2C 总线自动恢复（9 时钟脉冲释放总线）——由硬件上拉电阻自然恢复
- BQ76940 重新初始化/复位重连——属于更高层恢复策略，不在本次范围
- I2C DMA 或中断驱动改造——软件 I2C 位带操作保持不变
- 修改 CubeMX `.ioc` 配置

## 术语表

| 术语 | 定义 | Avoid |
|------|------|-------|
| I2C 超时 / I2C timeout | i2c_sw 位带操作中，SCL 拉高后在限定循环次数内未检测到预期电平 | 总线挂死、bus hang |
| comm_timeout_cnt | bq76940 层静态计数器，记录连续 I2C 超时次数 | 错误计数、fail_count |
| task_iic_error | 新增 FreeRTOS 任务，优先级 5（最高），由 sem_iic 唤醒，仅做 CAN 头插上报，不操作 I2C | iic_task、error_task |
| sem_iic | 二元信号量，bq76940 层 release → task_iic_error acquire | iic_sem、comm_sem |
| 头插 | ring_buf_put_front()，帧写入环形缓冲区逻辑头部，优先于尾插帧发送 | 优先级入队 |

## 影响面

| 模块 | 影响 | 改动量 |
|------|------|--------|
| `i2c_sw.h/c` | 在 `i2c_send_byte`/`i2c_recv_byte` 加超时循环（使已有枚举值 `I2C_SW_TIMEOUT=0x02` 可被实际返回） + 新增 `I2C_SW_TIMEOUT_CYCLES` 宏 | ~30 行 |
| `bq76940.h/c` | 新增 `comm_timeout_track()` 静态函数 + `comm_timeout_cnt`/`comm_is_faulted` 静态变量；`read_cells` 错误路径补 `.valid=0`；新增 `bq76940_is_faulted()` getter | ~40 行 |
| `protection.c` | 移除 `FAULT_COMM_LOSS` 检测逻辑（data.valid 检查代码段） | ~5 行 |
| `can_cmd.h/c` | `can_pub` 新增 `can_pub_build_fault()` / `can_pub_build_recovered()` 接口（供 task_iic_error 和 task_protect 共用） | ~20 行 |
| `bms_app.h/c` | 新增 task_iic_error 任务函数 + sem_iic 信号量；任务优先级从 6 任务更新为 7 任务 | ~50 行 |

## 已确认的关键决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 检测点 | bq76940 层（`bq76940_read_all` 及各 write 函数返回后） | I2C 超时可发生在任何 I2C 操作（sample 读 / protect 写 / balance 写），bq76940 层是统一汇聚点 |
| 确认/恢复计数 | 确认 2 次连续超时，恢复 1 次成功 | 用户裁决：2 次去抖防止单次毛刺，恢复即时响应 |
| 同步原语 | 二元信号量 `sem_iic` | 仅两种状态（故障/恢复），二元信号量比事件组更轻量（~40B vs ~60B）且语义更直接 |
| 响应任务 | 新增 task_iic_error（优先级 5），完全独立于 task_protect | 通信丢失是最高级故障——MCU 失明。独立任务保证上报不受 task_protect 阻塞影响；不操作 I2C 避免二次超时 |
| FET 控制 | 不通过 I2C 控制 FET，信任 BQ76940 硬件保护自治 | I2C 中断时写寄存器必然失败；BQ76940 OV/UV/OCD/SCD 硬件保护在配置后独立运行 |
| I2C 超时实现层 | i2c_sw 层位带操作加循环超时 | 最底层覆盖所有 I2C 操作，一处修改全量受益 |
| 超时计数器位置 | bq76940 层 static 变量 | 保持 i2c_sw 为纯 BSP 层（不感知 FreeRTOS），计数和信号量操作在 bq76940 层完成 |
| 任务优先级 | iic(5) > protec(4) > canrx(3) = sample(3) > balance(2) = soc(2) > cantx(1) | 详见 [ADR-0006](../../adr/0006-iic-error-task-priority.md) |

## 行为规范

> 本次为**修改既有行为**，使用差量三节。既有行为基于 active spec `bms-fullstack-redesign`。

### ADDED Requirements

#### Requirement: i2c_sw 位带操作 SHALL 具备超时检测

每条 `i2c_send_byte()` 和 `i2c_recv_byte()` 调用中，SCL 拉高后等待 SDA 变化的循环 SHALL 有最大迭代次数限制。超时后 SHALL 返回 `I2C_SW_TIMEOUT`（枚举值 `0x02`，已在 `i2c_sw.h` 中定义但当前未使用），并释放 SCL（拉低）终止当前事务。

##### Scenario: ACK 位超时

- **GIVEN** i2c_sw 已发送 8 个数据位，SCL 拉高，SDA 被从设备持续拉高（不应答）
- **WHEN** 循环计数超过 `I2C_SW_TIMEOUT_CYCLES` (10000)
- **THEN** SCL 被拉低，函数返回 `I2C_SW_TIMEOUT`，调用方可在上层检测超时

##### Scenario: 数据位超时

- **GIVEN** i2c_sw 正在接收数据字节，SCL 拉高，从设备未释放 SDA（时钟拉伸超限）
- **WHEN** 循环计数超过 `I2C_SW_TIMEOUT_CYCLES`
- **THEN** SCL 被拉低，函数返回 `I2C_SW_TIMEOUT`

##### Scenario: 正常通信不受影响

- **GIVEN** I2C 总线正常，从设备在 10 个循环内响应
- **WHEN** `i2c_send_byte()` 或 `i2c_recv_byte()` 执行
- **THEN** 超时计数器未触发，正常返回 ACK/NACK 或数据字节，行为与修改前完全一致

#### Requirement: bq76940 层 SHALL 追踪连续 I2C 超时次数

`bq76940.c` SHALL 维护 `comm_timeout_cnt` 静态变量。每次 I2C 操作返回错误时递增；任意一次成功时清零。`comm_is_faulted` 静态标志 SHALL 记录当前通信状态（0=正常, 1=丢失）。

##### Scenario: 连续 2 次超时触发 COMM_LOSS

- **GIVEN** `comm_timeout_cnt = 1`，`comm_is_faulted = 0`
- **WHEN** 下一次 I2C 操作（读或写）再次返回 `BQ76940_I2C_ERROR`（含 TIMEOUT 映射）
- **THEN** `comm_timeout_cnt = 2`，`comm_is_faulted = 1`，`osSemaphoreRelease(sem_iic)` 被调用一次

##### Scenario: 单次超时不触发（去抖）

- **GIVEN** `comm_timeout_cnt = 0`，`comm_is_faulted = 0`
- **WHEN** 一次 I2C 操作返回 `BQ76940_I2C_ERROR`
- **THEN** `comm_timeout_cnt = 1`，`comm_is_faulted = 0`，`sem_iic` 不被 release

##### Scenario: 通信恢复

- **GIVEN** `comm_is_faulted = 1`，I2C 已恢复
- **WHEN** 一次 I2C 操作返回 `BQ76940_OK`
- **THEN** `comm_timeout_cnt = 0`，`comm_is_faulted = 0`，`osSemaphoreRelease(sem_iic)` 被调用一次

##### Scenario: 故障态中持续超时不重复通知

- **GIVEN** `comm_is_faulted = 1`，`comm_timeout_cnt = 2`
- **WHEN** 后续 I2C 操作继续超时
- **THEN** `comm_timeout_cnt` 继续递增，但 `sem_iic` 不被再次 release（状态未变化）

#### Requirement: 系统 SHALL 新增 task_iic_error 独立处理通信故障

task_iic_error（优先级 5，栈 1024B）SHALL 以 `osSemaphoreAcquire(sem_iic, osWaitForever)` 阻塞。被唤醒后 SHALL 读取 `comm_is_faulted` 判断状态：若为 1 则构建 COMM_LOSS 紧急 CAN 帧并头插入 q_can_tx、点亮红灯；若为 0 则构建恢复 CAN 帧并头插入、熄灭红灯。

##### Scenario: COMM_LOSS 上报

- **GIVEN** task_iic_error 阻塞在 `sem_iic`，I2C 连续 2 次超时触发 release
- **WHEN** task_iic_error 被唤醒（立即抢占当前运行任务），`comm_is_faulted == 1`
- **THEN** CAN 帧（ID=0x100, fault_code=FAULT_COMM_LOSS）通过 `ring_buf_put_front(&q_can_tx, ...)` 头插入队，红色 LED 点亮

##### Scenario: 通信恢复上报

- **GIVEN** task_iic_error 阻塞在 `sem_iic`，I2C 恢复后 `comm_is_faulted` 变为 0 触发 release
- **WHEN** task_iic_error 被唤醒，`comm_is_faulted == 0`
- **THEN** CAN 恢复帧通过 `ring_buf_put_front(&q_can_tx, ...)` 头插入队，红色 LED 熄灭

##### Scenario: task_iic_error 不操作 I2C

- **GIVEN** task_iic_error 被唤醒处理 COMM_LOSS
- **WHEN** task_iic_error 执行
- **THEN** 不获取 `mutex_iic`，不调用任何 `bq76940_*` 函数，不访问 I2C 总线

#### Requirement: CAN 故障帧 SHALL 通过统一接口构建

`can_cmd` 模块 SHALL 提供 `can_pub_build_fault()` 和 `can_pub_build_recovered()` 两个接口，供 task_iic_error 和 task_protect 共用。帧格式 SHALL 与 task_protect 的 protect 帧一致（CAN ID=0x100, DLC=8），通过 `fault_code` 字段区分故障类型。

##### Scenario: task_iic_error 和 task_protect 共用故障帧接口

- **GIVEN** `can_pub_build_fault()` 接口已定义
- **WHEN** task_iic_error（COMM_LOSS）和 task_protect（其他故障）分别调用
- **THEN** 两个调用方生成的帧结构一致，仅 `fault_code` 字段值不同

### MODIFIED Requirements

#### MODIFIED: protection_check SHALL 不再检测 COMM_LOSS

原行为：`protection_check()` 检查 `data->cells.valid` 和 `data->temps.valid`，无效时设置 `FAULT_COMM_LOSS`。

新行为：`protection_check()` SHALL 仅检查电压/电流/温度阈值。COMM_LOSS 检测完全由 bq76940 层的 `comm_timeout_track()` 负责。`FAULT_COMM_LOSS` 保留在 `fault_code_t` 枚举中供 CAN 帧编码使用，但 `protection_check()` 内部不再产生此故障码。`ALL_FAULTS` 宏 SHALL 更新为 `0x0BFF`（排除 bit10: `0x0FFF & ~(1<<10) = 0x0BFF`）。

##### Scenario: protection_check 收到无效数据不设 COMM_LOSS

- **GIVEN** `bq76940_data_t` 中 `cells.valid == 0`（上一次读失败残留）
- **WHEN** `protection_check(&data, &settings)` 被调用
- **THEN** 函数不设置 `FAULT_COMM_LOSS`，仅跳过电压/电流阈值检查（因数据无效），返回 `prot_result_t.level = PROT_LVL_NONE`

#### MODIFIED: bq76940_read_cells SHALL 在错误路径清零 valid 标志

原行为：`bq76940_read_cells()` 在 I2C 错误时直接返回，不修改 `cells->valid`。

新行为：错误路径 SHALL 设置 `cells->valid = 0U`，与 `bq76940_read_temps()` 和 `bq76940_read_current()` 的行为一致。

##### Scenario: cells 读取失败后 valid 为 0

- **GIVEN** I2C 总线故障
- **WHEN** `bq76940_read_cells(&data.cells)` 被调用
- **THEN** 函数返回 `BQ76940_I2C_ERROR`，且 `data.cells.valid == 0U`

#### MODIFIED: bq76940_reg_read/reg_write SHALL 映射 I2C_SW_TIMEOUT

原行为：switch 仅映射 `I2C_SW_OK`、`I2C_SW_NACK`、`I2C_SW_CRC_ERROR`，default → `BQ76940_ERROR`。

新行为：switch SHALL 增加 `I2C_SW_TIMEOUT → BQ76940_I2C_ERROR` 的显式映射。超时与 NACK 均视为 I2C 通信错误。`comm_timeout_track()` 在收到 `BQ76940_I2C_ERROR` 时递增超时计数器。

##### Scenario: I2C 超时映射为 I2C_ERROR

- **GIVEN** `i2c_sw_read_crc()` 或 `i2c_sw_write_crc()` 因位带超时返回 `I2C_SW_TIMEOUT`
- **WHEN** `bq76940_reg_read()` 或 `bq76940_reg_write()` 的 switch 执行
- **THEN** 函数返回 `BQ76940_I2C_ERROR`（而非 `BQ76940_ERROR`），`comm_timeout_track()` 收到 `BQ76940_I2C_ERROR` 并递增超时计数器

### REMOVED Requirements

#### REMOVED: protection_check 中的 data.valid 检查

原行为：`protection_check()` 检查 `!data->cells.valid || !data->temps.valid` 并设置 `FAULT_COMM_LOSS`。

移除原因：COMM_LOSS 检测已上移至 bq76940 层的 `comm_timeout_track()`。`protection_check()` 的 data.valid 检查与新的分层检测策略重复，且无法区分"数据无效因为 I2C 超时"和"数据无效因为未初始化"。

---

## 方案设计

### 架构与组件

```
┌──────────────────────────────────────────────────────┐
│                    App 层                            │
│                                                      │
│  task_iic_error (new, prio 5)                        │
│    osSemaphoreAcquire(sem_iic)                       │
│    → can_pub_build_fault / can_pub_build_recovered   │
│    → ring_buf_put_front(&q_can_tx)                   │
│    → led_on/off                                      │
│                                                      │
│  task_protect (prio 4, unchanged)                    │
│    FAULT_COMM_LOSS no longer handled here            │
│                                                      │
│  can_cmd (modified)                                  │
│    + can_pub_build_fault()                           │
│    + can_pub_build_recovered()                       │
├──────────────────────────────────────────────────────┤
│                   BSP 层                             │
│                                                      │
│  bq76940 (modified)                                  │
│    + comm_timeout_cnt (static)                       │
│    + comm_is_faulted (static)                        │
│    + comm_timeout_track() → osSemaphoreRelease       │
│    + bq76940_is_faulted() getter                     │
│    fix: read_cells 错误路径 valid=0                  │
│    fix: reg_read/write 映射 I2C_SW_TIMEOUT           │
│                                                      │
│  i2c_sw (modified)                                   │
│    + I2C_SW_TIMEOUT (0x06)                           │
│    + i2c_send_byte: ACK 等待加循环超时               │
│    + i2c_recv_byte: 数据位等待加循环超时             │
└──────────────────────────────────────────────────────┘
```

**组件职责**：

| 组件 | 职责 | 依赖 |
|------|------|------|
| i2c_sw 超时 | 位带操作中超时检测，返回 TIMEOUT | systick (DWT 延时) |
| bq76940 计数 | 追踪连续超时，管理 comm_is_faulted 状态，操作 sem_iic | i2c_sw, FreeRTOS semaphore |
| task_iic_error | CAN 紧急上报 + LED 指示，不操作 I2C | can_cmd, ring_buf, led, bq76940_is_faulted |
| can_pub fault/recovered | 统一的故障/恢复 CAN 帧构建 | can_msg_t 结构体 |
| protection_check | 纯阈值检查，不检测 COMM_LOSS | bq76940_data_t, bms_settings_t |

### 数据流

```
任何在 mutex_iic 内的 I2C 操作:
  sample: bq76940_read_all()      ← I2C 读
  protect: bq76940_reg_write()    ← I2C 写 (FET/均衡/故障清除)
  balance: bq76940_set_balancing() ← I2C 写
  ...

  i2c_sw 层:
    i2c_send_byte / i2c_recv_byte
      └─ 超时? → return I2C_SW_TIMEOUT

  bq76940 层:
    reg_read/reg_write → 映射 I2C_SW_TIMEOUT → BQ76940_I2C_ERROR
    comm_timeout_track(BQ76940_I2C_ERROR)
      ├─ comm_timeout_cnt++
      ├─ if cnt>=2 && !faulted:
      │    comm_is_faulted=1
      │    osSemaphoreRelease(sem_iic)  ─────────────────┐
      └─ return                                          │
                                                         │
    comm_timeout_track(BQ76940_OK)                       │
      ├─ if faulted:                                     │
      │    comm_timeout_cnt=0, comm_is_faulted=0          │
      │    osSemaphoreRelease(sem_iic)  ─────────────────┤
      └─ else: comm_timeout_cnt=0                        │
                                                         │
         ┌───────────────────────────────────────────────┘
         ▼
  task_iic_error (prio 5, 抢占):
    osSemaphoreAcquire(sem_iic)
    if comm_is_faulted:
      can_pub_build_fault(FAULT_COMM_LOSS) → ring_buf_put_front → LED on
    else:
      can_pub_build_recovered() → ring_buf_put_front → LED off
```

### 关键接口

#### i2c_sw — 新增返回值

```c
typedef enum {
    I2C_SW_OK       = 0x00U,
    I2C_SW_ERROR    = 0x01U,
    I2C_SW_TIMEOUT  = 0x02U,  // already defined; added timeout loop to produce it
    // ... existing values unchanged
} i2c_sw_status_t;

#define I2C_SW_TIMEOUT_CYCLES  10000U  // NEW: ~500µs @ 72MHz
```

#### bq76940 — 新增状态查询

```c
// 查询当前通信故障状态（供 task_iic_error 使用）
uint8_t bq76940_is_faulted(void);  // 返回 comm_is_faulted

// 内部: 每次 I2C 操作后调用
static void comm_timeout_track(bq76940_status_t ret);
```

#### can_cmd — 新增故障帧构建接口

```c
// 构建故障 CAN 帧 (CAN ID=0x100, DLC=8)
void can_pub_build_fault(can_msg_t *frame, fault_code_t code,
                          const protection_ctx_t *ctx);

// 构建恢复 CAN 帧 (CAN ID=0x100, DLC=8, fault_code=0)
void can_pub_build_recovered(can_msg_t *frame);
```

#### bms_app — 新增任务和信号量

```c
// 同步原语 (新增)
extern osSemaphoreId_t sem_iic;

// 任务函数 (新增)
static void iic_error_task(void *arg);

// 任务属性
// .name = "iicErr", .stack_size = 256*4, .priority = osPriorityRealtime (48)
// 注: osPriorityRealtime 对应 CMSIS-RTOS 数值 48，高于 osPriorityHigh (24)
```

#### bms_app — ALL_FAULTS 宏更新

`ALL_FAULTS` 宏（定义于 `bms_app.h`）SHALL 排除 `FAULT_COMM_LOSS` (bit10):

```c
// ALL_FAULTS 排除 COMM_LOSS (bit10), 用于 evt_protect EventFlagsWait
#define ALL_FAULTS  0x0BFFU  // was 0x0FFF, excluding COMM_LOSS (bit10)
```

### 错误处理

| 错误路径 | 处理 |
|----------|------|
| i2c_sw 超时 | 返回 `I2C_SW_TIMEOUT` → 上层映射为 `BQ76940_I2C_ERROR` → `comm_timeout_track` 计数 |
| bq76940 CRC 错误 | 返回 `BQ76940_CRC_ERROR`，不计入超时计数（CRC 错误 ≠ 总线超时，是数据完整性故障） |
| task_iic_error CAN 队列满 | `ring_buf_put_front` 返回 false → 帧丢弃。24 帧深度在 100ms 周期下不会满 |
| sem_iic 多次 release | 二元信号量 max=1，第二次 release 被忽略（不会累积）——确保 task_iic_error 每轮只被唤醒一次 |
| I2C 恢复后立即又超时 | `comm_is_faulted` 在恢复时清零，下次超时重新计数 → 再次 release sem_iic → task_iic_error 再次上报 |
| 上电 BQ76940 不在线 | `bq76940_init()` 失败 → LED 红灯常亮；`bms_app_init` 在所有任务创建前完成 init，此时 sem_iic 尚未被任何任务等待，release 无效果 |

## 测试与验收策略

| Scenario / 检查项 | 维度 | 执行方式 | 验收证据 |
|-------------------|------|---------|---------|
| ACK 位超时 | unit | 任务内 TDD | `i2c_send_byte` 在 SDA 持续高时返回 `I2C_SW_TIMEOUT` |
| 数据位超时 | unit | 任务内 TDD | `i2c_recv_byte` 超时返回 |
| 正常通信不受影响 | unit | 任务内 TDD | 1000 次正常读写无超时误触发 |
| 连续 2 次超时触发 COMM_LOSS | integration | 任务内 TDD | `comm_is_faulted=1`, `sem_iic` released |
| 单次超时不触发 | integration | 任务内 TDD | `comm_is_faulted=0`, `sem_iic` not released |
| 通信恢复 | integration | 任务内 TDD | `comm_is_faulted=0`, recovery frame 入队 |
| 故障态持续超时不重复通知 | integration | 任务内 TDD | `sem_iic` 仅 release 一次（计数不变） |
| task_iic_error CAN 头插 | integration | 任务内 TDD | 故障帧在 `q_can_tx` 头部 |
| task_iic_error 不操作 I2C | integration | 任务内 TDD | 未获取 `mutex_iic` |
| protection_check 不设 COMM_LOSS | unit | 任务内 TDD | `active_faults` 不含 bit10 |
| bq76940_read_cells 错误路径 valid=0 | unit | 任务内 TDD | I2C 错误后 `cells.valid == 0` |
| I2C_SW_TIMEOUT 映射为 BQ76940_I2C_ERROR | unit | 任务内 TDD | `reg_read`/`reg_write` switch 覆盖新枚举值 |
| COMM_LOSS → 恢复 → 再次 COMM_LOSS 完整周期 | integration | 验收任务 (D) | 3 个周期 CAN trace 含 3 对 fault/recover 帧 |

## 风险与边缘情况

| 风险 | 缓解 |
|------|------|
| I2C 超时循环在 `bsp_delay_us` 依赖的 DWT 未初始化时死等 | i2c_sw_init 第一行调 `bsp_tick_init()`，确保 DWT 可用后才配置 GPIO |
| 位带超时循环可能被中断延迟拉长 | 超时循环使用递减计数器，不依赖绝对时间；中断延迟不改变循环退出条件 |
| sem_iic 在 bq76940 层直接操作 FreeRTOS API → BSP 层不再"纯" | bq76940 作为"厚 BSP"模块已是此项目的约定（已有 `bsp_tick_get` 调用）；sem_iic 依赖 FreeRTOS 是显式选择 |
| task_iic_error 优先级 5 高于 task_protect(4)，若 COMM_LOSS 频繁抖动会饥饿 task_protect | 二元信号量 max=1 防止累积唤醒；且 I2C 故障在正常工况下极少发生 |
| `I2C_SW_TIMEOUT_CYCLES = 10000` 在 72MHz 下约 500µs——对 BQ76940 的 100kHz I2C 是否合理 | BQ76940 最慢时钟拉伸 ≤ 50µs，10000 循环提供 ~10× 余量；实际调优可在集成测试中调整 |

## 开放问题

- `I2C_SW_TIMEOUT_CYCLES` 的精确值需在硬件实测后调优——当前 10000 为保守估计
- task_sample 栈从 2048→1536 是否足够（含 `comm_timeout_track` 调用链的栈帧）——编译器 `.su` 文件生成后确认

---

## 修改清单

| 文件 | 操作 | 改动量 |
|------|------|--------|
| `BSP/Inc/i2c_sw.h` | 修改 | 新增 `I2C_SW_TIMEOUT_CYCLES` 宏（`I2C_SW_TIMEOUT=0x02` 枚举值已存在） |
| `BSP/Src/i2c_sw.c` | 修改 | `i2c_send_byte` + `i2c_recv_byte` 加超时循环 |
| `BSP/Inc/bq76940.h` | 修改 | 新增 `bq76940_is_faulted()` 声明 |
| `BSP/Src/bq76940.c` | 修改 | 新增 `comm_timeout_track()` + 静态变量；`read_cells` 补 `valid=0`；`reg_read/write` switch 加 TIMEOUT 映射 |
| `App/Src/protection.c` | 修改 | 移除 COMM_LOSS 检测代码段（data.valid 检查） |
| `App/Inc/can_cmd.h` | 修改 | 新增 `can_pub_build_fault()` / `can_pub_build_recovered()` 声明 |
| `App/Src/can_cmd.c` | 修改 | 实现两个新接口 |
| `App/Inc/bms_app.h` | 修改 | `ALL_FAULTS` 宏更新 + 7 任务优先级表 + `sem_iic` 声明 |
| `App/Src/bms_app.c` | 修改 | 新增 task_iic_error 任务函数 + sem_iic 创建 + 优先级调整 |
| `.spec-dev/adr/0006-iic-error-task-priority.md` | **新增** | ADR 记录 |
