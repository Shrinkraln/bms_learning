# 任务规划：BMS 9S F103 电池管理系统开发

## 目标
基于 STM32F103 + FreeRTOS + BQ76940 的 9 串电池管理系统（BMS）固件开发。
**当前阶段**: 按 active spec `bms-fullstack-redesign` 重写代码，实现事件驱动的 6 任务架构。

## 设计基准

- **Active Spec**: `.spec-dev/2026-07-26-bms-fullstack-redesign/spec/bms-fullstack-redesign-design.md`
- **旧 Spec (已废弃)**: `.spec-dev/2026-07-22-arch-refactor/spec/arch-refactor-design.md` (status: draft)
- **ADR 记录**: `.spec-dev/adr/0001` ~ `0005`

## 全栈层次架构

```
Layer 0: Hardware
  STM32F103C8Tx (72MHz, 64KB FLASH, 20KB SRAM)
  BQ76940 (I2C 100kHz, CRC8, 地址 0x08)
  CAN Transceiver (PA11/PA12, 500kbps)

Layer 1: Core 层 (CubeMX 骨架 + USER CODE)
  main.c              — 系统入口, HAL 初始化, bms_app_init(), osKernelStart()
  freertos.c          — FreeRTOS 内核初始化 (MX_FREERTOS_Init)
  stm32f1xx_it.c      — 中断服务 (CAN/TIM/I2C/SysTick/HardFault)
  FreeRTOSConfig.h    — 内核配置 (堆 12288B)
  gpio.c/h, can.c/h, i2c.c/h, iwdg.c/h, tim.c/h
                      — CubeMX 生成的外设 HAL 配置

Layer 2: BSP 层 (板级硬件抽象 — 9 模块, 纯硬件, DAG 无环)
  bsp_common.h (新增) — 通用状态枚举 bsp_status_t
  ring_buf.h   (新增) — 元素级环形缓冲区 + put_front 头插
  systick.h/c         — DWT 微秒延时 + 毫秒时间戳
  led.h/c      (重写) — PA15 单 LED
  io_ctrl.h/c  (重写) — PA8 WAKE_BQ 单引脚
  i2c_sw.h/c          — 软件 I2C (PB10/PB11) + CRC8
  bq76940.h/c  (修改) — 移除 io_ctrl 依赖, 补 OCD/SCD
  can_drv.h/c  (修改) — 复用 ring_buf, 阻塞发送+信号量
  timer.h/c           — TIM2 1kHz, ISR 中释放 sem_sample
  wdg.h/c             — IWDG 封装

Layer 3: App 层 (纯策略/算法/协议 — 5 模块, 零 BSP 依赖)
  bms_shared.h/c(重写)— batteryval[] + 双 mutex
  protection.h/c(重写)— prot_result_t 签名, 纯判断
  soc_ocv.h/c   (保留)— 纯算法, 仅修精度 bug
  can_cmd.h/c   (重写)— can_action_req_t 返回 + can_pub
  bms_app.h/c   (重写)— 6 任务编排 + 5 同步原语
```

## 6 任务事件驱动架构

| 任务 | CMSIS-RTOS 优先级 | 周期 | 栈 | 触发方式 |
|------|------------------|------|-----|---------|
| **task_protect** | `osPriorityRealtime` (48) | 事件驱动 | 1024B | `osEventFlagsWait(evt_protect)` |
| **task_sample** | `osPriorityAboveNormal` (32) | 100ms | 2048B | `osSemaphoreAcquire(sem_sample)` ← TIM2 ISR give |
| **task_can_rx** | `osPriorityNormal` (24) | 50ms | 1024B | `osDelay(50)` 轮询 CAN RX FIFO |
| **task_balance** | `osPriorityBelowNormal` (16) | 500ms | 1024B | 首次 `EventFlagsWait(DATA_READY)`, 之后 `osDelay(500)` |
| **task_soc** | `osPriorityBelowNormal` (16) | 1000ms | 2048B | 首次 `EventFlagsWait(DATA_READY)`, 之后 `osDelay(1000)` |
| **task_can_tx** | `osPriorityLow` (8) | 100ms | 1024B | `osDelay(100)` + 喂狗 |

### 同步原语

