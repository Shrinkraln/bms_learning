# ADR-0002: App 模块禁止调用 BSP，硬件操作由任务函数中转

## 背景

审查发现 4 处 App 层直接调用 BSP 层的代码：

| 位置 | BSP 调用 |
|------|----------|
| protection.c | `io_ctrl_power_off_all()`、`bsp_tick_get()` |
| can_cmd.c | `bq76940_set_balancing()`、`bq76940_balance_off()`、`bq76940_shutdown()` |

这违反了 Core → BSP → App 的分层约定，导致 App 模块无法脱离硬件做单元测试。

项目中 `soc_ocv` 模块已示范了正确模式：纯算法、零 BSP 依赖、通过 `bms_shared` 交换数据。

## 决定

所有 App 模块（protection、can_cmd）只负责策略/算法/协议解析，不调用任何 BSP 函数。硬件操作通过返回值或输出参数向任务函数（bms_app.c 中的 protection_task、can_cmd_task_entry 等）报告意图，由任务函数调用 BSP。

具体机制：
- protection：返回 `prot_result_t`（含 `request_power_off` 标志），任务层调 `io_ctrl_power_off_all()`
- can_cmd：返回 `can_action_req_t`（含 action 枚举），任务层调 `bq76940_set_balancing()` 等

## 理由

- **可测试性**：App 模块脱离硬件后可在 PC 上用 host 编译器做单元测试
- **单一职责**：App 模块只做"想什么"，任务函数只做"做什么"
- **已验证模式**：soc_ocv 采用此模式且运行正确
- **改动量可控**：仅需增加两个小型返回结构体（prot_result_t ≤ 6 字节，can_action_req_t ≤ 4 字节），任务函数增加 switch/case 分支

## 被否方案

- **保持现状 + 文档标记为例外**：违反单一职责；层次违规会持续扩散，每一个新 App 模块都会复制此模式；无法做 host 单元测试
