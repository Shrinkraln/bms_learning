---
spec_dev:
  feature: bms-debug-bringup
  covers: ["Core/Src/main.c", "Core/Src/stm32f1xx_it.c", "Core/Inc/stm32f1xx_it.h", "BSP/Src/usart_drv.c", "BSP/Src/can_drv.c"]
  status: draft
---

# BMS 固件逐步调试验证方案

## 背景

BMS 固件（STM32F103 + FreeRTOS + BQ76940 + CAN）经历多次重构后，USART/CAN 模块在最近一次 commit 中被成片删除。
代码已部分恢复，但系统从未从"最小裸 HAL"开始逐步验证过每个外设是否正常工作。
需要一个从零构建、每步可观测的调试验证流程。

## 目标

将固件从最小 HAL 初始化开始，7 步增量构建到完整 FreeRTOS 系统运行。
每一步只加一个模块的 init + 一条可观测验证输出，编译 → 烧录 → 确认通过 → 进入下一步。

## 验证顺序

```
Step 1: HAL + Clock + GPIO   → LED 闪烁 3 次
Step 2: USART1 + printf       → 串口收到 "USART1 OK"
Step 3: LED + USART 联动      → printf 同步输出 LED toggle
Step 4: I2C1                  → BQ76940 地址 0x08 扫描
Step 5: BQ76940 数据采集       → printf 电芯电压
Step 6: CAN + ISR 修复         → 自收发 loopback 验证
Step 7: FreeRTOS 全系统        → CAN 周期数据帧 + LED 点亮
```

## 设计决策

### 决策 1: 逐步独立固件

每步编译烧录一个独立固件，USART1 输出验证信息。
**理由**: 问题隔离——某步失败时错误源唯一（当步新增的 ~20 行代码），无需在全系统中排查。

### 决策 2: 合并 HAL+Clock+GPIO 为 Step 1

HAL_Init / SystemClock_Config 无外设可用，必须等 MX_GPIO_Init 后才能用 LED 做首次可观测验证。
**理由**: 三个函数调用无外部依赖、无分支逻辑，合并不会模糊错误定位。

### 决策 3: Step 2 用直接 HAL_UART_Transmit + __io_putchar，不用 usart_drv

usart_drv 需要中断 RX + 环形缓冲区支持，且当前有 USART2 符号缺失的 bug。
早期步骤只需 printf 发字符串，不需要接收。
**理由**: 解耦——printf 实现只需要 4 行 `__io_putchar`；usart_drv 的 bug 不影响 Step 2-6，在 Step 7 统一修复。

### 决策 4: CAN 验证用 Silent Loopback 模式

Step 6 将 CAN 模式临时切为 `CAN_MODE_SILENT_LOOPBACK`，帧内部回环不发送到总线。
**理由**: 不依赖外部 CAN 收发器和总线上的其他节点，纯软件验证驱动层和 ISR 链路。

### 决策 5: 保持 CubeMX USER CODE 格式

所有修改在 `USER CODE BEGIN/END` 标记内，不覆盖 CubeMX 生成的代码块。
**理由**: 后续 CubeMX 重新生成代码时不会覆盖手动修改。

## 术语表

| 术语 | 定义 | 避免使用 |
|------|------|----------|
| Step | 一次编译-烧录-验证的独立固件版本 | Phase, 阶段 |
| 可观测验证 | 人可直接感知的确认手段（LED 闪烁 / 串口字符串 / 调试器断点） | 内部检查 |
| Silent Loopback | CAN 控制器内部回环模式：TX→RX 连接在控制器内部，不驱动总线引脚 | 回环测试 |

## Requirements

### Step 1: HAL + 时钟 + GPIO → LED 验证

#### Requirement: S1-LED-BLINK
固件 SHALL 在 HAL_Init / SystemClock_Config / MX_GPIO_Init 完成后使 LED(PA15) 以 500ms 间隔闪烁 3 次。

##### Scenario: 上电后 LED 闪烁
- GIVEN 固件仅包含 HAL_Init、SystemClock_Config、MX_GPIO_Init 和 led_init/led_toggle/HAL_Delay 调用
- WHEN MCU 上电或复位
- THEN LED(PA15) 以约 500ms 间隔亮灭 3 次后熄灭

### Step 2: USART1 + printf

#### Requirement: S2-USART1-PRINTF
固件 SHALL 在 MX_USART1_UART_Init 后通过 `printf("USART1 115200 OK\r\n")` 向 PA9(TX) 输出验证字符串。

