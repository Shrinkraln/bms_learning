# ADR-0004: 单 CAN 发送队列 + ring_buf 头插

## 背景

task_protect 产生紧急 CAN 帧需要优先于周期上报帧发送。方案选型:
- **A) 双 osMessageQueue** (urgent + normal) — task_can_tx 先查 urgent 再查 normal
- **B) 单 osMessageQueue** + 每帧带优先级字段 — O(n) 扫描或 O(n) 排序插入
- **C) 单 ring_buf** + 新增 `ring_buf_put_front()` 头插接口

## 决定

采用方案 C — 单 ring_buf + 头插。

## 理由

- 功能与双队列完全等价（在不溢出前提下），task_can_tx 统一 `ring_buf_get()` 即可
- 比 A 少一个 RTOS 对象（节省 ~200 字节 FreeRTOS 堆 + 队列控制块）
- 比 B 简单 — O(1) 入队出队，无需扫描或排序
- ring_buf 不依赖 FreeRTOS（纯 C 静态数组），不受堆碎片影响
- 深度 24 帧在 100ms 产生 ~5 帧/周期的工况下远不会满，溢出风险可忽略

## 被否方案

- **双 osMessageQueue**: 多占 RTOS 堆 + 代码更复杂（两个 get 调用）
- **单队列+优先级字段**: O(n) 扫描、实现复杂、没有实际收益（只有 2 级优先级）
