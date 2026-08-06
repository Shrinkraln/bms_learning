# 发现与决策记录

## 需求
基于 STM32F103 + FreeRTOS + BQ76940 的 9 串 BMS 固件，从底层向上开发完整驱动栈。硬件配置通过 CubeMX 管理。

## 全栈分层架构

### Layer 0: Hardware（硬件层）
- **MCU:** STM32F103C8Tx (Cortex-M3, 72MHz, 64KB FLASH, 20KB SRAM)
- **电池监视器:** BQ76940 (TI, 9-15 串, I2C 100kHz + CRC8)
- **时钟源:** HSE 8MHz + PLL×9 = 72MHz SYSCLK
- **调试接口:** SWD (PA13/PA14)
- **CAN 收发器:** 外部 PHY (PA11/PA12)
- **LED:** 2 个 (PC13 绿色, PB0 红色, 低电平亮)

### Layer 1: Core 层（CubeMX 生成的 HAL 骨架）

**职责**: 系统启动、时钟配置、外设 HAL 初始化、FreeRTOS 内核启动。**不含任何业务逻辑**。

| 文件 | 功能 | CubeMX 管理 |
|------|------|-------------|
| `main.c` | `main()` 入口: HAL_Init→SystemClock→MX_*_Init→bms_app_init→osKernelStart | USER CODE 区域手动维护 |
| `freertos.c` | `MX_FREERTOS_Init()`: 创建 6 个 BMS 任务 | USER CODE 区域手动维护 |
| `stm32f1xx_it.c` | 中断服务函数 (CAN/USART/TIM/I2C/SysTick) | USER CODE 区域手动维护 |
| `FreeRTOSConfig.h` | 内核配置: 堆大小、优先级数、钩子函数 | USER CODE 区域手动维护 |
| `gpio.c/h` | GPIO 初始化 (引脚模式/速度/上下拉) | ✅ CubeMX 生成 |
| `can.c/h` | CAN1 初始化 (500kbps, FIFO0) | ✅ CubeMX 生成 |
| `i2c.c/h` | I2C1 初始化 (100kHz, 备用) | ✅ CubeMX 生成 |
| `iwdg.c/h` | IWDG 初始化 (LSI 40kHz) | ✅ CubeMX 生成 |
| `tim.c/h` | TIM1(时基) + TIM2(BSP) 初始化 | ✅ CubeMX 生成 |
| `usart.c/h` | USART1(115200) + USART2(9600) 初始化 | ✅ CubeMX 生成 |
| `main.h` | 系统头文件 (HSE_VALUE=8000000, 引脚宏) | ✅ CubeMX 生成 |
| `stm32f1xx_hal_conf.h` | HAL 模块开关宏 | ✅ CubeMX 生成 |

**CubeMX 配置入口**: `bms_9s_f103.ioc` — 所有硬件引脚/时钟/外设由此文件定义

### Layer 2: BSP 层（板级硬件抽象 — 10 模块）

**职责**: 封装 HAL 外设为语义化 API，提供硬件操作接口。**不依赖 App 层，模块间 DAG 无环**。

模块依赖关系:
```
systick (无依赖)
  ├── led (无依赖)
  ├── io_ctrl (无依赖)
  ├── i2c_sw (无依赖) → bq76940 (依赖 i2c_sw)
  ├── usart_drv (无依赖, 依赖 ring_buf)
  ├── can_drv (无依赖, 依赖 ring_buf)
  ├── timer (无依赖)
  └── wdg (无依赖)
```

#### bsp_common.h（新增 — 通用定义）
- `bsp_status_t` 枚举: OK / ERROR / BUSY / TIMEOUT
- 各模块 typedef 别名: `can_drv_status_t`, `bq76940_status_t` 等

#### ring_buf.h（新增 — 通用环形缓冲区）
- `ring_buf_t` 结构体: buf指针 + head/tail/capacity
- 操作: `init` / `put` (drop-newest 溢出) / `get` / `available`
- usart_drv 和 can_drv 共用此实现

#### systick.h/c — 系统滴答定时器
- `bsp_tick_init()`: 使能 DWT 周期计数器
- `bsp_tick_get()`: 返回 ms 级时间戳 (uint32_t)
- `bsp_delay_us(n)`: DWT 微秒延时 (无中断依赖)
- `bsp_tick_elapsed(start)`: 计算距 start 的 ms 差 (处理溢出)
- `bsp_tick_timeout(start, timeout)`: 判断是否超时

#### led.h/c — LED 状态指示
- `led_init()`: 初始化 PC13/PB0 为推挽输出，初始灭
- `led_on(id)` / `led_off(id)` / `led_toggle(id)`: 枚举 API (LED_ID_1/LED_ID_2)
- 低电平点亮（开漏等效，PC13 硬件限制）
- 不使用 HAL GPIO 模式切换 (直接操作 BSRR/BRR)

