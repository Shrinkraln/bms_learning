---
spec_dev:
  feature: bms-fullstack-redesign
  covers:
    - Core/Src/main.c
    - Core/Src/freertos.c
    - Core/Src/stm32f1xx_it.c
    - Core/Inc/FreeRTOSConfig.h
    - Core/Inc/main.h
    - Core/Inc/gpio.h
    - Core/Src/gpio.c
    - BSP/Inc/systick.h
    - BSP/Src/systick.c
    - BSP/Inc/led.h
    - BSP/Src/led.c
    - BSP/Inc/io_ctrl.h
    - BSP/Src/io_ctrl.c
    - BSP/Inc/i2c_sw.h
    - BSP/Src/i2c_sw.c
    - BSP/Inc/bq76940.h
    - BSP/Src/bq76940.c
    - BSP/Inc/can_drv.h
    - BSP/Src/can_drv.c
    - BSP/Inc/timer.h
    - BSP/Src/timer.c
    - BSP/Inc/wdg.h
    - BSP/Src/wdg.c
    - BSP/Inc/ring_buf.h
    - BSP/Inc/bsp_common.h
    - App/Inc/bms_shared.h
    - App/Src/bms_shared.c
    - App/Inc/protection.h
    - App/Src/protection.c
    - App/Inc/soc_ocv.h
    - App/Src/soc_ocv.c
    - App/Inc/can_cmd.h
    - App/Src/can_cmd.c
    - App/Inc/bms_app.h
    - App/Src/bms_app.c
    - CMakeLists.txt
  status: active
---

# BMS 9S F103 全栈架构重新设计

## 目标

基于现有 CubeMX 硬件配置（STM32F103C8Tx, 72MHz, 20KB SRAM），从零重新设计 Core → BSP → App 三层软件架构，采用 6 任务事件驱动 FreeRTOS 模型，实现完整的电池监控、保护、SOC 估算、CAN 通信功能。

**不变量**: CubeMX 硬件配置 (`bms_9s_f103.ioc`) — 引脚、时钟、外设参数。

## 术语

| 术语 | 定义 | Avoid |
|------|------|-------|
| battery_val | `bms_shared_t.battery_val` (bq76940_data_t 结构体)，存放最新 BQ76940 采集数据（电压/电流/温度），受 mutex_data 保护 | 共享内存、全局变量 |
| protect 帧 | task_protect 产生的紧急 CAN 帧，头插入发送队列 | 告警帧、故障帧 |
| can_pub | CAN 帧发布函数，由各任务调用，将帧放入发送队列 | can_send, can_transmit |
| 头插 | ring_buf_put_front()，将数据写入环形缓冲区 tail 回退位置 | 优先级入队 |
| 任务编排 | bms_app.c 的任务函数，协调 App 策略模块输出与 BSP 硬件操作 | 调度、dispatch |

## 关键决策

| 决策 | 理由 | ADR |
|------|------|-----|
| 单 CAN 队列 + ring_buf 头插，不用双 osMessageQueue | 功能等价，减少一个 RTOS 对象，ring_buf 更轻量且不依赖 FreeRTOS 堆 | [ADR-0004](../../adr/0004-ringbuf-head-insert-can-queue.md) |
| protect 用 EventFlags 驱动 + osPriorityRealtime 抢占 | 故障时 protect 立即抢占所有任务，响应延迟 = 上下文切换时间 | [ADR-0005](../../adr/0005-eventflags-protect-preemption.md) |
| 删除 USART，CAN 兼做调试和数据上报 | CubeMX 已移除 USART1/USART2；CAN 总线是唯一对外通信通道 | — |
| 删除 data_report 模块，统一为 can_cmd | CAN ID 冲突，data_report 函数零调用者 | [ADR-0001](../../adr/0001-delete-data-report-module.md) |
| App 模块禁调 BSP，硬件操作由任务函数中转 | 分层纪律，App 可脱离硬件做 host 单元测试 | [ADR-0002](../../adr/0002-task-function-bsp-mediation.md) |
| 保护阈值从运行时配置读取 | CAN 配置命令写入后需实际生效 | [ADR-0003](../../adr/0003-runtime-config-protection-thresholds.md) |
| TIM2 回调通过 main.c HAL_TIM_PeriodElapsedCallback 分发 | 避免 HAL weak 函数冲突（TIM1 已被占用） | — |

## 架构图

