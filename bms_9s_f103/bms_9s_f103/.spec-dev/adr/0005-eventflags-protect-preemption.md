# ADR-0005: EventFlags 驱动 protect + osPriorityRealtime 抢占

## 背景

BQ76940 ALERT 引脚未连 MCU (PB2 用作 BOOT1)。保护检测必须由 MCU 软件轮询实现。需要设计 task_protect 的触发机制和优先级策略。

## 决定

task_sample (prio 32, AboveNormal) 轮询 BQ76940 寄存器 + 阈值检查 → 如有异常 → `osEventFlagsSet(evt_protect, fault_bit)` → task_protect (prio 48, Realtime) 从 `osEventFlagsWait` 被立即唤醒并抢占 task_sample。

## 理由

- protect 最高优先级 (osPriorityRealtime) 意味着故障时响应延迟 = 上下文切换时间 (< 10µs)
- EventFlags 是 CMSIS-RTOS2 标准原语，task_sample 只 set 不 wait，task_protect 只 wait 不 set，单向信号无竞态
- 比 task_protect 周期性轮询省 CPU（事件驱动 vs 每 10ms 轮询）
- BQ76940 硬件保护在 µs 级作为第一层，软件保护在 ms 级作为第二层冗余
- 故障恢复也经 protect 确认后才清除 EventFlags 位，对称设计

## 被否方案

- **protect 周期性轮询 mutex_data**: 浪费 CPU 在无故障期间；且 protect 需要高优先级，会频繁抢占其他任务导致整体吞吐量下降
- **sample 直接调 protect (函数调用)**: protect 执行期间阻塞 sample，数据采集停止；违反分层纪律（sample 不应知道 protect 的存在）
