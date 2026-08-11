# BMS 固件技术架构

> STM32F103C8T6 + FreeRTOS + BQ76940 | 严格 4 层分层 | 事件驱动

## 架构概览

```
┌──────────────────────────────────────────────────────────────┐
│                      App 层 (纯策略/算法)                      │
│  protection  ·  soc_ocv  ·  can_cmd  ·  bms_shared  ·  bms_app │
│          零 BSP 依赖 | 仅通过 bms_shared 交换数据              │
├──────────────────────────────────────────────────────────────┤
│                      BSP 层 (硬件抽象)                         │
│  bq76940  ·  i2c_sw  ·  can_drv  ·  timer  ·  wdg  ·  led   │
│  io_ctrl  ·  systick  ·  ring_buf  ·  bsp_common             │
│          纯硬件操作 | 模块间 DAG 无环                          │
├──────────────────────────────────────────────────────────────┤
│                      Core 层 (HAL 骨架)                       │
│  main.c  ·  freertos.c  ·  stm32f1xx_it.c                   │
│  gpio.c  ·  can.c  ·  i2c.c  ·  tim.c  ·  iwdg.c  ·  usart.c │
│          CubeMX 生成 | USER CODE 手动维护                     │
├──────────────────────────────────────────────────────────────┤
│                    Hardware (物理层)                           │
│  STM32F103C8T6 (Cortex-M3, 72MHz, 64KB/20KB)                │
│  BQ76940 (I2C, 9-15串 AFE)  ·  CAN Transceiver              │
└──────────────────────────────────────────────────────────────┘
```

## Layer 0: Hardware

| 组件 | 型号/规格 | 接口 |
|------|----------|------|
| MCU | STM32F103C8T6 | Cortex-M3, 72MHz |
| Flash | 64 KB | 内部 |
| SRAM | 20 KB | 内部 |
| AFE | BQ76940 (TI) | I2C 100kHz, 地址 0x08 |
| CAN PHY | 外部收发器 | PA11(RX)/PA12(TX), 500kbps |
| 调试 | SWD | PA13(SWDIO)/PA14(SWCLK) |
| LED | 2 个 | PC13(绿), PB0(红), 低电平亮 |
| 看门狗 | IWDG | LSI 40kHz |

## Layer 1: Core 层

**职责**: 系统启动、时钟配置、HAL 初始化、FreeRTOS 内核启动。**不含任何业务逻辑**。

### 文件清单

| 文件 | 功能 | 管理方式 |
|------|------|---------|
| `main.c` | `main()`: HAL_Init → SystemClock → MX_*_Init → bms_app_init → osKernelStart | USER CODE |
| `freertos.c` | `MX_FREERTOS_Init()`: 创建 6 个 BMS 任务 | USER CODE |
| `stm32f1xx_it.c` | ISR: CAN/TIM/I2C/SysTick/HardFault/.noinit 诊断 | USER CODE |
| `FreeRTOSConfig.h` | 内核参数: heap 12288B, 栈溢出检测=2 | USER CODE |
| `gpio.c/h` | GPIO 初始化 | CubeMX |
| `can.c/h` | CAN1 500kbps, FIFO0 | CubeMX |
| `i2c.c/h` | I2C1 100kHz (备用) | CubeMX |
| `iwdg.c/h` | IWDG LSI 40kHz | CubeMX |
| `tim.c/h` | TIM1(HAL时基) + TIM2(BSP) | CubeMX |
| `usart.c/h` | USART1(115200) + USART2(9600) | CubeMX |
| `main.h` | HSE_VALUE=8000000, 引脚宏 | CubeMX |

### CubeMX 配置入口
`bms_9s_f103.ioc` — 所有硬件引脚/时钟/外设由此文件定义。

### 硬件引脚 (仅 3 个由 CubeMX 管理)

| 引脚 | 功能 | 方向 |
|------|------|------|
| PA8 | MCU_WAKE_BQ | 推挽输出 |
| PA15 | LED_0_ON | 推挽输出 |
| PB2 | Boot1 | 输入 |

> PB10(SCL)/PB11(SDA) 由 i2c_sw 直接操作寄存器，不由 CubeMX 管理。

## Layer 2: BSP 层

**职责**: 封装 HAL 外设为语义化 API。**不依赖 App 层**。

### 模块依赖 DAG

```
systick (无依赖)
  ├── led (无依赖)
  ├── io_ctrl (无依赖)
  ├── i2c_sw (无依赖) → bq76940 (依赖 i2c_sw)
  ├── can_drv (依赖 ring_buf)
  ├── timer (无依赖)
  └── wdg (无依赖)
```

### 模块详情

#### bsp_common.h — 通用定义
- `bsp_status_t`: OK / ERROR / BUSY / TIMEOUT

#### ring_buf.h — 环形缓冲区
- `ring_buf_t`: 元素级环形缓冲区
- `put` (尾插) / `put_front` (头插, 紧急帧优先) / `get` / `available`
- can_drv 和 can_cmd 共用

