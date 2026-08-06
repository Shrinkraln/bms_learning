# 进度日志

## 会话: 2026-07-26

### 全栈分层架构重新规划
- **状态:** complete
- **开始时间:** 2026-07-26
- 已执行操作:
  - 恢复上下文: 读取所有现有规划文件 + spec 设计文档 + ADR 记录
  - 发现代码已超越旧规划文件: 新增 bms_shared/soc_ocv/can_cmd 3 个 App 模块
  - 发现 `.spec-dev/2026-07-22-arch-refactor/` 中存在完整的架构重构设计
  - 从底层向上重新指定 4 层架构: Hardware → Core → BSP → App
  - 明确 CubeMX 职责边界: 管理所有硬件引脚/时钟/外设配置
  - 更新 task_plan.md: 完整层次图 + 模块清单 + 待完成 spec 变更
  - 更新 findings.md: 每个模块的详细功能文档 + CubeMX 配置 + 编译指标
  - 识别 18 个待完成的 spec 变更 (P0-P3 优先级分类)
- 已创建/修改的文件:
  - task_plan.md (重写 — 全栈 4 层架构 + 模块功能说明)
  - findings.md (重写 — 12 个模块详细文档 + ADR + CubeMX 边界)
  - progress.md (更新 — 本次会话记录)

## 会话: 2026-07-20

### Phase 1: 需求分析与规划
- **状态:** complete
- 已执行操作:
  - 初始化规划文件系统
  - 分析项目结构和 CMakeLists.txt 开发步骤
  - 识别为 STM32F103 + FreeRTOS + BQ76940 的 9 串 BMS
- 已创建/修改的文件:
  - task_plan.md, findings.md, progress.md (创建)

### Phase 2: BSP 基础模块 (systick + LED)
- **状态:** complete
- 已执行操作:
  - 编写 systick.h/c — DWT 微秒延时 + HAL 毫秒封装 + 超时计算
  - 重写 led.h/c — 枚举 API, HAL GPIO, 低电平点亮适配
  - 编译通过 (0 警告 0 错误)
- 已创建/修改的文件:
  - BSP/Inc/systick.h, BSP/Src/systick.c (创建)
  - BSP/Inc/led.h, BSP/Src/led.c (重写)
  - BSP/Src/bsp_placeholder.c (删除)
  - CMakeLists.txt (更新)

### Phase 3: GPIO 控制 (io_ctrl)
- **状态:** complete
- 已执行操作:
  - 编写 io_ctrl.h/c — 6 路控制 IO 封装, 枚举 API, 批量断电
  - 编译通过 (0 警告 0 错误)
- 已创建/修改的文件:
  - BSP/Inc/io_ctrl.h, BSP/Src/io_ctrl.c (创建)
  - CMakeLists.txt (更新)

### Phase 4: 通信接口 (i2c_sw + usart_drv)
- **状态:** complete
- 已执行操作:
  - 编写 i2c_sw.h/c — 软件 I2C + CRC8, 直接寄存器操作
  - 编写 usart_drv.h/c — 中断接收环形缓冲区, 双串口, printf 支持
  - 编译通过 (0 警告 0 错误)
- 已创建/修改的文件:
  - BSP/Inc/i2c_sw.h, BSP/Src/i2c_sw.c (创建)
  - BSP/Inc/usart_drv.h, BSP/Src/usart_drv.c (创建)
  - CMakeLists.txt (更新)

### Phase 5: BQ76940 驱动核心
- **状态:** complete
- 已执行操作:
  - 编写 bq76940.h/c — 完整驱动 (初始化/电压/温度/电流/保护/均衡/CRC)
  - 编译通过 (0 警告 0 错误)
- 已创建/修改的文件:
  - BSP/Inc/bq76940.h, BSP/Src/bq76940.c (创建)
  - CMakeLists.txt (更新)

### Phase 6: CAN + 定时器 + 看门狗
- **状态:** complete
- 已执行操作:
  - 编写 can_drv.h/c — 500kbps CAN, FIFO0 中断接收
  - 编写 timer.h/c — TIM2 定时器
  - 编写 wdg.h/c — IWDG 封装
  - 修复 timer 回调冲突 (导出 bsp_timer_irq_handler)
  - 修复 wdg F1 HAL 兼容问题
  - 编译通过 (0 警告 0 错误)