| 名称 | 类型 | 用途 |
|------|------|------|
| `sem_sample` | `osSemaphore` (Binary, max=1) | TIM2 ISR → task_sample 采样触发 |
| `mutex_data` | `osMutex` (PrioInherit) | 保护 batteryval[] 采集数据 |
| `mutex_settings` | `osMutex` (PrioInherit) | 保护 bms_settings_t 可配置参数 |
| `evt_protect` | `osEventFlags` (12 bit) | 故障标志位，task_sample set → task_protect wait |
| `evt_data_ready` | `osEventFlags` (1 bit) | 首次数据就绪，解锁 task_balance + task_soc |
| `q_can_tx` | `ring_buf_t` (静态数组, 24×16B) | CAN TX 帧队列，支持 put_front 头插 |

### 核心数据流

```
TIM2 ISR (100ms, prio=5)
  osSemaphoreRelease(sem_sample)
    ↓
task_sample (prio 32)
  osSemaphoreAcquire(sem_sample)
  mutex_data lock
  bq76940_read_all() ──I2C──→ BQ76940
  if 连续 3 次失败 → osEventFlagsSet(evt_protect, FAULT_COMM_LOSS)
  写入 batteryval[]
  mutex_settings lock → 阈值预检 → 异常? osEventFlagsSet(evt_protect, fault_bit)
  首次完成 → osEventFlagsSet(evt_data_ready)
  mutex_data unlock

task_protect (prio 48) ← 最高优先级
  osEventFlagsWait(evt_protect, ALL_FAULTS)  ← 阻塞
  → 被唤醒后立即抢占当前任务
  → 读 batteryval[] 确定故障 → 执行保护动作
  → 生成紧急 CAN 帧 → ring_buf_put_front(&q_can_tx)  ← 头插
  → 回到阻塞

task_can_tx (prio 8)
  osDelay(100) → wdg_kick()
  can_pub() → 生成周期上报帧 → ring_buf_put()  ← 尾插
  while ring_buf_available: ring_buf_get → can_send  ← 自然按优先级出队
```

## CAN 协议

### 上行 (MCU → 上位机)

| CAN ID | 名称 | 来源 | 入队方式 |
|--------|------|------|---------|
| **0x100** | BMS_FAULT | task_protect | **头插** (硬件仲裁优先 + 软件优先) |
| 0x110 | CELL_VOLT_1_4 | can_pub | 尾插 |
| 0x111 | CELL_VOLT_5_9 | can_pub | 尾插 |
| 0x120 | BMS_STATUS | can_pub | 尾插 |
| 0x130 | SOC_OCV | can_pub | 尾插 |

### 下行 (上位机 → MCU)

| CAN ID | 名称 | 处理 |
|--------|------|------|
| 0x200 | QUERY | 立即回传指定数据 |
| 0x201 | CONTROL | FET/均衡/清除故障/关机 |
| 0x202 | CONFIG | 修改保护阈值 (掉电不保存) |

## 硬件引脚 (CubeMX 配置 — 仅 3 个)

| 引脚 | 功能 | 方向 | 说明 |
|------|------|------|------|
| PA8 | MCU_WAKE_BQ | 推挽输出 | BQ76940 TS1 启动唤醒 |
| PA15 | LED_0_ON | 推挽输出 | 单 LED 状态指示 |
| PB2 | Boot1 | 输入, 无上下拉 | MCU 启动模式引脚 |

> **注意**: PB10(SCL)/PB11(SDA) 不由 CubeMX 管理，由 i2c_sw 直接操作寄存器。PA11(CAN_RX)/PA12(CAN_TX) 由 CubeMX CAN 外设管理。

## 外设配置 (CubeMX)

| 外设 | 配置 | 用途 |
|------|------|------|
| SYSCLK | 72MHz (HSE 8MHz + PLL×9) | 系统时钟 |
| APB1 | 36MHz | I2C1, TIM2, CAN1 |
| APB2 | 72MHz | TIM1, GPIO |
| TIM1 | 1MHz→1kHz | HAL 时基 (1ms) |
| TIM2 | 1kHz, NVIC prio=5 | 采样触发 (ISR → semaphore) |
| I2C1 | 100kHz | 硬件 I2C (备用) |
| CAN1 | 500kbps, NVIC prio=5 | BMS 通信 |
| IWDG | LSI 40kHz | 独立看门狗 |

> **已移除**: USART1 (PA9/PA10), USART2 (PA2/PA3) — CAN 是唯一对外通信通道。

## 内存预算