#### systick — 系统滴答
- `bsp_tick_init()`: DWT 周期计数器
- `bsp_tick_get()`: ms 级时间戳
- `bsp_delay_us(n)`: DWT 微秒延时
- `bsp_tick_elapsed()` / `bsp_tick_timeout()`: 超时判断

#### led — LED 指示
- `led_init()` / `led_on(id)` / `led_off(id)` / `led_toggle(id)`
- PA15 单 LED, 直接 BSRR/BRR 操作

#### io_ctrl — GPIO 控制
- `io_ctrl_set(IO_ID_WAKE_BQ, state)`: PA8 控制
- `io_ctrl_init()`: TS1 boot 脉冲 (500μs 上升沿唤醒 BQ76940)

#### i2c_sw — 软件 I2C + CRC8
- 引脚: SCL=PB8, SDA=PB9 (开漏 + 内部上拉)
- `i2c_sw_init()` / `i2c_sw_write()` / `i2c_sw_read()`
- `i2c_sw_crc8()`: 多项式 x⁸+x²+x+1, key=0x07
- 非 CRC 模式 (芯片 BQ7694000/4002)
- SDA 模式技巧: 一直保持开漏输出, 读前先写 ODR=1

#### bq76940 — 电池监视器驱动
- I2C 地址: 0x08 (7-bit)
- 核心功能:
  - `bq76940_init(cfg)`: 初始化 + 保护配置 + ADC 校准
  - `bq76940_read_all(data)`: 一次读取全部数据
  - 9 电芯电压 (14-bit ADC → mV, 增益 ~380μV/LSB)
  - 总压 + 电流 (16-bit ADC)
  - 3 路温度 (内部 + 外部 NTC)
  - 保护状态 (OV/UV/OCD/SCD)
  - `bq76940_set_protection(cfg)`: OV/UV/OCD/SCD 阈值
- 关键约束:
  - **不支持连续大块读取** (>2 字节) — 必须逐对 2 字节读
  - **非 CRC 版本** — 使用无 CRC 的 I2C 命令
  - 写入间隔 ≥ 10ms

#### can_drv — CAN 驱动
- CAN1: 500kbps (APB1=36MHz, prescaler=9, BS1=5, BS2=2)
- FIFO0 中断接收 → ring_buf
- `can_drv_init()` / `can_send(msg)` / `can_recv(msg)` / `can_available()`
- `can_msg_t`: id(11-bit), len(0-8), data[8]
- 阻塞发送 + 计数信号量

#### timer — 定时器
- TIM2: APB1 36MHz, PSC=36000-1, ARR=1000-1 → 1kHz
- ISR 中释放 `sem_sample` 信号量
- `timer_set_callback(cb)` / `timer_set_timeout(ms, cb)`

#### wdg — 看门狗
- IWDG: LSI 40kHz
- `wdg_init(timeout_ms)` / `wdg_kick()` / `wdg_is_reset_source()`

## Layer 3: App 层

**职责**: 数据处理、策略判断、协议解析。**禁止直接调用 BSP 函数或访问 HAL 寄存器**。

### bms_shared — 共享数据中心
- 多任务间数据交换枢纽
- `bms_shared_t`: bq_data + prot_ctx + soc_permil + settings
- 双 Mutex: `data_mutex` + `settings_mutex`

### protection — 保护判断 (纯判断)
- 12 种故障: CELL_OV/UV, PACK_OV/UV, OC(放电/充电), SHORT, OT, UT, IMBALANCE, COMM_LOSS, WATCHDOG
- 3 级: NONE → WARNING → ALERT → FAULT
- 故障确认去抖 (3 次进入/恢复)
- 签名: `prot_result_t protection_check(data, settings)` — 零 BSP

### soc_ocv — SOC/OCV 估算 (纯算法, 分层标杆)
- 增强型安时积分 + OCV 查表校正
- 4 状态 FSM: IDLE → DISCHARGE → CHARGE → POLARIZATION
- 温度补偿容量: C_effective = C_nominal × 系数(T)
- 动态 OCV 校正: 连续权重 × 静置计时器
- 电流零漂自估计: EMA + 冻结/超时
- OCV-SOC 表: 31 点 (SCV_SOC_Mapping.csv 真实数据), 线性插值
- SOC 安全余量: 底部 3% 隐藏 (SOC_DISPLAY_OFFSET=30)

### can_cmd — CAN 协议 (纯协议)
- `can_cmd_dispatch()`: 返回 `can_action_req_t` (不调 BSP)
- `can_pub()`: 周期上报帧生成
- 上行: 0x100/0x110/0x111/0x120/0x130
- 下行: 0x200/0x201/0x202

### bms_app — 任务编排 (唯一有权调 BSP + App)
- `bms_app_init()`: 初始化全部 BSP + App (严格顺序)
- 6 个 FreeRTOS 任务 + 7 个同步原语

## FreeRTOS 任务设计

### 6 任务事件驱动架构