```
Layer 0: Hardware ─────────────────────────────────────────
  STM32F103C8Tx (72MHz, 64KB FLASH, 20KB SRAM)
  BQ76940 (I2C 100kHz, CRC8)
  CAN Transceiver (PA11/PA12, 500kbps)

Layer 1: Core (CubeMX 生成 + USER CODE 手动维护) ─────────
  main.c / freertos.c / stm32f1xx_it.c / FreeRTOSConfig.h
  gpio.c/h / can.c/h / i2c.c/h / iwdg.c/h / tim.c/h

Layer 2: BSP (板级硬件抽象, 9 模块) ──────────────────────
  bsp_common.h / ring_buf.h    ← 新增公共模块
  systick / led / io_ctrl / i2c_sw / bq76940 / can_drv / timer / wdg

Layer 3: App (纯策略/算法/协议, 5 模块) ──────────────────
  bms_shared / protection / soc_ocv / can_cmd / bms_app
```

## 任务架构

| 任务 | CMSIS-RTOS 优先级 | 周期 | 栈 | 触发方式 |
|------|------------------|------|-----|---------|
| **task_protect** | `osPriorityRealtime` (48) | 事件驱动 | 1024B | `osEventFlagsWait(evt_protect)` |
| **task_sample** | `osPriorityAboveNormal` (32) | 100ms | 2048B | `osSemaphoreAcquire(sem_sample)` ← TIM2 ISR give |
| **task_can_rx** | `osPriorityNormal` (24) | 50ms | 1024B | `osDelay(50)` 轮询 CAN RX FIFO |
| **task_balance** | `osPriorityBelowNormal` (16) | 500ms | 1024B | 首次阻塞等 DATA_READY, 之后 osDelay(500) |
| **task_soc** | `osPriorityBelowNormal` (16) | 1000ms | 2048B | 首次阻塞等 DATA_READY, 之后 osDelay(1000) |
| **task_can_tx** | `osPriorityLow` (8) | 100ms | 1024B | `osDelay(100)` |

## 同步原语

| 名称 | 类型 | 用途 |
|------|------|------|
| `sem_sample` | `osSemaphore` (Binary, max=1) | TIM2 ISR —(give)→ task_sample —(take)→ 采样 |
| `mutex_data` | `osMutex` (PrioInherit) | 保护 `batteryval[]` 全局数组 |
| `mutex_settings` | `osMutex` (PrioInherit) | 保护 `bms_settings_t` 可配置参数 |
| `evt_protect` | `osEventFlags` (12 bit) | 故障标志位 (每 bit 对应 fault_code_t 的一种故障) |

### evt_protect 故障位映射

| Bit | 故障码 | 触发条件 |
|-----|--------|---------|
| 0 | `FAULT_CELL_OV` (1<<0) | 任一电芯 > settings.cell_ov_mv |
| 1 | `FAULT_CELL_UV` (1<<1) | 任一电芯 < settings.cell_uv_mv |
| 2 | `FAULT_PACK_OV` (1<<2) | pack_mv > settings.pack_ov_mv |
| 3 | `FAULT_PACK_UV` (1<<3) | pack_mv < settings.pack_uv_mv |
| 4 | `FAULT_DISCHARGE_OC` (1<<4) | 放电电流 > settings.discharge_oc_ma |
| 5 | `FAULT_CHARGE_OC` (1<<5) | 充电电流 > settings.charge_oc_ma |
| 6 | `FAULT_SHORT_CIRCUIT` (1<<6) | 电流 > settings.short_circuit_ma |
| 7 | `FAULT_OVER_TEMP` (1<<7) | 任一温度 > settings.over_temp_mdeg |
| 8 | `FAULT_UNDER_TEMP` (1<<8) | 任一温度 < settings.under_temp_mdeg |
| 9 | `FAULT_CELL_IMBALANCE` (1<<9) | (max_cell - min_cell) > settings.cell_diff_max_mv |
| 10 | `FAULT_COMM_LOSS` (1<<10) | 连续 3 次 bq76940_read_all() 失败 |
| 11 | `FAULT_WATCHDOG` (1<<11) | 启动时 wdg_is_reset_source() 检测到 |

`ALL_FAULTS` 宏定义为 `0x0FFFU` (bit0-11 全掩码)，定义于 `bms_app.h`。
| `evt_data_ready` | `osEventFlags` (1 bit) | 首次数据就绪，通知 balance/soc |
| `q_can_tx` | `ring_buf_t` (自定义, 静态数组, 深度 24, 元素=can_msg_t 16B) | CAN TX 帧队列，支持 `put_front`(头插) 和 `put`(尾插)，满时 `put` 尾丢弃、`put_front` 头丢弃 |

## 数据流