| 区域 | 大小 | 备注 |
|------|------|------|
| SRAM 总量 | 20 KB | STM32F103C8Tx |
| FreeRTOS 堆 | 12288 B | configTOTAL_HEAP_SIZE |
| 任务栈: sample(2048)+soc(2048)+protect(1024)+balance(1024)+can_rx(1024)+can_tx(1024) | 8192 B | 堆内分配 |
| RTOS 控制块 (6×TCB + 2×EventGroup + 2×Mutex + 1×Semaphore) | ~1160 B | 堆内分配 |
| 内核开销 (定时器任务+空闲任务+堆管理) | ~2200 B | 堆内分配 |
| **堆使用合计** | **~11.5 KB** | 余量 ~0.8 KB |
| q_can_tx 静态数组 (24 × 16B) | 384 B | .bss |
| bms_shared_t 全局实例 | ~128 B | .bss |
| CSTACK (系统栈) | 1024 B | 链接器脚本 |
| 其他全局 + HAL + CubeMX | ~3.5 KB | .data+.bss |
| .noinit 故障诊断 (5×uint32_t) | 20 B | .noinit |
| **静态数据合计** | **~5.0 KB** | |
| **SRAM 总计** | **~16.5 KB / 20 KB** | 余量 ~3.5 KB |

---

## 实施阶段

当前代码实现的是**旧 arch-refactor 架构**。需要按照 active spec 重写。

### Phase 1: Spec 对齐准备
- [ ] **task_plan.md 更新** — 反映 active spec 架构 (本文件)
- [ ] **findings.md 更新** — 同步模块文档到新架构
- [ ] 明确旧代码中哪些可保留、哪些需重写

### Phase 2: Core 层修改

| 文件 | 操作 | 说明 |
|------|------|------|
| `Core/Inc/FreeRTOSConfig.h` | 确认 | heap=12288 (当前值需核实) |
| `Core/Src/main.c` | 修改 | 移除 USART1/USART2 初始化调用 |
| `Core/Src/freertos.c` | 修改 | 删除 defaultTask |
| `Core/Src/stm32f1xx_it.c` | 修改 | TIM2 ISR 添加 `osSemaphoreRelease(sem_sample)` |
| `Core/Src/stm32f1xx_it.c` | 修改 | 故障处理器保存 SCB 诊断寄存器到 .noinit |

### Phase 3: BSP 新增模块

| 文件 | 操作 | 说明 |
|------|------|------|
| `BSP/Inc/ring_buf.h` | **新增** | 元素级环形缓冲区 + put_front 头插, 纯头文件 |
| `BSP/Inc/bsp_common.h` | **新增** | bsp_status_t 枚举 (OK/ERROR/BUSY/TIMEOUT) |

### Phase 4: BSP 模块重写/修改

| 文件 | 操作 | 说明 |
|------|------|------|
| `BSP/Inc/led.h`, `BSP/Src/led.c` | **重写** | PA15 单 LED, 移除双 LED 枚举 |
| `BSP/Inc/io_ctrl.h`, `BSP/Src/io_ctrl.c` | **重写** | 精简为 PA8 单引脚, 移除 6 路 IO |
| `BSP/Inc/bq76940.h`, `BSP/Src/bq76940.c` | 修改 | 移除 io_ctrl 依赖; 补全 OCD/SCD 保护配置; 多字节读加 CRC8 |
| `BSP/Inc/can_drv.h`, `BSP/Src/can_drv.c` | 修改 | 改用 ring_buf 替代自实现 FIFO; 阻塞发送+计数信号量 |
| `BSP/Inc/usart_drv.h`, `BSP/Src/usart_drv.c` | **删除** | CAN 是唯一通信通道 |
| `BSP/Inc/i2c_sw.h` | 修改 | 确保 PB10/PB11 GPIOB 时钟在 init 时显式使能 |
| `BSP/Inc/timer.h`, `BSP/Src/timer.c` | 保留 | 接口不变 |

### Phase 5: App 模块重写/修改

| 文件 | 操作 | 说明 |
|------|------|------|
| `App/Inc/data_report.h`, `App/Src/data_report.c` | **删除** | 功能已被 can_cmd + task_can_tx 覆盖 |
| `App/Inc/protection.h`, `App/Src/protection.c` | **重写** | 签名为 `prot_result_t protection_check(data, settings)`; 零 BSP 调用 |
| `App/Inc/can_cmd.h`, `App/Src/can_cmd.c` | **重写** | can_cmd_dispatch 返回 can_action_req_t; 新增 can_pub(); can_rx_queue 封装为 static |
| `App/Inc/bms_shared.h`, `App/Src/bms_shared.c` | **重写** | batteryval[] + 双 mutex 新接口; bms_settings_t 补全阈值字段 |
| `App/Inc/soc_ocv.h`, `App/Src/soc_ocv.c` | 保留 | 接口不变, 仅修 calc_correction_weight 亚秒精度 bug |
| `App/Inc/bms_app.h`, `App/Src/bms_app.c` | **重写** | 6 任务 + 5 同步原语 + 1 ring_buf; 任务函数协调 App↔BSP |