#### io_ctrl.h/c — GPIO 输入输出控制
- 6 路控制 IO: BQ_POWER(PA0), D_POWER(PA1), WAKE_BQ(PA8), STA_WAKE(PB1), BQ1_ALERT(PB2), IN_STA(PB12)
- 枚举索引 API: `io_ctrl_set(id, state)` / `io_ctrl_get(id)`
- `io_ctrl_power_off_all()`: 批量断电 (紧急情况)
- BSRR/BRR 直接寄存器操作

#### i2c_sw.h/c — 软件 I2C + CRC8
- 引脚: SCL=PB10, SDA=PB11 (开漏输出, 外部上拉)
- `i2c_sw_init()`: 初始化引脚, 释放总线
- `i2c_sw_write(addr, reg, data, len)`: 写设备寄存器
- `i2c_sw_read(addr, reg, data, len)`: 读设备寄存器
- `i2c_sw_read_crc(addr, reg, data, len)`: 带 CRC8 的多字节读取
- `i2c_sw_crc8(data, len)`: CRC8 计算 (多项式 0x07, 初值 0x00)
- SDA 模式技巧: 一直保持开漏输出, 读前先写 ODR=1 释放总线

#### usart_drv.h/c — 串口驱动
- USART1 (PA9/PA10): 115200-8N1, 调试控制台
- USART2 (PA2/PA3): 9600-8N1, 外部通信
- 中断接收 + 环形缓冲区 (`ring_buf_t`)
- `usart_drv_init()`: 使能 IDLE 中断
- `usart_send_buf(usart, data, len)`: 阻塞发送
- `usart_printf(usart, fmt, ...)`: 格式化打印
- `usart_available(usart)` / `usart_recv_byte(usart)`: 环形缓冲区读取

#### bq76940.h/c — BQ76940 电池监视器驱动
- I2C 地址: 0x08 (7-bit) / 0x10 (8-bit)
- `bq76940_init(cfg)`: 芯片初始化 + 保护配置
- `bq76940_read_all(data)`: 一次读取全部数据
  - 9 电芯电压 (14-bit ADC → mV, 增益 380μV/LSB)
  - 总压 + 电流 (16-bit ADC)
  - 3 路温度传感器 (内部 + 外部 NTC)
  - 保护状态 (OV/UV/OCD/SCD)
- `bq76940_set_protection(cfg)`: 配置 OV/UV/OCD/SCD 阈值及延时
- `bq76940_set_balancing(mask)`: 设置电芯均衡 (CELLBAL1/2)
- 待实现: 多字节读取加 CRC8 验证, 补全 OCD/SCD 配置

#### can_drv.h/c — CAN 总线驱动
- CAN1: 500kbps (APB1=36MHz, prescaler=9, BS1=5, BS2=2, SJW=1)
- FIFO0 中断接收 → 环形缓冲区 (`ring_buf_t`)
- `can_drv_init()`: 启动 CAN 外设 + 使能中断
- `can_send(msg)`: 发送 CAN 帧 (待改为阻塞等待 + 计数信号量)
- `can_recv(msg)`: 从环形缓冲区取一帧
- `can_available()`: 环形缓冲区中可用帧数
- `can_msg_t` 结构体: id(11-bit), len(0-8), data[8]

#### timer.h/c — 定时器驱动
- TIM2: APB1 36MHz, PSC=36000-1, ARR=1000-1 → 1kHz
- `timer_init()`: 启动 TIM2
- `timer_set_callback(cb)`: 注册周期性回调
- `timer_set_timeout(ms, cb)`: 注册单次超时回调
- `bsp_timer_irq_handler(htim)`: 中断分发 (在 main.c 中调用)

#### wdg.h/c — 看门狗封装
- IWDG: LSI 40kHz
- `wdg_init(timeout_ms)`: 初始化独立看门狗
- `wdg_kick()`: 重装载计数器 (喂狗)
- `wdg_is_debug()`: 调试模式下暂停看门狗

### Layer 3: App 层（纯策略/算法/协议 — 5 模块）

**职责**: 数据处理、策略判断、协议解析。**禁止直接调用 BSP 函数或访问 HAL 寄存器**。通过 `bms_shared` 交换数据，通过返回值向任务函数报告硬件操作意图。

#### bms_shared.h/c — 共享数据中心
- 多任务间数据交换枢纽
- `bms_shared_t` 结构:
  - `bq_data` — 最新采集数据 (acquisition 写, 其他读)
  - `prot_ctx` + `prot_level` — 保护状态 (protection 写, can_tx 读)
  - `soc_permil` + `ocv_mv` + `remaining_mah` — SOC/OCV (soc 写, can_tx 读)
  - `settings` — 运行时可配置参数 (can_rx 写, 各任务读)