- 已创建/修改的文件:
  - BSP/Inc/can_drv.h, BSP/Src/can_drv.c (创建)
  - BSP/Inc/timer.h, BSP/Src/timer.c (创建)
  - BSP/Inc/wdg.h, BSP/Src/wdg.c (创建)
  - Core/Src/main.c (修改 — timer 分发)
  - CMakeLists.txt (更新)

### Phase 7: App 应用层
- **状态:** partial (基础实现完成, spec 对齐待完成)
- 已执行操作:
  - 编写 protection.h/c — 12 种故障检测, 3 级保护
  - 编写 data_report.h/c — CAN + 串口数据上报 (待删除)
  - 编写 bms_app.c — 6 任务编排, 100Hz 主循环
  - 编写 bms_shared.h/c — 共享数据中心 (双 mutex)
  - 编写 can_cmd.h/c — CAN 协议 + 指令分发
  - 编写 soc_ocv.h/c — 增强型安时积分 + OCV 查表
  - 编译通过 (0 警告 0 错误)
- 待完成:
  - 删除 data_report 模块
  - protection_check 改签名
  - can_cmd_dispatch 返回 action
  - 任务创建移入 freertos.c
- 已创建/修改的文件:
  - App/Inc/protection.h, App/Src/protection.c (创建)
  - App/Inc/data_report.h, App/Src/data_report.c (创建, 待删除)
  - App/Inc/bms_app.h, App/Src/bms_app.c (创建)
  - App/Inc/bms_shared.h, App/Src/bms_shared.c (创建)
  - App/Inc/can_cmd.h, App/Src/can_cmd.c (创建)
  - App/Inc/soc_ocv.h, App/Src/soc_ocv.c (创建)
  - CMakeLists.txt (更新)

### Phase 8: 编译验证
- **状态:** complete (当前版本编译通过)
- 已执行操作:
  - 全量编译, 0 警告 0 错误
  - GCC size: FLASH ~26.7KB, RAM ~9.3KB

## 最终成果汇总 (2026-07-20 开发阶段)

| 指标 | 数值 |
|------|------|
| 新增/重写源文件 | 20+ 个 (.h + .c) |
| BSP 模块 | 9 个 |
| App 模块 | 6 个 (1 个待删除) |
| 编译警告 | 0 |
| 编译错误 | 0 |
| FLASH 占用 | 26.7 KB / 64 KB (41.7%) |
| RAM 占用 | 9.3 KB / 20 KB (46.5%) |

## 错误日志
| 时间戳 | 错误 | 尝试次数 | 解决方案 |
|--------|------|----------|----------|
| 2026-07-20 | usart_drv 未使用变量警告 | 1 | ring_buf_t 指针化 |
| 2026-07-20 | i2c_sw HAL_GPIO_Init 开销 | 1 | BSRR/BRR 直接寄存器操作 |
| 2026-07-20 | wdg.c __HAL_IWDG_GET_COUNTER | 1 | IWDG->RLR 直接读取 |
| 2026-07-20 | wdg.c __HAL_RCC_CLEAR_FLAG | 1 | SET_BIT(RCC->CSR, RCC_CSR_RMVF) |
| 2026-07-20 | timer.c 回调冲突 | 1 | 导出 bsp_timer_irq_handler() |

## 会话: 2026-07-29

### 三 Spec 融合实施 — 7 阶段全量重写
- **状态:** complete
- **开始时间:** 2026-07-29
- **覆盖 Spec:**
  - `bms-fullstack-redesign` (active) — 6 任务事件驱动架构
  - `soc-fsm` (active) — 4 状态机 SOC + Q_max 学习 + 安全余量映射
  - `sample-calib` (active) — ADC GAIN/OFFSET 校准 + NTC 103AT 查表 + 可配置 R_sense
- **已执行操作:**
  - Phase 1: 创建 ring_buf.h + bsp_common.h, 删除 usart_drv + data_report, CMakeLists 更新
  - Phase 2: BSP 重写 — LED(PA15 单), io_ctrl(PA8 单), header guard 重命名
  - Phase 3: bq76940 重写 (ADC 校准 + NTC 33 点查表 + OCD/SCD + FET SYS_CTRL2) + can_drv ring_buf 化
  - Phase 4: soc_ocv 重写 — 4 状态 FSM (IDLE→DISCHARGE/CHARGE→POLARIZATION), Q_max EMA, SOC 安全余量
  - Phase 5: bms_shared (扩展 settings_t), protection (prot_result_t, 零 BSP), can_cmd (can_action_req_t, can_pub)
  - Phase 6: bms_app 重写 — 6 任务 + 7 同步原语 + ring_buf, 锁层级 mutex_iic→mutex_data
  - Phase 7: Core 层 — stm32f1xx_it.c (TIM2 信号量 + .noinit 故障诊断), freertos.c (移除 defaultTask), main.c (初始化顺序), linker script (.noinit 段)
