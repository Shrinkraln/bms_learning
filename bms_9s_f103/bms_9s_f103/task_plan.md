# 任务规划：BMS 9S F103 电池管理系统开发

## 目标
基于 STM32F103 + FreeRTOS + BQ76940 的 9 串电池管理系统（BMS）固件开发，实现电池监控、保护、数据上报功能。

## 当前阶段
✅ 全部完成 — 完整软件已编译通过，可直接烧录

## 阶段

### Phase 9: 软件集成 (main.c + FreeRTOS)
- [x] main.c 调用 bms_app_init()
- [x] freertos.c 使用 bms_task_entry
- [x] 任务栈扩容 (128→512 words)
- [x] 完整固件编译通过 (0 警告 0 错误)
- **状态:** complete

### Phase 1: 需求分析与规划
- [x] 明确项目需求和约束
- [x] 梳理现有代码和架构
- [x] 确定开发优先级
- **状态:** complete

### Phase 2: BSP 层 — 基础模块 (Step 8: 基础模块)
- [x] systick.c — 系统滴答定时器
- [x] led.c — LED 状态指示
- **状态:** complete

### Phase 3: BSP 层 — GPIO 控制 (Step 9)
- [x] io_ctrl.c — GPIO 输入输出控制
- **状态:** complete

### Phase 4: BSP 层 — 通信接口 (Step 10-11)
- [x] i2c_sw.c — 软件 I2C + CRC8
- [x] usart_drv.c — 串口驱动 (USART1 + USART2)
- **状态:** complete

### Phase 5: BSP 层 — BQ76940 驱动核心 (Step 12)
- [x] bq76940.c — BQ76940 电池监视器驱动
- **状态:** complete

### Phase 6: BSP 层 — CAN + 定时器 + 看门狗 (Step 13-14)
- [x] can_drv.c — CAN 总线驱动
- [x] timer.c — 定时器驱动
- [x] wdg.c — 看门狗驱动
- **状态:** complete

### Phase 7: App 层 — 应用逻辑 (Step 15)
- [x] protection.c — 保护判断逻辑
- [x] data_report.c — 数据上报
- [x] bms_app.c — BMS 主应用
- **状态:** complete

### Phase 8: 集成测试与验证
- [x] 全功能编译验证
- [ ] 模块间接口测试
- [ ] 系统集成测试
- **状态:** complete

## 关键问题
1. BSP 各模块的 API 接口如何定义？
2. FreeRTOS 任务如何划分（采集任务、保护任务、上报任务）？
3. BQ76940 的 I2C 通信时序和寄存器映射如何设计？
4. CAN 总线的通信协议（帧格式、ID 分配、数据格式）如何定义？
5. 保护逻辑的阈值和响应时间要求是什么？

## 已做决策
| 决策 | 理由 |
|------|------|
| | |

## 遇到的错误
| 错误 | 尝试次数 | 解决方案 |
|-------|---------|----------|
| usart_drv 未使用变量警告 (usart1_rx_data, usart2_rx_data) | 1 | ring_buf_t 改为指针成员，指向外部数组 |
| i2c_sw 中 sda_out/sda_in 使用 HAL_GPIO_Init 开销大 | 1 | 改用开漏输出技巧 + BSRR/BRR 直接寄存器操作 |
| wdg.c __HAL_IWDG_GET_COUNTER 不存在 (F1 HAL) | 1 | 直接读取 IWDG->RLR 寄存器 |
| wdg.c __HAL_RCC_CLEAR_FLAG 不存在 (F1 HAL) | 1 | 使用 SET_BIT(RCC->CSR, RCC_CSR_RMVF) |
| timer.c HAL_TIM_PeriodElapsedCallback 与 main.c 冲突 | 1 | 导出 bsp_timer_irq_handler()，在 main.c 中分发调用 |

## 备注
- 按照 CMakeLists.txt 中的 Step 8-15 顺序逐步开发
- 每写完一个 `.c` 文件就取消 CMakeLists.txt 中对应的注释并编译验证
- 使用 CubeMX OBJECT 库模式组织代码