```
TIM2 ISR (100ms, prio=5, ISR-safe)
  │ osSemaphoreRelease(sem_sample)                        ← 从 ISR 释放信号量
  ▼
task_sample (prio 32)
  osSemaphoreAcquire(sem_sample)                           ← 阻塞等待
  osMutexAcquire(mutex_data)
  ├─ bq76940_read_all(&data) ──I2C──▶ BQ76940
  ├─ if ret != OK:
  │    comm_err_cnt++
  │    if comm_err_cnt >= 3:
  │        osEventFlagsSet(evt_protect, FAULT_COMM_LOSS)   ← 连续 3 次失败触发
  │    osMutexRelease(mutex_data)
  │    goto wait_next
  ├─ comm_err_cnt = 0
  ├─ 写入 batteryval[] 全局数组
  ├─ osMutexAcquire(mutex_settings)                        ← 读 settings 需持锁
  │   遍历检查阈值 (对照 settings):
  │     如有异常 → osEventFlagsSet(evt_protect, fault_bit)
  ├─ osMutexRelease(mutex_settings)
  ├─ 首次完成 → osEventFlagsSet(evt_data_ready, 0x01)    ← 解锁 balance/soc
  └─ osMutexRelease(mutex_data)
  wait_next:
  [等待下一个 semaphore]

task_protect (prio 48, 最高优先级)
  osEventFlagsWait(evt_protect, ALL_FAULTS)               ← 阻塞, 被 set 后抢占当前任务
  osMutexAcquire(mutex_data, osWaitForever)                ← 使用 osWaitForever 避免超时跳过保护
  ├─ 读 batteryval[] 确定故障类型
  ├─ 执行保护动作
  ├─ 生成 protect CAN 帧 (ID=0x100)
  └─ osMutexRelease(mutex_data)
  ring_buf_put_front(&q_can_tx, &protect_frame)            ← 头插, 下一次优先发送
  [回到 EventFlagsWait 阻塞]

task_can_tx (prio 8)
  osDelay(100)
  wdg_kick()                                               ← 喂狗 (最低优先级任务兼)
  can_pub() → 生成周期上报帧 (~4 帧/周期)
  ring_buf_put(&q_can_tx, &periodic_frame)                 ← 尾插, 满时自动丢旧
  while (ring_buf_available(&q_can_tx)):
      ring_buf_get(&q_can_tx, &frame)                      ← 自然按优先级出队
      can_send(&frame)                                     ← 发送失败不重试, 丢帧继续
  osDelay(100)

task_can_rx (prio 24)
  osDelay(50)
  while can_available():
      can_recv(&frame)
      0x200 查询 → 立即回传指定数据
      0x201 控制 → FET/均衡/清除故障/关机
      0x202 配置 → 更新 settings (需 mutex_settings)

task_balance (prio 16)          task_soc (prio 16)
  首次: EventFlagsWait(DATA_READY, 5000ms)  ← 5s 超时, 到期不管有无数据都开始运行
  首次: EventFlagsWait(DATA_READY, 5000ms)
  之后每 500ms:                      之后每 1000ms:
   osMutexAcquire(mutex_data, 10ms)   osMutexAcquire(mutex_data, 10ms)
   if acquired:
     → 均衡策略 (先检查 prot_level   → 安时积分+OCV
       非 FAULT 才执行)               → 更新 SOC
     → bq76940_set_balancing()        osMutexRelease(mutex_data)
     osMutexRelease(mutex_data)
   osDelay(500)                      osDelay(1000)
```

## CAN 协议

### 上行 (MCU → 上位机)

| CAN ID | 名称 | 来源 | 周期/触发 | 入队方式 |
|--------|------|------|----------|---------|
| **0x100** | BMS_FAULT | task_protect | 事件驱动 | **头插** (ring_buf_put_front) |
| 0x110 | CELL_VOLT_1_4 | can_pub | 100ms | 尾插 (ring_buf_put) |
| 0x111 | CELL_VOLT_5_9 | can_pub | 100ms | 尾插 |
| 0x120 | BMS_STATUS | can_pub | 100ms | 尾插 |
| 0x130 | SOC_OCV | can_pub | 1000ms | 尾插 |

> 0x100 为最小 CAN ID → 总线硬件仲裁优先级最高，配合 ring_buf 头插实现软件+硬件双重优先

### 下行 (上位机 → MCU)

| CAN ID | 名称 | 处理 |
|--------|------|------|
| 0x200 | QUERY | 请求立即回传指定数据类型 |
| 0x201 | CONTROL | FET 控制 / 均衡设置 / 清除锁存故障 / 系统关机 |
| 0x202 | CONFIG | 修改保护阈值 (需 mutex_settings, 掉电不保存) |

### 0x100 BMS_FAULT (DLC=8)

| Byte | 字段 |
|------|------|
| 0 | 故障等级 (0=NONE, 1=WARNING, 2=ALERT, 3=FAULT) |
| 1-2 | 活跃故障位掩码 (uint16, LSB first) |
| 3 | 最高电芯电压 (mV/10, 0-255 对应 0-2550mV) |
| 4 | 最低电芯电压 (mV/10, 0-255 对应 0-2550mV) |
| 5-6 | 电流 (int16, mA/10, LSB first) |
| 7 | 最高温度 (signed, °C offset +40) |

