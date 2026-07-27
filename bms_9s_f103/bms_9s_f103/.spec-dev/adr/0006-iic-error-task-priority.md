# ADR-0006: I2C 通信故障由独立最高优先级任务处理

**日期**: 2026-07-27
**状态**: Accepted

## 背景

BMS 系统中 BQ76940 通过 I2C 与 MCU 通信。I2C 总线故障（超时/断开）是最高严重级别的故障——MCU 完全失去电池数据视野。在 bms-fullstack-redesign 的 6 任务架构中，故障处理统一由 task_protect（优先级 4）负责。但 I2C 通信丢失有其特殊性：task_protect 的响应动作（写 BQ76940 寄存器控制 FET）在 I2C 中断时必然失败。

## 决策

新增 task_iic_error（优先级 5），独立于 task_protect（优先级 4），专门处理 I2C 通信故障。

**任务优先级分层**:
```
5  task_iic_error  ← NEW: I2C 通信故障上报
4  task_protect    ← 其他故障处理 (电压/电流/温度)
3  task_can_rx     ← CAN 指令接收
3  task_sample     ← 数据采集 + 保护预检
2  task_balance    ← 均衡控制
2  task_soc        ← SOC/OCV 更新
1  task_can_tx     ← 周期上报 + 喂狗
```

**同步原语**: 二元信号量 `sem_iic`，由 bq76940 层在通信状态变化时 release。

## 理由

1. **严重性优先**: 通信丢失 > 任何单一电芯故障。MCU 失明时必须优先通知上位机。
2. **职责隔离**: task_iic_error 只做 CAN 上报 + LED，不操作 I2C。task_protect 继续处理需要 I2C 写入的 FET 控制。两者逻辑完全不重叠。
3. **抢占保证**: task_iic_error 优先级 5 高于 task_protect(4)，通信故障上报不会被正在执行 FET 写操作的 task_protect 阻塞。
4. **信号量 > 事件组**: 仅两种状态（故障/恢复），二元信号量更轻量（~40B vs ~60B），语义更直接。

## 备选方案（已拒绝）

- **不加新任务，task_protect 内部分支处理**: 需要在 task_protect 中区分 COMM_LOSS（跳过 I2C）和其他故障（写 I2C），复杂度增加；且 task_protect 在执行 FET 写时可能被阻塞在 mutex_iic 上，延迟 COMM_LOSS 上报。
- **用事件组唤醒**: 用户最初提议用 evt_iic 事件组。讨论后认定二元信号量更合适——无需区分多个事件位。