##### Scenario: 串口输出验证字符串
- GIVEN 固件包含 Step 1 全部 init + MX_USART1_UART_Init + `__io_putchar` 实现
- WHEN MCU 上电
- THEN PC 端串口工具 (115200-8N1) 收到 "USART1 115200 OK" 后跟 CR+LF

#### Requirement: S2-USART2-REMOVAL
usart_drv.c SHALL 移除所有 USART2(USART_ID_2) 相关代码，包括 huart2 引用、case 分支、usart2_rx_data/rb 静态变量。

##### Scenario: 编译通过
- GIVEN usart.c 仅定义了 huart1（USART1 单实例）
- WHEN 编译包含 usart_drv.c 的固件
- THEN 无 "huart2 undeclared" 链接错误

### Step 3: LED + USART 联动

#### Requirement: S3-LED-SERIAL-SYNC
固件 SHALL 在 LED 快闪 5 次（100ms 间隔）的同时，每次通过 printf 输出 "LED toggle OK"。

##### Scenario: LED 闪烁与串口同步
- GIVEN Step 2 固件 + LED toggle 循环
- WHEN MCU 上电
- THEN LED 闪烁 5 次且串口收到 5 行 "LED toggle OK"

### Step 4: I2C1 总线扫描

#### Requirement: S4-I2C-SCAN
固件 SHALL 在 MX_I2C1_Init 后扫描 BQ76940 I2C 地址 (0x08)，通过 printf 输出扫描结果。

##### Scenario: BQ76940 在线
- GIVEN BQ76940 已上电且 I2C 连接正常
- WHEN MCU 执行 HAL_I2C_IsDeviceReady(&hi2c1, 0x08<<1, 3, 100)
- THEN 串口输出 "I2C 0x08: OK"

##### Scenario: BQ76940 离线
- GIVEN BQ76940 未上电或 I2C 断开
- WHEN MCU 执行 HAL_I2C_IsDeviceReady
- THEN 串口输出 "I2C 0x08: FAIL"

### Step 5: BQ76940 数据采集

#### Requirement: S5-BQ76940-READ
固件 SHALL 初始化 BQ76940 并执行一次 bq76940_read_all，通过 printf 输出 pack 总电压和前 9 节电芯电压。

##### Scenario: 读取电芯电压
- GIVEN BQ76940 在线且至少连接 9 节电芯
- WHEN bq76940_read_all(&data) 返回 BQ76940_OK
- THEN 串口输出 "Pack: XXXXX mV" 和 "Cell[0..8]: XXXX XXXX ..."

### Step 6: CAN 总线验证

#### Requirement: S6-CAN-ISR-FIX
stm32f1xx_it.c SHALL 包含 USB_LP_CAN1_RX0_IRQHandler 实现，调用 HAL_CAN_IRQHandler(&hcan)；stm32f1xx_it.h SHALL 声明该函数。

##### Scenario: CAN RX0 中断被正确处理
- GIVEN CAN 过滤器已配置且 FIFO0 中断已使能
- WHEN CAN 帧到达 FIFO0
- THEN HAL_CAN_RxFifo0MsgPendingCallback 被调用，帧进入 ring_buf

#### Requirement: S6-CAN-LOOPBACK
固件 SHALL 在 Silent Loopback 模式下发送一帧并立即从 ring_buf 读到同一帧，通过 printf 输出 ID 和 data。

##### Scenario: CAN 自收发成功
- GIVEN CAN 配置为 CAN_MODE_SILENT_LOOPBACK
- WHEN can_send(&tx_msg) 发送 ID=0x200 的测试帧
- THEN can_recv(&rx_msg) 返回同一帧，串口输出 "CAN OK: ID=0x200"

### Step 7: FreeRTOS 全系统

#### Requirement: S7-FREERTOS-FULL
固件 SHALL 恢复完整的 main.c（osKernelInitialize → usart_drv_init → bms_app_init → MX_FREERTOS_Init → osKernelStart），FreeRTOS 启动后 task_can_tx 正常发送周期 CAN 帧。

##### Scenario: 全系统运行
- GIVEN 固件包含所有 7 步的全部 init 和 FreeRTOS 调度器
- WHEN osKernelStart 启动调度
- THEN LED 常亮，CAN 总线每 100ms 有一帧周期数据 (0x110/0x111/0x120/0x121/0x130)