### 0x120 BMS_STATUS (DLC=8)

| Byte | 字段 |
|------|------|
| 0 | SOC (%, 0-100) |
| 1-2 | 总压 (mV, uint16, LSB first) |
| 3-4 | 电流 (int16, mA/10, LSB first) |
| 5 | FET 状态 (0=全关, 1=充电开, 2=放电开, 3=全开) |
| 6 | 均衡掩码高字节 |
| 7 | 保留 |

---

## ADDED Requirements

### ADDED: ring_buf 通用环形缓冲区（元素级）

系统 SHALL 提供 `BSP/Inc/ring_buf.h`，定义 `ring_buf_t` 结构体及 `ring_buf_init` / `ring_buf_put` / `ring_buf_put_front` / `ring_buf_get` / `ring_buf_available` 操作。缓冲区以固定大小元素为操作单位（`elem_size` 在 init 时设定）。满时 `put` 丢弃最旧元素（tail 前进）、`put_front` 丢弃最新元素（head 位置不变）。

```c
typedef struct {
    uint8_t *buf;          // 静态数组指针
    uint16_t elem_size;    // 每个元素大小 (字节)
    uint16_t capacity;     // 最大元素数
    uint16_t head;         // 写入位置 (元素索引)
    uint16_t tail;         // 读取位置 (元素索引)
    uint16_t count;        // 当前元素数
} ring_buf_t;

void     ring_buf_init(ring_buf_t *rb, uint8_t *buf, uint16_t elem_size, uint16_t capacity);
uint16_t ring_buf_available(const ring_buf_t *rb);
uint8_t  ring_buf_put(ring_buf_t *rb, const void *elem);        // 尾插, 0=OK, 1=满(尾丢弃)
uint8_t  ring_buf_put_front(ring_buf_t *rb, const void *elem);  // 头插, 0=OK, 1=满(头丢弃)
uint8_t  ring_buf_get(ring_buf_t *rb, void *elem);              // 取队首, 0=OK, 1=空
```

`put` 满时行为: 丢弃 tail 位置的旧元素，tail 前进，写入新元素到 head。
`put_front` 满时行为: head 不动，覆盖其前一个位置（逻辑队首）。

#### Scenario: protect 帧头插后优先出队
- GIVEN q_can_tx 中有 2 帧周期数据 (can_msg_t, 各 16B, ID=0x110, 0x111)
- WHEN task_protect 调用 ring_buf_put_front(&q_can_tx, &fault_frame)
- THEN 下一次 ring_buf_get 返回 protect 帧 (ID=0x100)，随后依次返回 0x110、0x111

#### Scenario: 队列满时尾插覆盖最旧
- GIVEN q_can_tx 已满 (24 帧)
- WHEN can_pub 调用 ring_buf_put() 尾插新帧
- THEN 最旧的 1 帧被丢弃 (tail+1)，新帧写入 head 位置，返回 1 (指示发生过丢弃)

### ADDED: 6 任务 FreeRTOS 架构

系统 SHALL 创建 6 个 FreeRTOS 任务——task_sample、task_protect、task_can_rx、task_balance、task_soc、task_can_tx——并配置指定优先级和同步原语。任务创建代码 SHALL 位于 `bms_app_init()` 中。

#### Scenario: 全部任务正常创建
- GIVEN FreeRTOS 堆为 12288 字节
- WHEN bms_app_init() 依次创建 6 个任务 + 4 个同步原语 + 1 个 ring_buf
- THEN 所有 osThreadNew/sync create 返回非 NULL，调度器启动后全部任务进入就绪/阻塞状态

#### Scenario: protect 抢占 task_sample
- GIVEN task_sample 持有 mutex_data 正在读取 I2C
- WHEN task_sample 调用 osEventFlagsSet(evt_protect, FAULT_CELL_OV)
- AND task_protect (prio 48) 从 osEventFlagsWait 被唤醒
- THEN task_protect 立即抢占 task_sample，在 task_sample 释放 mutex 之前即开始执行

#### Scenario: protect 帧头插队列
- GIVEN task_can_tx 即将在下个周期发送队列中的帧
- WHEN task_protect 调用 ring_buf_put_front() 插入 protect 帧
- THEN task_can_tx 下一次 ring_buf_get() 首先获取 protect 帧并发送

### ADDED: TIM2 ISR → Binary Semaphore 采样触发

系统 SHALL 在 TIM2 中断服务函数 (100ms 周期) 中调用 `osSemaphoreRelease(sem_sample)` 唤醒 task_sample。TIM2 NVIC 优先级 SHALL 为 5 (= `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`)，确保 ISR 可安全调用 FreeRTOS API。