- 双 mutex: `data_mutex` (采集/保护/SOC) + `settings_mutex` (参数)
- `bms_shared_data_lock(ms)` / `bms_shared_data_unlock()`: 数据加锁
- `bms_shared_settings_lock(ms)` / `bms_shared_settings_unlock()`: 参数加锁
- `bms_shared_update_*()`: 便捷更新函数

#### protection.h/c — 保护判断模块（纯判断）
- 12 种故障检测:
  - 电压: CELL_OV, CELL_UV, PACK_OV, PACK_UV
  - 电流: DISCHARGE_OC, CHARGE_OC, SHORT_CIRCUIT
  - 温度: OVER_TEMP, UNDER_TEMP
  - 系统: CELL_IMBALANCE, COMM_LOSS, WATCHDOG
- 3 级保护: NONE → WARNING → ALERT → FAULT
- 故障确认计数器 (去抖): 进入 3 次确认, 恢复 3 次确认
- 待实现: 签名改为 `protection_check(data, settings)` → `prot_result_t`
- 待实现: 移除内部 BSP 调用 (`io_ctrl_power_off_all`, `bsp_tick_get`)

#### soc_ocv.h/c — SOC/OCV 估算模块（纯算法，分层标杆）
- 算法: 增强型安时积分 + OCV 查表校正
- 温度补偿容量: C_effective = C_nominal × 系数(T)
- 动态 OCV 校正: 连续权重 × 静置计时器
- 电流零漂自估计: EMA + 冻结/超时
- 上电 OCV 直接初始化
- OCV-SOC 表: 21 点 (5% 间隔), 线性插值
- 零 BSP 依赖, 零 HAL 调用 — 纯算法模块
- 待修复: `calc_correction_weight` 亚秒级精度丢失

#### can_cmd.h/c — CAN 指令协议（纯协议解析）
- 上行 CAN ID (MCU→上位机, 周期性):
  - 0x100: 电池综合状态 (总压/电流/SOC/FET/故障)
  - 0x110: 电芯电压 1-4
  - 0x111: 电芯电压 5-9
  - 0x112: 电芯电压 (备用/扩展)
  - 0x120: 保护状态 + 故障码
  - 0x130: SOC/OCV 数据
- 下行 CAN ID (上位机→MCU, 事件驱动):
  - 0x200: 查询命令 → 立即回传指定数据
  - 0x201: 控制命令 → FET/均衡/关机/清除故障
  - 0x202: 参数配置 → 修改保护阈值
- 待实现: `can_cmd_dispatch()` 返回 `can_action_req_t` (纯协议, 不调 BSP)
- 待实现: `can_rx_queue` 封装为 static
- 待实现: FET 控制加安全条件检查 (无故障才允许开启)

#### bms_app.h/c — 主应用 + 任务编排（唯一有权同时调用 BSP 和 App 的模块）
- `bms_app_init()`: 初始化全部 BSP + App 模块 (顺序见代码)
- 6 个 FreeRTOS 任务:

| 任务 | 优先级 | 周期 | 栈 | 职责 |
|------|--------|------|-----|------|
| can_rx_task | High | 事件驱动 | 2048B | CAN 指令接收 + 分发 |
| protection_task | AboveNormal | 10ms | 1024B | 保护检查 + FET 控制 |
| acquisition_task | Normal | 100ms | 2048B | BQ76940 数据采集 |
| can_tx_task | Normal | 100ms | 1024B | CAN 数据上报 |
| soc_task | Normal | 1000ms | 2048B | SOC/OCV 更新 |
| watchdog_task | Low | 500ms | 512B | 喂狗 + LED 心跳 |

- 待实现: 任务创建从 `bms_app_init()` 移至 `freertos.c`

## CubeMX 硬件配置

**所有硬件配置通过 CubeMX IDE 的 `bms_9s_f103.ioc` 文件管理**，包括:
- 芯片型号: STM32F103C8Tx
- 时钟树: HSE 8MHz → PLL×9 → SYSCLK 72MHz, APB1 36MHz, APB2 72MHz
- 引脚功能分配 (见 task_plan.md 硬件引脚表)
- 外设参数: CAN 波特率、USART 波特率、I2C 速度、定时器分频
- FreeRTOS CMSIS-RTOS V2 中间件启用
- NVIC 中断优先级分组和使能

**CubeMX 修改流程**:
1. 在 CubeMX 中打开 `.ioc` 文件修改配置
2. Regenerate Code (生成 Core 层 HAL 代码)
3. 手动合并 USER CODE 区域的冲突
4. 重新编译验证

## 架构重构设计 spec

位置: `.spec-dev/2026-07-22-arch-refactor/spec/arch-refactor-design.md`

