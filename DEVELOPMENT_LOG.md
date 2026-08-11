# Development Log — BMS 全栈项目开发日志

> 时间线记录：从参考代码逆向分析到全栈联调的完整过程。

---

## 目录

- [Phase 0: 参考项目逆向分析](#phase-0-参考项目逆向分析)
- [Phase 1: 开发框架搭建](#phase-1-开发框架搭建)
- [Phase 2: BSP 驱动层开发](#phase-2-bsp-驱动层开发)
- [Phase 3: App 应用层开发](#phase-3-app-应用层开发)
- [Phase 4: 架构重构 — 全栈重设计](#phase-4-架构重构--全栈重设计)
- [Phase 5: 三 Spec 融合实施](#phase-5-三-spec-融合实施)
- [Phase 6: BMS Qt 上位机开发](#phase-6-bms-qt-上位机开发)
- [Phase 7: 全链路调试与问题修复](#phase-7-全链路调试与问题修复)
- [Phase 8: 上位机 Bug 修复](#phase-8-上位机-bug-修复)
- [Phase 9: SOC 算法与硬件问题排查](#phase-9-soc-算法与硬件问题排查)
- [关键发现汇总](#关键发现汇总)
- [ADR 决策记录索引](#adr-决策记录索引)

---

## Phase 0: 参考项目逆向分析

**日期**: 2026-07-13 ~ 2026-07-15

### 目标
分析 TI BQ76940 参考项目 (Keil MDK-ARM)，提取完整引脚配置、寄存器定义、通信协议和保护参数。

### 参考项目
`datasheet/BQ76940发货资料 20220817/3.程序/1.BQ76940（程序加CAN20210915）改温度探头/BMS_s940/`

### 发现与决策

#### 硬件引脚完整映射

**GPIOA**:
| 引脚 | 功能 | 方向 |
|------|------|------|
| PA0 | QB 电源控制 | OUT |
| PA1 | D 电源控制 | OUT |
| PA8 | BQ76940 唤醒信号 | OUT |
| PA9 | USART1 TX (QT 上位机) | AF |
| PA10 | USART1 RX (QT 上位机) | AF |
| PA11 | CAN RX | AF |
| PA12 | CAN TX (⚠️ 与 MCU_KZ_P2 冲突) | AF |
| PA13-14 | SWD 调试接口 | AF |

**GPIOB**:
| 引脚 | 功能 | 方向 |
|------|------|------|
| PB0 | LED 指示灯 | OUT |
| PB1 | 唤醒状态指示 | OUT |
| PB2 | BQ76940 ALERT 信号 | OUT |
| PB8-9 | 软件 I2C (SCL/SDA) | OD |
| PB12 | 状态指示 | OUT |
| PB13-15 | SPI Flash | AF |

#### BQ76940 关键寄存器

| 地址 | 名称 | 描述 |
|------|------|------|
| 0x00 | SYS_STAT | 系统状态 |
| 0x01-03 | CELLBAL1-3 | 电池均衡控制 |
| 0x04 | SYS_CTRL1 | ADC 使能 / SHIP 模式 |
| 0x05 | SYS_CTRL2 | DSG/CHG FET 控制 |
| 0x06-08 | PROTECT1-3 | SCD/OCD 保护参数 |
| 0x09 | OV_TRIP | 过压阈值 |
| 0x0A | UV_TRIP | 欠压阈值 |
| 0x0B | CC_CFG | 库仑计配置 |
| 0x0C-0x29 | VC1-VC15 | 15 路 14-bit ADC (仅用 9 路) |
| 0x32-0x33 | CC | 库仑计数器 (电流) |
| 0x50-51 | ADCGAIN1/ADCOFFSET | ADC 校准 |
| 0x59 | ADCGAIN2 | ADC 增益 2 |

#### 电压计算公式
```
ADC_GAIN = ((gain1 & 0x0C) << 1) + ((gain2 & 0xE0) >> 5)
GAIN = 365 + ADC_GAIN (μV/LSB)
V_cell(mV) = (ADC_raw × GAIN) / 1000 + ADC_offset
```

#### 保护参数

| 保护类型 | 阈值 | 恢复 | 执行层 |
|---------|------|------|--------|
| 过压 (OV) | 4400mV | 4300mV | BQ76940 硬件 |
| 欠压 (UV) | 2400mV | 2500mV | BQ76940 硬件 |
| 软件 OV | 4200mV | 4100mV | MCU → CHG FET |
| 软件 UV | 2800mV | 2800mV | MCU → DSG FET |
| 过流 (OC) | 2000mA | 2000mA | MCU → FET |
| SCD | 66A / 400μs | - | BQ76940 硬件 |
| OCD | 100A / 1280ms | - | BQ76940 硬件 |

#### 通信协议

- **USART1** (115200bps): QT 上位机指令 (帧头 `01 02 55` 等)
- **USART2** (9600bps): HMI 串口屏 ASCII 指令
- **CAN** (500kbps): 7 帧数据上报, ID 0x0001-0x0007

#### 关键技术决策

| 决策 | 理由 |
|------|------|
| HAL 库替代 StdPeriph | CubeMX 默认, API 现代化 |
| 保持软件 I2C (GPIO 模拟) | BQ76940 CRC 协议需要灵活时序 |
| 保持原 9 节 VC 采集索引 | 硬件连接决定物理通道映射 |
| CMake 替代 Keil 工程 | 跨平台, VSCode 友好 |

#### BQ76940 初始化序列

```
SYS_STAT  = 0xFF  // 清除所有状态
CELLBAL1  = 0x00  // 关闭均衡
SYS_CTRL1 = 0x18  // 启用 ADC
SYS_CTRL2 = 0x43  // DSG ON, CHG ON
CC_CFG    = 0x19  // 库仑计配置
```

---

## Phase 1: 开发框架搭建

**日期**: 2026-07-15

### 交付物

1. **DEVELOPMENT_GUIDE.md** (~1370 行) — 完整开发流程指南
   - 11 个章节: 环境搭建 → CubexMX → CMake → 分层设计 → BSP 驱动 → BQ76940 → App → 调试 → 验证
   - StdPeriph → HAL 转换对照表 (20+ API)

2. **项目脚手架**
   - `CMakeLists.txt` — ARM GCC 构建脚本
   - `STM32F103C8Tx_FLASH.ld` — GCC 链接脚本
   - `Core/Inc/` — main.h (完整引脚宏) + stm32f1xx_hal_conf.h
   - `BSP/Inc/*.h` — 9 个 BSP 驱动头文件 (含完整宏定义)
   - `.vscode/` — IntelliSense + Debug + Tasks + Extensions
   - `scripts/setup_cmsis_hal.sh` — CMSIS/HAL 自动下载

3. **规划文件**
   - `findings.md` — 参考项目完整分析
   - `task_plan.md` — 5 阶段任务计划
   - `progress.md` — 进度追踪

---

## Phase 2: BSP 驱动层开发

**日期**: 2026-07-20

### 模块开发顺序

#### ✅ systick + LED (最基础)
- DWT 微秒延时 + HAL 毫秒封装 + 超时计算
- LED 枚举 API, HAL GPIO, 低电平点亮适配 (PC13/PB0)

#### ✅ io_ctrl — GPIO 控制
- 6 路控制 IO: BQ_POWER(PA0), D_POWER(PA1), WAKE_BQ(PA8), STA_WAKE(PB1), BQ1_ALERT(PB2), IN_STA(PB12)
- 枚举索引 API: `io_ctrl_set(id, state)` / `io_ctrl_get(id)`
- BSRR/BRR 直接寄存器操作

#### ✅ i2c_sw — 软件 I2C + CRC8
- 5 层实现: GPIO 初始化 → I2C 基本时序 → 标准读写 → CRC8 写 → CRC8 算法
- 引脚 SCL/SDA (开漏输出, 外部上拉)
- CRC8 多项式: x⁸+x²+x+1, key=0x07

#### ✅ usart_drv — 串口驱动
- USART1 (115200) + USART2 (9600)
- 中断接收 + 环形缓冲区 + printf 支持

#### ✅ BQ76940 驱动 (核心)
- 完整驱动: 初始化 / 电压 / 温度 / 电流 / 保护 / 均衡 / CRC
- 9 节电池非连续 VC 读取 (VC1/2/5/6/7/10/11/12/15)
- ADC 校准 (GAIN + OFFSET)
- FET 控制 (SYS_CTRL2: DSG/CHG)
- NTC 103AT 温度计算

#### ✅ CAN + Timer + WDG
- CAN 500kbps, FIFO0 中断接收
- TIM2 定时器 (1kHz)
- IWDG 独立看门狗封装

### 编译指标

| 指标 | 数值 |
|------|------|
| FLASH | ~26.7 KB / 64 KB (41.7%) |
| RAM | ~9.3 KB / 20 KB (46.5%) |
| 警告/错误 | 0 / 0 |
| BSP 模块 | 9 个 |
| App 模块 | 6 个 |

### 遇到的问题

| 问题 | 解决方案 |
|------|---------|
| usart_drv 未使用变量警告 | ring_buf_t 指针化 |
| i2c_sw HAL_GPIO_Init 开销大 | BSRR/BRR 直接寄存器操作 |
| wdg.c `__HAL_IWDG_GET_COUNTER` 不可用 | 直接读 IWDG->RLR |
| wdg.c `__HAL_RCC_CLEAR_FLAG` 无定义 | SET_BIT(RCC->CSR, RCC_CSR_RMVF) |
| timer.c 回调冲突 | 导出 `bsp_timer_irq_handler()` |

---

## Phase 3: App 应用层开发

**日期**: 2026-07-20 ~ 2026-07-22

### 创建的模块

#### protection — 保护判断
- 12 种故障检测: CELL_OV/UV, PACK_OV/UV, DISCHARGE_OC, CHARGE_OC, SHORT_CIRCUIT, OVER_TEMP, UNDER_TEMP, CELL_IMBALANCE, COMM_LOSS, WATCHDOG
- 3 级保护: NONE → WARNING → ALERT → FAULT
- 故障确认去抖计数器 (3 次进入 / 3 次恢复)

#### bms_shared — 共享数据中心
- 双 Mutex 保护: data_mutex (采集数据) + settings_mutex (可配置参数)
- 多任务间数据交换枢纽

#### can_cmd — CAN 协议解析
- 上行: 0x100/0x110/0x111/0x120/0x130
- 下行: 0x200/0x201/0x202
- can_pub() 周期上报

#### soc_ocv — SOC/OCV 估算
- 增强型安时积分 + OCV 查表校正
- 4 状态 FSM: IDLE → DISCHARGE → CHARGE → POLARIZATION
- Q_max 学习 (EMA)
- SOC 安全余量映射 (底部 3% 隐藏)

#### bms_app — 任务编排
- 6 个 FreeRTOS 任务 + 7 个同步原语

### 6 任务事件驱动架构

| 任务 | 优先级 | 周期 | 栈 | 职责 |
|------|--------|------|-----|------|
| can_rx_task | High | 事件驱动 | 2048B | CAN 指令接收 + 分发 |
| protection_task | AboveNormal | 10ms | 1024B | 保护检查 + FET 控制 |
| acquisition_task | Normal | 100ms | 2048B | BQ76940 数据采集 |
| can_tx_task | Normal | 100ms | 1024B | CAN 周期上报 |
| soc_task | Normal | 1000ms | 2048B | SOC/OCV 更新 |
| watchdog_task | Low | 500ms | 512B | 喂狗 + LED 心跳 |

---

## Phase 4: 架构重构 — 全栈重设计

**日期**: 2026-07-22 ~ 2026-07-26

### 背景
代码已超越初始规划文件。发现 `.spec-dev/2026-07-22-arch-refactor/` 中的完整架构重构设计。

### 4 层严格分层架构

```
Layer 0: Hardware  — STM32F103 + BQ76940 + CAN Transceiver
Layer 1: Core      — CubeMX HAL 骨架 + FreeRTOS 内核 (不含业务逻辑)
Layer 2: BSP       — 板级硬件抽象 (10 模块, DAG 无环)
Layer 3: App       — 纯策略/算法/协议 (零 BSP 依赖)
```

### 核心设计原则

| 原则 | 说明 |
|------|------|
| CubeMX 最小化 | 仅配置 3 个引脚 (PA8/PA15/PB2), 其余 BSP 代码管理 |
| BSP 纯硬件抽象 | 不依赖 App 层, 模块间 DAG 无环 |
| App 禁调 BSP | App 只做纯判断/纯协议, 通过返回值报告意图 |
| bms_app 唯一中转 | 任务函数是唯一同时调用 BSP 和 App 的代码 |
| 事件驱动保护 | task_sample 预检 → EventFlags → task_protect (Realtime 抢占) |

### ADR 决策

| ADR | 决策 | 理由 |
|-----|------|------|
| 0001 | 删除 data_report 模块 | CAN ID 冲突 + 零调用者 |
| 0002 | App 禁调 BSP | 分层纪律 + 可测试性 |
| 0003 | 保护阈值运行时配置 | CAN 配置命令需生效 |
| 0004 | 单 CAN 队列 + ring_buf 头插 | 省 ~200B RTOS 堆 |
| 0005 | EventFlags 驱动保护 + Realtime | 故障响应 < 10μs |

---

## Phase 5: 三 Spec 融合实施

**日期**: 2026-07-28 ~ 2026-07-29

### 覆盖 Spec

1. **bms-fullstack-redesign** (active) — 6 任务事件驱动架构
2. **soc-fsm** (active) — 4 状态机 SOC + Q_max 学习 + 安全余量
3. **sample-calib** (active) — ADC 校准 + NTC 103AT 查表 + 可配置 R_sense

### 变更范围

| 阶段 | 内容 |
|------|------|
| Phase 1 | 新增 ring_buf.h + bsp_common.h, 删除 usart_drv + data_report |
| Phase 2 | LED(PA15), io_ctrl(PA8), header guard 重命名 |
| Phase 3 | bq76940 重写 (ADC 校准/NTC/OCD/SCD/FET), can_drv ring_buf 化 |
| Phase 4 | soc_ocv 重写 (FSM + Q_max + 安全余量) |
| Phase 5 | bms_shared(扩展), protection(prot_result_t), can_cmd(can_action_req_t) |
| Phase 6 | bms_app 重写 (6 任务 + 7 同步原语 + ring_buf) |
| Phase 7 | Core 层 (.noinit 诊断, 移除 defaultTask, 初始化顺序) |

### 编译指标

| 指标 | 数值 |
|------|------|
| FLASH | 44.3 KB / 64 KB (67.6%) |
| RAM | 18.8 KB / 20 KB (91.6%) |
| 警告/错误 | 0 / 0 |
| 已删除文件 | usart_drv.c/h, data_report.c/h |
| 已重写文件 | 16 个 |

---

## Phase 6: BMS Qt 上位机开发

**日期**: 2026-08-06 ~ 2026-08-09

### Qt 上位机架构

- **框架**: Qt 6 + QML (Hybrid MVVM)
- **CAN**: PCAN-Basic API → CanWorker 后台线程
- **协议**: BmsProtocolDecoder (多帧批处理合并, 30ms 窗口)
- **数据模型**: BmsDataModel, CellVoltageModel, TemperatureModel, FaultListModel, CommunicationMonitor
- **QML 页面**: OverviewPage, CellsPage, TrendsPage, DeviceStatusPage, ControlPanel
- **日志**: CsvLogger (电压/温度/故障持续记录)

### 关键技术点

1. **多帧批处理合并**: PCAN-USB 将连续 CAN 帧切分为非确定性 USB chunks，上位机用 30ms 窗口累积追加不同 chunk 的帧
2. **CellBar 锚点布局**: anchor 定位替代 Column，柱状条底部固定向上增长，消除跳动闪烁
3. **DBC 辅助解码**: BmsDbcHelper 解析 CAN 消息信号

### 开发 spec

- `.spec-dev/2026-08-06-bms-qt-host/spec/bms-qt-host-design.md`
- ADR-0008: Qt Host Hybrid MVVM 架构

---

## Phase 7: 全链路调试与问题修复

**日期**: 2026-08-09 (约 3 小时, ~20 次编译-烧录-观察循环)

### 输入
Debug 目录 4 个串口录制文件 + 4 维度并行代码审查

### 症状
1. 系统持续重启 (有时初始化中途截断)
2. BQ76940 所有读数恒为 0mV/0mA, 但从未报 I2C 错误

### 🔴 根因 #1: BQ76940 数据全零 — TS1 Boot 脉冲缺失 + 浮空 SDA 假 ACK

**证据链**:
1. TS1 boot 脉冲从未发出 — `io_ctrl_set(IO_BQ_WAKE)` 零调用点
2. BQ76940 一直处于 SHIP 模式 (未唤醒)
3. 浮空 SDA → 开漏读 IDR 恒为 0 → `ack` 始终判为 ACK
4. 读回全 `0x00` 字节 → CRC8(0x00) = 0x00 "校验通过"
5. `bq76940_init()` 返回值被丢弃 → 无条件打印 "BQ76940 done"
6. 全零电压/电流 → `g_afe_online=1` → COMM_LOSS 永不触发

### 🔴 根因 #2: 系统随机复位 — 任务栈溢出 (4x 缩水)

**证据链**:
1. `cmsis_os2.c`: `osThreadNew` 的 `stack_size` 单位是**字节**, 内部 `/sizeof(StackType_t)` 转 words
2. 代码传 `512U` → 实际栈 = 128 words = 512 字节 (非声称的 2048B)
3. sample 任务栈上局部变量 > 280B + I2C 嵌套调用 → 栈极可能溢出
4. `configCHECK_FOR_STACK_OVERFLOW` 未定义 (默认关闭)
5. 非确定性崩溃 = 栈溢出的典型症状

### 🟡 次要发现

| # | 问题 | 严重度 |
|---|------|--------|
| 3 | 复位原因从未打印 (RCC->CSR 不读) | 诊断盲区 |
| 4 | I2C 引脚存疑 (PB10/11 vs PB8/9) | 需硬件确认 |
| 5 | 事件标志位冲突 (PROT_RECOVERED=0x0001 vs FAULT_CELL_OV) | P2 潜伏 |
| 6 | BQ76940 寄存器地址表大面积错误 (9 个地址错误) | 数据手册校验 |
| 7 | I2C bit-bang 无超时保护 | 低优先级 |

### 修复实施 (P0 优先)

| 优先级 | 修复 | 文件 |
|--------|------|------|
| 🔴 P0 | TS1 boot 脉冲 — 500μs 上升沿唤醒 | `bms_app.c` |
| 🔴 P0 | I2C 引脚修正 — PB10/11→PB8/9 | `i2c_sw.h` |
| 🔴 P0 | 栈大小恢复 — sample 2048B, soc 2048B, 其余 1024B | `bms_app.c` |
| 🔴 P0 | 栈溢出检测 — configCHECK_FOR_STACK_OVERFLOW=2 | `FreeRTOSConfig.h` |
| 🔴 P0 | defaultTask 栈 512→1024B | `freertos.c` |
| 🔴 P0 | I2C 内部上拉 — GPIO_PULLUP | `i2c_sw.c` |
| 🟡 P1 | 复位原因打印 — 读 RCC->CSR | `main.c` |
| 🟡 P1 | bq76940_init 返回值检查 | `bms_app.c` |
| 🟡 P1 | 非 CRC 模式 — 芯片为 BQ7694000 非 CRC 版本 | `bq76940.c` |
| 🟡 P1 | DEV_XRDY 轮询移除 — bit7=CC_READY 非设备就绪 | `bq76940.c` |
| 🟢 P2 | 寄存器地址表全修正 — 数据手册为准 | `bq76940.h` |
| 🟢 P2 | 位定义修正 — ADC_EN bit4, CC_EN→SYS_CTRL2 bit6 | `bq76940.h` |
| 🟢 P2 | set_protection 重写 — 单字节写入 | `bq76940.c` |
| 🟢 P2 | Cell 映射表 — 9-cell 非连续 VC 逐对读取 | `bq76940.c` |
| 🟢 P2 | 事件标志位冲突 — PROT_RECOVERED→bit15 | `bms_app.h` |

### 关键发现 (调试过程中的重要教训)

1. **BQ76940 不支持连续大块读取 (>2 字节)** — 后续字节全部返回 0xFF (NACK)
2. **芯片为非 CRC 版本** — 所有 CRC 读写均被拒绝
3. **硬件 I2C 引脚为 PB8/PB9** — 非 PB10/PB11
4. **寄存器映射完全符合 TI 数据手册** — 之前以为是 clone 芯片，实为批量读取 bug
5. **`osThreadNew` 栈参数是字节** — 给 512 实际只有 128 words

### 最终状态 (2026-08-09 12:50)

| 项目 | 状态 |
|------|------|
| 系统复位 | ✅ 无 IWDG 复位, 持续稳定 |
| 9 路电压 | ✅ 全部正确 (2.89~3.57V) |
| 总压 | ✅ 30.7V |
| 电流 | ✅ 130mA |
| CAN 通信 | ✅ 上位机 CSV 持续记录 |
| BQ76940 | ✅ 初始化成功 |
| 温度 | ⚠️ 读数 = 0 (待排查) |
| DSG | ⚠️ OFF (C4=2.79V < UV 2.80V, 正确保护) |

---

## Phase 8: 上位机 Bug 修复

**日期**: 2026-08-09 ~ 2026-08-10

### Bug 1: C1-C4 电芯显示不显示/清零

**症状**: 固件复位时 C1-C4 短暂显示正常, 随后全部变为 "---" / 灰条

**根因**: PCAN-USB 将连续 CAN 帧切分为非确定性 chunks, `BmsDataModel::onBatchReady()` 使用 `=` 替换而非追加:
- chunk1: [0x110] → pendingBatch = {0x110}
- chunk2: [0x111, 0x120, 0x130, 0x121] → pendingBatch **替换**为 {0x111, 0x120, 0x130, 0x121} → 0x110 丢失!
- 30ms 定时器触发 → C1-C4 = 0

**修复 (三层防御)**:

| 层 | 文件 | 修复 |
|----|------|------|
| 1 (主) | `BmsDataModel.cpp` | `=` 替换 → `append()` 累积合并 (上限 50 帧) |
| 2 (兜底) | `CellVoltageModel.cpp` | `mV==0 && old!=0` → 保留旧值 |
| 3 (可选) | 固件 `can_pub()` | 原子入队减少帧间间隙 |

### Bug 2: CellBar 跳动闪烁

**修复**: `CellBar.qml` 完全重写
- Column 布局 → anchor 定位
- cellLabel: `anchors.bottom: parent.bottom` (固定不动, 下方对齐)
- bar: `anchors.bottom: cellLabel.top` (底部固定, 向上增长)
- 动画: `SmoothedAnimation { duration: 150; velocity: 80 }` — 仅过渡高度, 不动画 y
- 无数据占位: voltage=0 → 灰条 + "---"

### 额外发现: OverviewPage 硬编码 "C?"
- 文件: `qml/OverviewPage.qml:87-89`
- 评级: 低优先级

---

## Phase 9: SOC 算法与硬件问题排查

**日期**: 2026-08-10

### 🔴 根因 #8: SOC 恒为 0 — cell_min 查旧 NMC 表 + C4 低压钳制

**证据链**:
1. `task_soc` 传入 `cell_min` = C4 = 2793mV
2. 旧 OCV 表最低点 {3000mV, 0‰} → `soc_ocv_lookup(2793)` clamp 返回 0
3. IDLE 状态 `|I|<200mA` → 每周期 OCV 修正覆盖 `soc_real=0`

**修复**:
- OCV 表: 通用 NMC 21 点 → SCV_SOC_Mapping.csv 31 点加密
- 参考电压: cell_min → total_mv/9 (平均电压)
- CC 不可用时强制 current=0 → 纯 OCV 模式

### 🔴 根因 #9: DEVICE_XREADY 硬件锁存 → FET/CC 不可用

**证据**:
1. `ctrl2=00 stat=20` — SYS_CTRL2 始终 0, SYS_STAT bit5 始终 1
2. 所有 I2C 写入 ACK 成功但寄存器即时恢复
3. 2s 延时/CC_CFG/CC_EN/CC_ONESHOT/100ms 自愈 — 全部无效
4. 问题在代码改动**前**已存在

**结论**: DEVICE_XREADY 在 I2C ACK 后微秒级重新锁存, 芯片硬件自动清零 SYS_CTRL2。需硬件排查: REGOUT/REGSRC 去耦电容、VC 输入滤波、BAT 供电稳定性。

### 🟡 发现 #10: SCV_SOC_Mapping.csv 电芯特性
- 3.5V → 通用 NMC ~200‰ / 真实 ~65‰ (差异巨大)
- 30-40% SOC 平台极平 (3755-3762mV, 仅 7mV 跨度)
- 5-10% SOC 膝部极陡 (3642→3344mV, 298mV 跨度)
- 满电 4198mV, 0% 截止 3000mV

### 🟡 发现 #11: 上位机 0x110 帧偶发丢失 (已在上位机侧防御)

### 编译指标 (最终状态)

| 指标 | 数值 |
|------|------|
| FLASH | 56.2 KB / 64 KB (87.8%) |
| RAM | 19.6 KB / 20 KB (95.8%) |
| 编译 | 0 错误 0 警告 |
| 修改文件 | 12 个 |

### 已知遗留问题

| 问题 | 原因 | 影响 |
|------|------|------|
| FET 控制不可用 | DEVICE_XREADY 硬件锁存 | 无法充放电控制 |
| 电流读取不可用 | CC 需 SYS_CTRL2 CC_EN (被清零) | 无安时积分 |
| SOC 纯 OCV 模式 | 无安时积分修正 | 精度受限 |
| 温度读数=0 | NTC 查表或 TS 字节序 | 温度保护不可用 |
| I2C 无超时保护 | bit-bang 无限阻塞 | 低概率挂死 |

---

## 关键发现汇总

### 硬件相关

| # | 发现 | 影响 | 日期 |
|---|------|------|------|
| F1 | BQ76940 不支持连续大块读取 (>2 字节) | 必须逐对 2 字节读 | 08-09 |
| F2 | 芯片为非 CRC 版本 (BQ7694000/4002) | CRC 读写被 NACK | 08-09 |
| F3 | I2C 硬件引脚为 PB8/PB9 (非 PB10/PB11) | 设计文档修正 | 08-09 |
| F4 | 寄存器映射完全符合 TI 数据手册 | 之前误判为 clone 芯片 | 08-09 |
| F5 | TS1 boot 脉冲通过二极管接法 (A→PA8, K→TS1) | 标准接法, 脉冲正常 | 08-09 |
| F6 | DEVICE_XREADY 硬件锁存 | FET/CC 物理不可用 | 08-10 |
| F7 | 电芯 OCV 曲线与通用 NMC 差异极大 | SOC 查表必须用真实数据 | 08-10 |

### 软件相关

| # | 发现 | 影响 | 日期 |
|---|------|------|------|
| F8 | `osThreadNew` 栈参数是字节, 内部 `/4` 转 words | 栈大小仅声称的 1/4 | 08-09 |
| F9 | 浮空 SDA 假 ACK + 假 CRC 校验 | 通信失败被静默掩藏 | 08-09 |
| F10 | 事件标志位冲突 (PROT_RECOVERED=bit0) | 修复前 OV 故障被误判为恢复 | 08-09 |
| F11 | PCAN USB chunk 边界切分 CAN 帧批次 | C1-C4 帧偶发丢失 | 08-10 |
| F12 | OCV 表边界 clamp 导致 SOC 恒为 0 | cell_min 低电压电芯绑架 SOC | 08-10 |

---

## ADR 决策记录索引

| ADR | 标题 | 日期 |
|-----|------|------|
| 0001 | 删除 data_report 模块 | 2026-07-22 |
| 0002 | App 层禁止调用 BSP, 任务函数中介 | 2026-07-22 |
| 0003 | 保护阈值从运行时配置读取 | 2026-07-22 |
| 0004 | 单 CAN 队列 + ring_buf 头插 | 2026-07-26 |
| 0005 | EventFlags 驱动保护 + Realtime 抢占 | 2026-07-26 |
| 0006 | I2C 错误任务优先级 | 2026-07-27 |
| 0007 | 电芯均衡故障/不均衡排除 | 2026-07-28 |
| 0008 | Qt Host Hybrid MVVM 架构 | 2026-08-06 |

> 完整 ADR 文档: `bms_9s_f103/bms_9s_f103/.spec-dev/adr/`

---

*最后更新: 2026-08-11 — 文档整理, 全时间线合并*