#### Scenario: 100ms 采样周期
- GIVEN TIM2 配置为 10Hz (PSC=7200-1, ARR=1000-1)
- WHEN TIM2 每次溢出触发 ISR
- THEN osSemaphoreRelease 使 task_sample 从阻塞返回，执行一次完整采样

#### Scenario: ISR 优先级安全
- GIVEN TIM2 NVIC prio = 5, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
- WHEN TIM2 ISR 调用 osSemaphoreRelease
- THEN 不触发 FreeRTOS assert (configASSERT 检查通过)

### ADDED: bsp_common.h 通用状态枚举

系统 SHALL 提供 `BSP/Inc/bsp_common.h`，定义 `bsp_status_t` 枚举 (OK/ERROR/BUSY/TIMEOUT)。各 BSP 模块 SHALL typedef 各自的类型别名。

### ADDED: bms_settings_t 覆盖全部保护阈值

`bms_settings_t` SHALL 包含: `cell_ov_mv`, `cell_uv_mv`, `pack_ov_mv`, `pack_uv_mv`, `discharge_oc_ma`, `charge_oc_ma`, `short_circuit_ma`, `over_temp_mdeg`, `under_temp_mdeg`, `cell_diff_max_mv`, `balance_thresh_mv`, `balance_min_mv`。

### ADDED: protection_check 读运行时配置

`protection_check()` SHALL 从 `bms_settings_t` 读取阈值（而非编译期 `#define`），签名为:
```c
prot_result_t protection_check(const bq76940_data_t *data,
                                const bms_settings_t *settings);
```

### ADDED: protection_check 返回 prot_result_t

`protection_check()` SHALL 返回 `prot_result_t` 结构体，包含 `level` (保护等级)、`active_faults` (活跃故障位掩码)、`request_power_off` (需断电标志)。函数内部 SHALL NOT 调用任何 BSP 函数。

### ADDED: can_cmd_dispatch 返回 can_action_req_t

`can_cmd_dispatch()` SHALL 返回 `can_action_req_t`，不在函数内部执行 BSP 调用。任务层的 switch 语句 SHALL 覆盖 `can_action_t` 的所有枚举值。

### ADDED: 故障处理器 .noinit 段诊断

系统 SHALL 在 HardFault/MemManage/BusFault/UsageFault 触发时，将 SCB 寄存器 (CFSR, HFSR, MMFAR, BFAR) + 签名 0xDEADBEEF 保存到 `.noinit` 段静态变量，复位后启动代码可读取。

---

## MODIFIED Requirements

### MODIFIED: FreeRTOSConfig.h 堆大小

系统 SHALL 使用 `configTOTAL_HEAP_SIZE = 12288`（原 3072）。

### MODIFIED: CubeMX 引脚配置

系统 SHALL 在 CubeMX 中仅配置:
- PA8: MCU_WAKE_BQ (Output PP) — BQ76940 TS1 启动唤醒
- PA15: LED_0_ON (Output PP) — 单 LED 状态指示
- PB2: Boot1 (Input, no pull) — MCU 启动模式引脚

原配置中的 PA0/PA1/PB1/PB12/PC13/PB0 等 SHALL 全部移除。

### MODIFIED: io_ctrl 精简为单引脚

`io_ctrl.h/c` SHALL 仅管理 PA8 (MCU_WAKE_BQ) 一个输出引脚。移除原 6 路 IO 的枚举和 `io_ctrl_power_off_all()` 函数。

#### Scenario: BQ76940 启动唤醒
- GIVEN BQ76940 处于 SHIP 模式或刚上电
- WHEN 调用 io_ctrl_set(IO_ID_WAKE_BQ, IO_LEVEL_HIGH) → bsp_delay_us(100) → io_ctrl_set(LOW)
- THEN TS1 引脚产生上升沿脉冲，BQ76940 退出 SHIP 模式进入 NORMAL 模式

### MODIFIED: LED 改为 PA15 单 LED

`led.h/c` SHALL 改为单 LED (PA15)。移除原双 LED 枚举 (LED_ID_1/LED_ID_2) 和 LED_BUSY/LED_TIMEOUT 状态。API 简化为 `led_init()/led_on()/led_off()/led_toggle()`。

### MODIFIED: bq76940 移除 BQ_POWER 和 ALERT 依赖

`bq76940.h/c` SHALL 移除 `io_ctrl.h` 依赖（不再控制 BQ_POWER/ALERT）。BQ76940 始终由电池供电，MCU 仅通过 PA8 TS1 脉冲唤醒芯片。

### MODIFIED: task_sample 加阈值预检

task_sample SHALL 在 bq76940_read_all() 后遍历数据: 如有异常（电压/电流/温度超出阈值）→ `osEventFlagsSet(evt_protect, fault_bit)` 触发 protect。

### MODIFIED: task_protect 改为事件驱动