### 已完成的 spec 变更
- bms_shared.h/c (共享数据中心)
- soc_ocv.h/c (SOC/OCV 估算)
- can_cmd.h/c (CAN 协议解析)

### 待完成的 spec 变更

| 优先级 | 变更 | 文件 | 类型 |
|--------|------|------|------|
| P0 | 删除 data_report 模块 | App/ | REMOVED |
| P0 | protection_check 改签名为 prot_result_t | App/Src/protection.c | MODIFIED |
| P0 | can_cmd_dispatch 返回 can_action_req_t | App/Src/can_cmd.c | MODIFIED |
| P0 | 任务创建移至 freertos.c | Core/Src/freertos.c, App/Src/bms_app.c | MODIFIED |
| P1 | FreeRTOS 堆 3072→15360 | Core/Inc/FreeRTOSConfig.h | MODIFIED |
| P1 | 删除 defaultTask | Core/Src/freertos.c | REMOVED |
| P1 | 故障处理器 .noinit 诊断 | Core/Src/stm32f1xx_it.c | MODIFIED |
| P1 | can_send 阻塞等待 (计数信号量) | BSP/Src/can_drv.c | MODIFIED |
| P1 | ring_buf.h + bsp_common.h | BSP/Inc/ | ADDED |
| P1 | bms_settings_t 补全阈值字段 | App/Inc/bms_shared.h | ADDED |
| P2 | BQ76940 多字节读加 CRC8 | BSP/Src/bq76940.c | MODIFIED |
| P2 | BQ76940 OCD/SCD 保护配置补全 | BSP/Src/bq76940.c | MODIFIED |
| P2 | soc_ocv 校正权重精度修正 | App/Src/soc_ocv.c | MODIFIED |
| P2 | 头文件卫重命名 (14 文件) | BSP/Inc/*, App/Inc/* | MODIFIED |
| P3 | usart_drv 复用 ring_buf | BSP/Src/usart_drv.c | MODIFIED |
| P3 | can_drv 复用 ring_buf | BSP/Src/can_drv.c | MODIFIED |
| P3 | CAN ID 0x112 显式定义 | App/Inc/can_cmd.h | MODIFIED |
| P3 | can_rx_queue 封装为 static | App/Src/can_cmd.c | MODIFIED |
| P3 | CAN TX 快照拷贝后解锁 | App/Src/bms_app.c | MODIFIED |

## 技术决策 (ADR)

| ADR | 决策 | 理由 |
|-----|------|------|
| 0001 | 删除 data_report 模块 | CAN ID 冲突 + 零调用者 + can_cmd 完整覆盖 |
| 0002 | App 禁调 BSP, 任务函数中转 | 分层纪律 + host 单元测试可行性 |
| 0003 | 保护阈值从运行时配置读取 | CAN 配置命令需实际生效 |

## 项目的 CubeMX 职责边界

1. **CubeMX 管理**: 引脚分配、时钟树、外设初始化参数、FreeRTOS 启用
2. **手动维护**: 所有 BSP/App 源码、main.c/freertos.c/stm32f1xx_it.c 的 USER CODE 区域
3. **CubeMX Regenerate 后**: 仅覆盖 Core 层非 USER CODE 区域，BSP/App 不受影响

## 编译指标 (当前状态)

| 指标 | 数值 |
|------|------|
| FLASH 占用 | ~26.7 KB / 64 KB (41.7%) |
| RAM 占用 | ~9.3 KB / 20 KB (46.5%) |
| 编译器 | arm-none-eabi-gcc (C11, -O0 -g3) |
| 警告/错误 | 0 / 0 |
| 源文件 | 20+ 个 (.h + .c) |
| BSP 模块 | 9 个 |
| App 模块 | 6 个 (含待删除 data_report) |

## 重构后预期指标

| 指标 | 目标 |
|------|------|
| FLASH | < 28 KB (删 data_report -1KB, 新增 ~200行 +0.8KB) |
| RAM | < 18.5 KB (ucHeap 12KB + 静态 6KB + .noinit 20B + 余量) |
| 分层违规 | 0 处 |
| App 模块 | 5 个 (活跃) |
| BSP 模块 | 10 个 (+ ring_buf) |

## 资源
- 项目根目录: `D:\coding_codes\bms_learning\bms_9s_f103\bms_9s_f103`
- CubeMX 配置: `bms_9s_f103.ioc`
- 架构 spec: `.spec-dev/2026-07-22-arch-refactor/spec/arch-refactor-design.md`
- ADR 记录: `.spec-dev/adr/0001-*.md` ~ `0003-*.md`
- CMakeLists.txt: 分层 OBJECT 库编译 (stm32cubemx → BSP → App → executable)

---

*每 2 次查看/浏览器/搜索操作后更新此文件*