```
TIM2 ISR (100ms, prio=5)
  osSemaphoreRelease(sem_sample)
    ↓
task_sample (prio 32)
  osSemaphoreAcquire(sem_sample)
  mutex_data lock
  bq76940_read_all() ──I2C──→ BQ76940
  故障预检 → osEventFlagsSet(evt_protect, fault_bit)
  首次完成 → osEventFlagsSet(evt_data_ready)
  mutex_data unlock

task_protect (prio 48) ← 最高, 事件驱动
  osEventFlagsWait(evt_protect) → 阻塞
  → 被唤醒后立即抢占当前任务
  → 读数据 → 保护动作 → 紧急 CAN 帧 (ring_buf_put_front)

task_can_tx (prio 8)
  wdg_kick() + can_pub() → ring_buf_put()
  while available: ring_buf_get → can_send
  (自然按优先级出队: 头插故障帧先发)
```

### 任务参数

| 任务 | CMSIS-RTOS 优先级 | 周期 | 栈 | 触发 |
|------|------------------|------|-----|------|
| task_protect | Realtime (48) | 事件驱动 | 1024B | EventFlagsWait |
| task_sample | AboveNormal (32) | 100ms | 2048B | Semaphore (TIM2 ISR) |
| task_can_rx | Normal (24) | 50ms | 1024B | osDelay 轮询 |
| task_balance | BelowNormal (16) | 500ms | 1024B | EventFlagsWait + osDelay |
| task_soc | BelowNormal (16) | 1000ms | 2048B | EventFlagsWait + osDelay |
| task_can_tx | Low (8) | 100ms | 1024B | osDelay + 喂狗 |

### 同步原语

| 名称 | 类型 | 用途 |
|------|------|------|
| `sem_sample` | Binary Semaphore | TIM2 ISR → task_sample |
| `mutex_data` | Mutex (PrioInherit) | 保护 batteryval[] |
| `mutex_settings` | Mutex (PrioInherit) | 保护 bms_settings_t |
| `evt_protect` | EventFlags (12 bit) | 故障标志位 |
| `evt_data_ready` | EventFlags (1 bit) | 首次数据就绪 |
| `q_can_tx` | ring_buf_t (24×16B) | CAN TX 队列 (支持头插) |
| `mutex_iic` | Mutex | I2C 总线互斥 |

## CAN 通信协议

### 上行帧 (MCU → 上位机, 周期性)

| CAN ID | 名称 | 内容 | 周期 | 入队方式 |
|--------|------|------|------|---------|
| 0x100 | BMS_FAULT | 故障状态 (紧急) | 事件驱动 | **头插优先** |
| 0x110 | CELL_VOLT_1_4 | C1–C4 电压 (mV) | 100ms | 尾插 |
| 0x111 | CELL_VOLT_5_9 | C5–C9 电压 (mV) | 100ms | 尾插 |
| 0x120 | BMS_STATUS | 总压/电流/SOC/FET/故障/保护 | 100ms | 尾插 |
| 0x130 | SOC_OCV | SOC/OCV 详细数据 | 1000ms | 尾插 |

### 下行帧 (上位机 → MCU, 事件驱动)

| CAN ID | 名称 | 用途 |
|--------|------|------|
| 0x200 | QUERY | 查询 → MCU 立即回传 |
| 0x201 | CONTROL | FET / 均衡 / 清除故障 / 关机 |
| 0x202 | CONFIG | 修改保护阈值 (掉电不保存) |

## 内存预算

| 区域 | 大小 | 备注 |
|------|------|------|
| SRAM 总量 | 20 KB | STM32F103C8Tx |
| FreeRTOS 堆 | 12288 B | configTOTAL_HEAP_SIZE |
| 任务栈合计 | 8192 B | sample(2048)+soc(2048)+4×1024 |
| RTOS 控制块 | ~1160 B | TCB + EventGroup + Mutex + Semaphore |
| 内核开销 | ~2200 B | 定时器任务 + 空闲任务 + 堆管理 |
| q_can_tx 静态 | 384 B | .bss |
| 全局变量 + HAL | ~3.5 KB | .data + .bss |
| .noinit 诊断 | 20 B | 5×uint32_t |
| **总计** | **~19.6 KB** | 余量 ~0.4 KB |

## 编译指标 (最终状态)

| 指标 | 数值 |
|------|------|
| 编译器 | arm-none-eabi-gcc (C11, -O0 -g3) |
| FLASH | 56.2 KB / 64 KB (87.8%) |
| RAM | 19.6 KB / 20 KB (95.8%) |
| 警告/错误 | 0 / 0 |
| BSP 模块 | 9 个 (+ ring_buf, bsp_common) |
| App 模块 | 5 个 |

## 设计原则

| 原则 | 说明 |
|------|------|
| **CubeMX 最小化** | 仅配置 3 个引脚, 其余 BSP 代码直接管理 |
| **BSP 纯硬件抽象** | 不依赖 App 层, DAG 无环 |
| **App 禁调 BSP** | 通过返回值向任务函数报告意图 |
| **bms_app 唯一中转** | 任务函数是唯一同时调 BSP + App 的代码 |
| **事件驱动保护** | 故障响应 < 10μs |
| **CAN 头插优先** | 故障帧 0x100 硬件仲裁最高 + 软件头插 |