task_protect SHALL 以 `osEventFlagsWait(evt_protect, ...)` 阻塞，不再周期性轮询。被唤醒后执行保护动作，生成 protect CAN 帧并头插入 q_can_tx。

### MODIFIED: 头文件卫命名

所有 BSP 和 App 头文件卫 SHALL 使用 `BSP_XXX_H` / `APP_XXX_H` 格式（C11 7.1.3 合规），移除前导双下划线。

---

## REMOVED Requirements

### REMOVED: usart_drv 模块

`BSP/Inc/usart_drv.h` 和 `BSP/Src/usart_drv.c` SHALL 被删除。USART1/USART2 已从 CubeMX 移除，CAN 总线是唯一对外通信通道。

### REMOVED: data_report 模块

`App/Inc/data_report.h` 和 `App/Src/data_report.c` SHALL 被删除。CAN 数据上报统一由 `can_cmd` + `task_can_tx` 负责。

### REMOVED: defaultTask 空任务

`freertos.c` 中的 `defaultTask`（仅 `osDelay(1000)` 空循环）SHALL 被移除，释放栈空间 512 字节。

---

## BSP 模块接口

### systick

```c
void     bsp_tick_init(void);
uint32_t bsp_tick_get(void);             // ms 时间戳
void     bsp_delay_us(uint32_t us);      // DWT µs 延时
uint32_t bsp_tick_elapsed(uint32_t start);
uint8_t  bsp_tick_is_timeout(uint32_t start, uint32_t timeout_ms);
```

### led

```c
void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
```

### io_ctrl

```c
typedef enum { IO_ID_WAKE_BQ = 0U } io_ctrl_pin_t;
typedef enum { IO_LEVEL_LOW = 0U, IO_LEVEL_HIGH = 1U } io_level_t;

io_ctrl_status_t io_ctrl_init(void);
io_ctrl_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level);
```

### i2c_sw

```c
// 引脚: PB10(SCL), PB11(SDA), 开漏, ~100kHz
// CRC8 多项式: 0x07 (x^8 + x^2 + x + 1)

void i2c_sw_init(void);
i2c_sw_status_t i2c_sw_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data);
i2c_sw_status_t i2c_sw_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *data);
i2c_sw_status_t i2c_sw_write_buf(uint8_t dev_addr, uint8_t reg, const uint8_t *buf, uint8_t len);
i2c_sw_status_t i2c_sw_read_buf(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint8_t len);
i2c_sw_status_t i2c_sw_read_crc(uint8_t dev_addr, uint8_t reg, uint8_t *data);  // 带 CRC8 验证
uint8_t i2c_sw_crc8(const uint8_t *data, uint8_t len);
```

### bq76940

```c
// I2C 地址: 0x08 (7-bit), BQ7694001/03 (CRC 版本)
// 主要数据结构
typedef struct {
    uint16_t cells_mv[9];         // 9 电芯电压 (mV)
    uint16_t cells_min_mv, cells_max_mv;
    uint16_t pack_mv;             // 总压 (mV)
    int32_t  current_ma;          // 电流 (mA)
    int16_t  ts_mdeg_c[3];        // 3 路温度 (0.1°C)
    uint8_t  fault_bits;          // BQ 硬件故障位
    uint32_t timestamp_ms;
    uint8_t  valid;               // CRC 校验通过标记
} bq76940_data_t;

bq76940_status_t bq76940_init(const bq76940_cfg_t *cfg);
bq76940_status_t bq76940_read_all(bq76940_data_t *data);
bq76940_status_t bq76940_set_balancing(uint16_t mask);      // CELLBAL1/2
bq76940_status_t bq76940_set_protection(const bq76940_cfg_t *cfg);
bq76940_status_t bq76940_clear_faults(void);
```

### can_drv

```c
// CAN1, 500kbps, FIFO0 中断接收
typedef struct { uint32_t id; uint8_t len; uint8_t data[8]; } can_msg_t;

void can_drv_init(void);
uint8_t can_send(const can_msg_t *msg);                      // 阻塞, 超时 50ms
uint8_t can_recv(can_msg_t *msg);                            // 从 FIFO 取一帧
uint8_t can_available(void);                                  // FIFO 中可用帧数
```

### timer

```c
// TIM2, 1kHz, 中断分发至 main.c
void timer_init(void);
void bsp_timer_irq_handler(TIM_HandleTypeDef *htim);         // 在 main.c HAL_TIM_PeriodElapsedCallback 中调用
void timer_set_callback(void (*cb)(void));                    // 注册周期性回调
```

### wdg

```c
// IWDG, LSI 40kHz
void wdg_init(uint32_t timeout_ms);
void wdg_kick(void);
uint8_t wdg_is_reset_source(void);                           // 检测是否为看门狗复位
```

---

## App 模块接口

### bms_shared

