# ADR-0001: 删除 data_report 模块

## 背景

`data_report` 模块（`App/Inc/data_report.h`、`App/Src/data_report.c`）定义了 CAN 数据上报函数（`data_report_all`/`can`/`serial`）和 CAN ID 宏（0x100–0x120）。它与 `can_cmd` 模块的 CAN ID 定义存在冲突（0x112 语义不同），且其所有发送函数在全代码库中零调用者。`data_report_init()` 为空函数。

## 决定

删除 `data_report` 模块（两个文件），从 CMakeLists.txt 和 `bms_app_init()` 中移除其引用。CAN 数据上报统一由 `can_cmd` 模块负责。

## 理由

- CAN ID 冲突（0x112）是一个等待触发的 bug
- 死代码占用 FLASH 空间且增加维护成本
- `can_cmd` 已独立实现了相同的 CAN 上报功能（can_tx_task_entry 发送 0x100/0x110-0x112/0x120/0x130），功能完整覆盖
- 并行维护两套 CAN ID 定义容易引入不一致

## 被否方案

- **合并到 can_cmd**：data_report 的串口调试上报功能可合并，但目前无调用场景，合并后仍是死代码。需要时再从 can_cmd 中提取串口功能。
- **保留但停用**（注释 CMakeLists + 移除 init 调用）：源码留在仓库中会继续与 can_cmd 的 CAN ID 定义冲突，且给人"此模块可用"的误导。