- **编译结果:** 0 错误 0 警告
- **内存:**
  - FLASH: 44,268 B / 64 KB (67.55%)
  - RAM: 18,768 B / 20 KB (91.64%)
- **已创建文件:**
  - BSP/Inc/ring_buf.h, bsp_common.h
- **已删除文件:**
  - BSP/Inc/usart_drv.h, BSP/Src/usart_drv.c
  - App/Inc/data_report.h, App/Src/data_report.c
- **已重写文件 (16):**
  - BSP: led.h/c, io_ctrl.h/c, bq76940.h/c, can_drv.h/c
  - App: soc_ocv.h/c, protection.h/c, bms_shared.h/c, can_cmd.h/c, bms_app.h/c
- **已修改文件:**
  - BSP/Inc/i2c_sw.h, systick.h, timer.h, wdg.h (header guard)
  - BSP/Src/i2c_sw.c (GPIOB clock enable)
  - Core/Src/stm32f1xx_it.c (TIM2 semaphore + .noinit fault diag)
  - Core/Src/freertos.c (remove defaultTask)
  - Core/Src/main.c (init order: bms_app_init after osKernelInitialize)
  - CMakeLists.txt (remove usart_drv + data_report)
  - STM32F103XX_FLASH.ld (.noinit section)

## 最终成果汇总 (2026-07-29 三 Spec 实施)

| 指标 | 数值 |
|------|------|
| FLASH 占用 | 44.3 KB / 64 KB (67.6%) |
| RAM 占用 | 18.8 KB / 20 KB (91.6%) |
| 编译警告 | 0 |
| 编译错误 | 0 |
| BSP 模块 | 9 个 (+ ring_buf, bsp_common; - usart_drv) |
| App 模块 | 5 个 (- data_report) |
| FreeRTOS 任务 | 6 (新事件驱动架构) |
| 同步原语 | 7 (3 mutex + 2 event + 1 semaphore + 1 ring_buf) |
| CAN 帧格式 | 5 种上行 + 3 种下行 |

## 错误日志
| 时间戳 | 错误 | 尝试次数 | 解决方案 |
|--------|------|----------|----------|
| 2026-07-29 | bsp_common.h typedef 与模块自带 enum 冲突 (bq76940/i2c_sw/timer/wdg) | 1 | 从 bsp_common.h 移除冲突 typedef |
| 2026-07-29 | bms_settings_t 在 protection.h 中 unknown type | 1 | 调整 include 顺序: bms_shared.h 先定义 bms_settings_t 再包含 protection.h |
| 2026-07-20 | usart_drv 未使用变量警告 | 1 | ring_buf_t 指针化 |
| 2026-07-20 | i2c_sw HAL_GPIO_Init 开销 | 1 | BSRR/BRR 直接寄存器操作 |
| 2026-07-20 | wdg.c __HAL_IWDG_GET_COUNTER | 1 | IWDG->RLR 直接读取 |
| 2026-07-20 | wdg.c __HAL_RCC_CLEAR_FLAG | 1 | SET_BIT(RCC->CSR, RCC_CSR_RMVF) |
| 2026-07-20 | timer.c 回调冲突 | 1 | 导出 bsp_timer_irq_handler() |

## 五问重启测试
| 问题 | 答案 |
|------|------|
| 我在哪里？ | **完成** — 三个 spec 全部实施完毕 |
| 我要去哪里？ | 硬件验证 + 参数调优 |
| 目标是什么？ | 4 层严格分层、零违规、spec 与代码一致 — **已达成** |
| 我学到了什么？ | 见 findings.md + 下方错误日志 |
| 我做了什么？ | 7 阶段全量重写 29+ 文件, 实现 6 任务事件驱动 BMS |

---

*每个阶段完成后或遇到错误时更新*