### Phase 6: 头文件卫批量重命名

| 范围 | 操作 | 说明 |
|------|------|------|
| BSP/Inc/*.h (≤9 文件) | 修改 | `__BSP_XXX_H` → `BSP_XXX_H` (C11 7.1.3 合规) |
| App/Inc/*.h (5 文件) | 修改 | `__APP_XXX_H` → `APP_XXX_H` |

### Phase 7: CMakeLists.txt 更新

- [ ] 移除 `BSP/Src/usart_drv.c`
- [ ] 移除 `App/Src/data_report.c`
- [ ] ring_buf 和 bsp_common 为纯头文件，无需添加 .c

### Phase 8: CubeMX .ioc 更新

- [ ] 在 CubeMX 中移除 USART1/USART2
- [ ] 移除 PA0/PA1/PB1/PB12/PC13/PB0 等旧引脚
- [ ] 确认 PA8(MCU_WAKE_BQ), PA15(LED_0_ON), PB2(Boot1) 配置正确
- [ ] Regenerate Code, 手动合并 USER CODE 区域

### Phase 9: 编译验证

- [ ] 全量编译 0 警告 0 错误
- [ ] GCC size 分析 (目标: FLASH < 28KB, RAM ~16.5KB)
- [ ] 分层违规检查 (App 禁调 BSP, 0 处违规)

---

## 已做决策 (ADR)

| ADR | 决策 | 理由 |
|-----|------|------|
| 0001 | 删除 data_report 模块 | CAN ID 冲突 + 零调用者 + can_cmd 完整覆盖 |
| 0002 | App 禁调 BSP, 任务函数中转 | 分层纪律 + host 单元测试可行性 |
| 0003 | 保护阈值从运行时配置读取 | CAN 配置命令需实际生效 |
| 0004 | 单 CAN 队列 + ring_buf 头插 | 功能等价双队列, 节省 ~200B RTOS 堆 |
| 0005 | EventFlags 驱动 protect + Realtime 抢占 | 故障响应 < 10µs, 事件驱动省 CPU |

## 关键设计原则

| 原则 | 说明 |
|------|------|
| CubeMX 最小化 | 仅配置 3 个引脚 (PA8/PA15/PB2), 其余由 BSP 代码直接管理 |
| BSP 纯硬件抽象 | 不依赖 App 层, 模块间 DAG 无环 |
| App 禁调 BSP | App 只做纯判断/纯协议解析, 通过返回值向任务函数报告意图 |
| bms_app 任务编排 | bms_app.c 的任务函数是唯一有权同时调用 BSP 和 App 的代码 |
| 事件驱动保护 | task_sample 预检 → set EventFlags → task_protect (Realtime) 立即抢占 |
| CAN 优先发送 | protect 帧 CAN ID 0x100 (硬件仲裁最高) + ring_buf 头插 (软件优先) |

## 风险

| 风险 | 缓解 |
|------|------|
| TIM2 NVIC prio=5 = MAX_SYSCALL 阈值 | 标注约束, CubeMX regenerate 后检查 |
| PB10/PB11 时钟由 PB2 间接使能 | i2c_sw_init() 显式检查并手动使能 GPIOB 时钟 |
| ring_buf_put_front 无 ISR 保护 | ring_buf 仅任务上下文写入, ISR 不写 |
| 单 CAN 队列满时 protect 帧丢弃 | 深度 24 帧, 100ms 周期 ~5 帧/周期, 远不会满 |
| CubeMX regenerate 覆盖手动修改 | USER CODE 区域保护; regenerate 后检查 NVIC 优先级 |

## 不改的内容

| 模块 | 理由 |
|------|------|
| soc_ocv 算法核心 | 纯算法零依赖, 已是分层标杆 |
| i2c_sw CRC8 + 寄存器操作 | 已验证可行, 性能足够 |
| systick DWT µs 延时 | 依赖链最小, 无需改动 |
| 任务创建位置 (bms_app_init vs freertos.c) | 保留在 bms_app_init, spec 决策见 ADR-0002 |

---

*最后更新: 2026-07-27 — 从旧 arch-refactor 架构同步至 active spec bms-fullstack-redesign*