```c
typedef struct {
    bq76940_data_t  battery_val;       // 最新采集数据
    prot_level_t    prot_level;        // 当前保护等级
    uint16_t        active_faults;     // 活跃故障掩码
    uint16_t        latched_faults;    // 锁存故障掩码
    uint16_t        soc_permil;        // SOC (0-1000)
    uint16_t        ocv_mv;           // OCV 估算 (mV)
    int32_t         remaining_mah;     // 剩余容量
    bms_settings_t  settings;          // 可配置参数
} bms_shared_t;

void bms_shared_init(void);
bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms);
void bms_shared_data_unlock(void);
bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms);
void bms_shared_settings_unlock(void);
void bms_shared_update_bq_data(const bq76940_data_t *data);
void bms_shared_update_protection(prot_level_t level, uint16_t faults);
void bms_shared_update_soc(uint16_t soc_permil, uint16_t ocv_mv, int32_t remaining_mah);
```

### protection

```c
typedef enum { PROT_LVL_NONE=0, PROT_LVL_WARNING=1, PROT_LVL_ALERT=2, PROT_LVL_FAULT=3 } prot_level_t;

typedef enum {
    FAULT_CELL_OV=(1<<0), FAULT_CELL_UV=(1<<1), FAULT_PACK_OV=(1<<2),
    FAULT_PACK_UV=(1<<3), FAULT_DISCHARGE_OC=(1<<4), FAULT_CHARGE_OC=(1<<5),
    FAULT_SHORT_CIRCUIT=(1<<6), FAULT_OVER_TEMP=(1<<7), FAULT_UNDER_TEMP=(1<<8),
    FAULT_CELL_IMBALANCE=(1<<9), FAULT_COMM_LOSS=(1<<10), FAULT_WATCHDOG=(1<<11),
} fault_code_t;

typedef struct {
    prot_level_t level;
    uint16_t     active_faults;
    bool         request_power_off;     // 需立即断电
} prot_result_t;

void protection_init(void);
prot_result_t protection_check(const bq76940_data_t *data, const bms_settings_t *settings);
const char *protection_fault_str(fault_code_t code);
```

### soc_ocv

```c
void soc_ocv_init(int32_t nominal_mah);  // 0 = 默认 20000mAh
void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma,
                     int16_t temp_mdeg, uint32_t dt_ms);
uint16_t soc_ocv_get_soc(void);           // 0-1000
uint16_t soc_ocv_get_ocv(void);           // mV
int32_t  soc_ocv_get_remaining_mah(void);
```

### can_cmd

```c
typedef enum {
    CAN_ACTION_NONE=0, CAN_ACTION_CLEAR_FAULT,
    CAN_ACTION_FET_CHG_ON, CAN_ACTION_FET_CHG_OFF,
    CAN_ACTION_FET_DSG_ON, CAN_ACTION_FET_DSG_OFF,
    CAN_ACTION_BALANCE_SET, CAN_ACTION_BALANCE_OFF,
    CAN_ACTION_SHUTDOWN,
} can_action_t;

typedef struct { can_action_t action; uint16_t balance_mask; } can_action_req_t;

void can_cmd_init(void);
can_action_req_t can_cmd_dispatch(const can_msg_t *msg, uint16_t active_faults);
void can_pub(const bms_shared_t *bms, can_msg_t *frame);  // 生成周期上报帧
```

### bms_app

```c
void bms_app_init(void);  // 初始化全部模块 + 创建 6 个任务 + 5 个同步原语 + 1 ring_buf
```

## 内存预算

| 项目 | 大小 | 备注 |
|------|------|------|
| FreeRTOS 堆 | 12288 B | configTOTAL_HEAP_SIZE |
| **任务栈** | | |
| task_sample (2048B) + task_soc (2048B) | 4096 B | 含 I2C 栈帧 + 浮点运算 |
| task_protect (1024B) + task_balance (1024B) | 2048 B | |
| task_can_rx (1024B) + task_can_tx (1024B) | 2048 B | |
| **RTOS 控制块** | | |
| 6 × TCB (每任务 ~120B) | ~720 B | FreeRTOS + CMSIS-RTOS2 |
| 2 × EventGroups | ~200 B | evt_protect + evt_data_ready |
| 2 × Mutex | ~160 B | mutex_data + mutex_settings |
| 1 × Semaphore | ~80 B | sem_sample |
| **内核开销** | | |
| FreeRTOS 定时器任务栈 (256w) + TCB | ~1200 B | configUSE_TIMERS=1 |
| 空闲任务栈 (128w) + TCB | ~600 B | |
| 堆管理开销 (heap_4 BlockLink_t) | ~400 B | |
| **堆使用合计** | **~11.5 KB** | 余量 ~0.8 KB |
| **静态数据** (.data+.bss) | | |
| q_can_tx 静态数组 (24 × 16B) | 384 B | ring_buf 静态分配 |
| bms_shared_t 全局实例 | ~128 B | |
| CSTACK (系统栈) | 1024 B | 链接器脚本分配 |
| 其他全局 + HAL + CubeMX | ~3.5 KB | |
| .noinit 故障诊断 (5×uint32_t) | 20 B | |
| **静态数据合计** | **~5.0 KB** | |
| **SRAM 总计** | **~16.5 KB / 20 KB** | 余量 ~3.5 KB |

## 风险

| 风险 | 缓解 |
|------|------|
| TIM2 NVIC prio=5 刚好等于 MAX_SYSCALL 阈值，future CubeMX 改 NVIC 分组可能导致溢出 | 在 spec 和代码注释中标注此约束，CubeMX regenerate 后检查 |
| PB10/PB11 不在 CubeMX 配置中，依赖 GPIOB 时钟由 PB2 间接使能 | 在 i2c_sw_init() 中显式检查并手动使能 GPIOB 时钟 |
| ring_buf_put_front 无原子性保护，ISR 与任务并发写入可能竞态 | ring_buf 仅由任务上下文写入（ISR 不写），无需额外保护 |
| 单 CAN 队列满时 protect 帧丢弃 | 深度 24 帧，正常工况下远不会满（100ms 周期产生 ~5 帧/周期） |

## 不改的内容

| 模块 | 理由 |
|------|------|
| soc_ocv 算法核心 | 纯算法零依赖，已是分层标杆（仅修 calc_correction_weight 亚秒精度问题） |
| i2c_sw CRC8 + 寄存器级操作 | 已验证可行，性能足够 |
| systick DWT µs 延时 | 依赖链最小，无需改动 |
| CAN NVIC 优先级配置 | CubeMX 配置保持不变，需在 regenerate 后手动检查 |

---

## 修改清单

| 文件 | 操作 | 改动量 |
|------|------|--------|
| `Core/Inc/FreeRTOSConfig.h` | 确认 | heap=12288 (已更新) |
| `Core/Src/main.c` | 修改 | 移除 USART init，适配新 bms_app_init |
| `Core/Src/freertos.c` | 修改 | 删除 defaultTask |
| `Core/Src/stm32f1xx_it.c` | 修改 | TIM2 ISR 添加 semaphore give |
| `CMakeLists.txt` | 修改 | 移除 usart_drv 和 data_report |
| `BSP/Inc/ring_buf.h` † | **新增** | ~25 行 |
| `BSP/Inc/bsp_common.h` † | **新增** | ~12 行 |
| `BSP/Inc/led.h`, `BSP/Src/led.c` | 重写 | PA15 单 LED |
| `BSP/Inc/io_ctrl.h`, `BSP/Src/io_ctrl.c` | 重写 | 精简为 PA8 单引脚 |
| `BSP/Inc/bq76940.h`, `BSP/Src/bq76940.c` | 修改 | 移除 io_ctrl 依赖, API 简化 |
| `BSP/Inc/usart_drv.h`, `BSP/Src/usart_drv.c` | **删除** | — |
| `BSP/Inc/can_drv.h`, `BSP/Src/can_drv.c` | 修改 | 采用 ring_buf |
| `BSP/Inc/i2c_sw.h` | 修改 | 确保 PB10/PB11 时钟使能 |
| `App/Inc/data_report.h`, `App/Src/data_report.c` | **删除** | — |
| `App/Inc/protection.h`, `App/Src/protection.c` | 重写 | prot_result_t 签名 |
| `App/Inc/can_cmd.h`, `App/Src/can_cmd.c` | 重写 | can_action_req_t + can_pub |
| `App/Inc/bms_shared.h`, `App/Src/bms_shared.c` | 重写 | batteryval[] + 双 mutex |
| `App/Inc/soc_ocv.h`, `App/Src/soc_ocv.c` | 保留 | 接口不变，修精度 bug |
| `App/Inc/bms_app.h`, `App/Src/bms_app.c` | 重写 | 6 任务 + 新同步原语 |
| 头文件卫 (≤14 个文件) | 修改 | `__XXX_` → `XXX_` |

† ring_buf 和 bsp_common 为纯头文件（inline 实现），不需要 .c 文件。

## 资源

| 指标 | 当前 | 目标 |
|------|------|------|
| FLASH | ~26.7 KB | < 28 KB |
| RAM (含堆) | ~9.3 KB | ~17 KB |
| 分层违规 | 4 处 | 0 处 |
| BSP 模块 | 9 | 9（删 usart_drv, 加 ring_buf+bsp_common） |
| App 模块 | 6 (1 死代码) | 5 (活跃) |
| FreeRTOS 任务 | 6 | 6 (完全不同的事件驱动架构) |
| 同步原语 | 2 | 5 (sem+2mutex+2evt+ringbuf) |
