---
spec_dev:
  feature: arch-refactor
  covers:
    - App/Src/protection.c
    - App/Inc/protection.h
    - App/Src/can_cmd.c
    - App/Inc/can_cmd.h
    - App/Src/bms_app.c
    - App/Inc/bms_app.h
    - App/Src/bms_shared.c
    - App/Inc/bms_shared.h
    - App/Src/soc_ocv.c
    - App/Src/data_report.c
    - App/Inc/data_report.h
    - BSP/Src/bq76940.c
    - BSP/Src/can_drv.c
    - BSP/Inc/can_drv.h
    - BSP/Src/usart_drv.c
    - BSP/Inc/usart_drv.h
    - BSP/Src/i2c_sw.c
    - BSP/Inc/ring_buf.h
    - BSP/Inc/bsp_common.h
    - Core/Src/freertos.c
    - Core/Src/stm32f1xx_it.c
    - Core/Inc/FreeRTOSConfig.h
    - CMakeLists.txt
  status: draft
---

# BMS 9S F103 全栈分层架构重构

## 目标

对现有 Core → BSP → App 三层架构做全面审查与重构，建立严格分层纪律：
- **Core**：仅 CubeMX 生成的 HAL 配置与 FreeRTOS 内核设定，不含业务逻辑
- **BSP**：纯硬件抽象层，不依赖 App 层，模块间依赖无环
- **App**：纯策略、算法与协议解析，不直接操作硬件（不调用 BSP 函数、不访问 HAL 寄存器）
- **任务编排**（bms_app.c 的任务函数）：唯一有权调用 BSP 的代码，集中处理硬件操作

以 `soc_ocv` 模块为设计标杆——它已是严格分层的范例（零 BSP 依赖、零 HAL 调用、纯算法）。

## 术语

| 术语 | 定义 | Avoid |
|------|------|-------|
| 任务编排 | 在任务函数中协调 App 策略模块的输出与 BSP 硬件操作的执行 | 调度、dispatch |
| 分层违规 | App 层代码直接调用 BSP 层函数或访问 HAL 寄存器 | 跨层调用、越界访问 |
| action 请求 | App 协议模块解析 CAN 命令后返回的操作意图，由任务函数执行 | 命令、指令 |
| 纯判断 | App 模块只分析数据并返回结论/请求，不执行任何硬件操作 | 无状态检查 |

## 关键决策

| 决策 | 理由 | ADR |
|------|------|-----|
| 删除 data_report 模块 | CAN ID 与 can_cmd 冲突；所有函数无调用者；功能已被 can_cmd 完全覆盖 | [ADR-0001](../../adr/0001-delete-data-report-module.md) |
| App 模块禁调 BSP，硬件操作由任务函数中转 | 分层纪律：App 模块可脱离硬件做 host 单元测试；soc_ocv 已验证此模式的可行性 | [ADR-0002](../../adr/0002-task-function-bsp-mediation.md) |
| 保护阈值从运行时配置读取 | CAN 配置命令写入后需实际生效；编译期默认值仍作为出厂默认 | [ADR-0003](../../adr/0003-runtime-config-protection-thresholds.md) |
| 保持 CubeMX USER CODE 兼容 | 后续修改 HAL 配置时不被覆盖 | — |
| soc_ocv 模块不作结构性变更 | 已是最佳实践的模块，仅修一个精度问题 | — |

## 重构架构图

```
┌──────────────────────────────────────────────────────┐
│ Core (CubeMX 骨架，USER CODE 区域修改)                 │
│  main.c / freertos.c / stm32f1xx_it.c /              │
│  FreeRTOSConfig.h                                     │
│  - 堆大小 3072 → 15360                                │
│  - 删 defaultTask，任务创建移至此层                     │
│  - 故障处理器捕获 SCB 诊断寄存器                        │
├──────────────────────────────────────────────────────┤
│ BSP (9 模块，纯硬件抽象)                                │
│  ring_buf (新增) / bsp_common (新增)                   │
│  systick / led / io_ctrl / i2c_sw / usart_drv /      │
│  bq76940 / can_drv / timer / wdg                      │
│  - bq76940: 补全 OCD/SCD 保护配置                      │
│  - bq76940: 多字节读取加 CRC8 验证                      │
│  - can_drv: 实现真正阻塞发送                            │
│  - usart_drv / can_drv: 复用共享 ring_buf              │
│  - 各模块共享 bsp_status_t 状态枚举                     │
├──────────────────────────────────────────────────────┤
│ App (5 模块，纯策略/算法/协议)                           │
│  bms_shared / protection / soc_ocv / can_cmd /        │
│  bms_app (任务编排)                                    │
│  - protection: 纯判断 + 读 settings，不调 BSP           │
│  - can_cmd: 纯协议解析返回 action 请求，不调 BSP         │
│  - bms_app: 任务函数执行 BSP 操作                       │
│  - 删除 data_report                                   │
└──────────────────────────────────────────────────────┘
```

---

## REMOVED Requirements

### REMOVED: data_report 模块及其 CAN ID 定义

`App/Inc/data_report.h` 和 `App/Src/data_report.c` SHALL 被删除，其 CAN ID 定义（0x100/0x110-0x112/0x120）已由 `can_cmd.h` 覆盖。

#### Scenario: CAN ID 无冲突
- GIVEN data_report 已删除
- WHEN 编译系统
- THEN `CAN_TX_BMS_STATUS`、`CAN_TX_CELL_VOLT1-3`、`CAN_TX_PROTECTION` 唯一定义于 can_cmd.h，无重定义

#### Scenario: 移除无效初始化调用
- GIVEN data_report 已删除
- WHEN `bms_app_init()` 执行
- THEN 不再调用 `data_report_init()`

### REMOVED: defaultTask 空任务

freertos.c 中的 `defaultTask`（仅 `osDelay(1000)` 空循环）SHALL 被移除，释放栈空间 512 字节。

#### Scenario: 无空转任务
- GIVEN defaultTask 已移除
- WHEN 系统启动后运行
- THEN 仅存在 6 个 BMS 任务 + 空闲任务 + 定时器任务，无可观测的空转任务

---

## MODIFIED Requirements

### MODIFIED: FreeRTOS 堆大小

系统 SHALL 使用 `configTOTAL_HEAP_SIZE = 15360`（原 3072），满足 6 个 BMS 任务 + 定时器任务 + 队列 + 互斥锁的内存需求。

#### Scenario: 所有任务正常创建
- GIVEN 堆大小为 15360 字节
- WHEN 系统启动并创建 6 个 BMS 任务
- THEN 所有 `osThreadNew` 返回非 NULL 句柄

### MODIFIED: 故障处理器捕获诊断信息

系统 SHALL 在 HardFault / MemManage / BusFault / UsageFault 触发时，将 SCB 故障寄存器（CFSR、HFSR、MMFAR、BFAR）保存到备份 SRAM（0x2000F000），然后进入死循环。

#### Scenario: HardFault 发生后可追溯
- GIVEN 代码触发了一次 HardFault
- WHEN 系统复位后读取备份 SRAM
- THEN 0x2000F000 处保存了 CFSR 和 HFSR 值，0x2000F010 处有签名 0xDEADBEEF

### MODIFIED: bms_app_init 不创建任务

系统 SHALL 将任务创建从 `bms_app_init()` 移至 `freertos.c` 的 `MX_FREERTOS_Init()` 末尾（`USER CODE BEGIN RTOS_THREADS` 区域）。`bms_app_init()` 仅初始化 BSP 和 App 模块。

#### Scenario: 内核初始化后创建任务
- GIVEN 系统启动
- WHEN `osKernelInitialize()` 完成后
- THEN `MX_FREERTOS_Init()` 内依次创建 6 个 BMS 任务

### MODIFIED: protection_check 读运行时配置

系统 SHALL 在 `protection_check()` 中从 `bms_settings_t` 读取阈值（而非编译期 `#define`），签名为：

```c
prot_result_t protection_check(const bq76940_data_t *data,
                                const bms_settings_t *settings);
```

其中 `prot_result_t` 包含 `level`、`active_faults` 和 `request_power_off` 标志。

#### Scenario: CAN 修改阈值后保护生效
- GIVEN 当前过压阈值为 4250mV
- WHEN 通过 CAN 配置命令将 `cell_ov_mv` 改为 4100mV，且下一周期电芯电压为 4150mV
- THEN `protection_check()` 触发 `FAULT_CELL_OV`，`request_power_off = true`

#### Scenario: 编译期默认值作为出厂默认
- GIVEN `bms_shared_init()` 将 `cell_ov_mv` 初始化为 4250mV
- WHEN 系统首次上电，未收到任何 CAN 配置命令
- THEN 保护使用默认值 4250mV

### MODIFIED: protection_check 返回操作请求而非执行 BSP 调用

系统 SHALL 移除 `protection_check()` 内部对 `io_ctrl_power_off_all()` 和 `bsp_tick_get()` 的调用。紧急断电需求通过 `request_power_off` 标志返回，由 `protection_task` 执行。

#### Scenario: 严重故障时任务层执行断电
- GIVEN `protection_check()` 返回 `request_power_off = true`
- WHEN `protection_task` 检查返回结果
- THEN 任务函数调用 `io_ctrl_power_off_all()`

### MODIFIED: 保护恢复增加去抖

系统 SHALL 要求故障恢复需连续 3 次（与进入去抖对称）采样均正常才清除故障标志。

#### Scenario: 瞬态恢复不误清除故障
- GIVEN 某电芯处于过压状态
- WHEN 电压短暂回落至阈值以下仅 1 个采样周期
- THEN 故障不清除；需连续 3 次正常采样才恢复

### MODIFIED: can_cmd_dispatch 返回 action 请求

系统 SHALL 使 `can_cmd_dispatch()` 返回 `can_action_req_t` 结构体，而非在内部执行 BSP 调用。`can_cmd_task_entry` 根据返回的 action 调用 `bq76940_set_balancing()`、`bq76940_shutdown()` 等 BSP 函数。

#### Scenario: CAN 平衡命令不直接调 BSP
- GIVEN 收到 CAN ID 0x201，cmd=0x30（BALANCE_SET），mask=0x0003
- WHEN `can_cmd_dispatch()` 处理该消息
- THEN 返回 `{action: CAN_ACTION_BALANCE_SET, balance_mask: 0x0003}`，不调用 `bq76940_set_balancing()`

#### Scenario: 任务层执行平衡操作
- GIVEN `can_cmd_dispatch()` 返回 `CAN_ACTION_BALANCE_SET`
- WHEN `can_cmd_task_entry` 收到该 action
- THEN 调用 `bq76940_set_balancing(mask)`

### MODIFIED: FET 控制命令实际执行

系统 SHALL 在 `can_cmd_task_entry` 处理 `CAN_ACTION_FET_CHG_ON` 和 `CAN_ACTION_FET_DSG_ON` 时，调用 `io_ctrl_set()` 控制对应 FET 引脚。`can_cmd_dispatch()` 内部只做安全条件检查（无故障才允许开启），返回对应的 action。

#### Scenario: 无故障时 CAN 命令开启充电 FET
- GIVEN 无过压/过流充电故障
- WHEN 收到 CAN ID 0x201，cmd=0x10（FET_CHG_ON）
- THEN `can_cmd_dispatch()` 返回 `CAN_ACTION_FET_CHG_ON`，任务层调用 `io_ctrl_set(IO_ID_FET_CHG, 1)`

#### Scenario: 有故障时 CAN 命令被拒绝
- GIVEN 存在 FAULT_CELL_OV 故障
- WHEN 收到 CAN ID 0x201，cmd=0x10（FET_CHG_ON）
- THEN `can_cmd_dispatch()` 返回 `CAN_ACTION_NONE`，FET 不动作

### MODIFIED: can_rx_queue 封装

`can_rx_queue` SHALL 改为 `static` 变量（`can_cmd.c` 内部），移除 `can_cmd.h` 中 `extern osMessageQueueId_t can_rx_queue` 声明。外部模块不可直接访问该队列。

### MODIFIED: CAN TX 任务快照拷贝后解锁

`can_tx_task_entry` SHALL 在持有 mutex 期间仅做数据快照拷贝（栈上），解锁后再调用 `can_send()`，不持锁阻塞发送。

#### Scenario: 保护任务不因 CAN TX 阻塞
- GIVEN CAN 总线繁忙，`can_send()` 需要等待
- WHEN `can_tx_task` 已解锁后才发送
- THEN `protection_task` 可立即获取 mutex，不受 CAN 发送延迟影响

### MODIFIED: CAN ID 0x112 显式定义

系统 SHALL 在 `can_cmd.h` 中增加 `#define CAN_TX_CELL_VOLT3 0x112U`，替代现有 `CAN_TX_CELL_VOLT2 + 1U` 的间接引用。

### MODIFIED: BQ76940 多字节读取使用 CRC

系统 SHALL 在 `bq76940_read_cells()` 中使用 `i2c_sw_read_crc()` 替代 `i2c_sw_read_buf()`，对 18 字节电芯电压数据做 CRC8 完整性校验。

#### Scenario: CRC 校验失败时数据无效
- GIVEN I2C 通信受干扰导致数据损坏
- WHEN `bq76940_read_cells()` 中 CRC8 校验失败
- THEN 返回 `BQ76940_CRC_ERROR`，`cells.valid` 保持 0

### MODIFIED: BQ76940 保护配置补全

系统 SHALL 在 `bq76940_set_protection()` 中写入 OCD_TRIP、SCD_TRIP 寄存器及对应延时寄存器（原仅写 OV_TRIP/UV_TRIP）。

#### Scenario: OCD/SCD 阈值配置生效
- GIVEN 调用 `bq76940_set_protection()` 且配置了 `ocd_ma=20000, scd_ma=50000`
- WHEN 芯片配置完成后
- THEN OCD_TRIP 和 SCD_TRIP 寄存器值对应配置的阈值，OCD/SCD 延时寄存器也写入相应值

### MODIFIED: can_send 阻塞等待

系统 SHALL 实现 `can_send()` 在硬件邮箱满时阻塞等待，超时返回 `CAN_DRV_TIMEOUT`，不再立即返回。

#### Scenario: 邮箱满时等待直到有空位
- GIVEN CAN 硬件 3 个发送邮箱全满
- WHEN 调用 `can_send()`
- THEN 函数阻塞等待直到有邮箱空闲或超过 50ms 超时

### MODIFIED: 统一环形缓冲区

系统 SHALL 提取 `BSP/Inc/ring_buf.h`（通用环形缓冲区结构体与操作函数），`usart_drv` 和 `can_drv` 均使用 `ring_buf_t` 替代各自独立的实现。

### MODIFIED: 头文件卫命名

系统 SHALL 将所有 BSP 和 App 头文件卫从 `__BSP_XXX_H` / `__APP_XXX_H` 改为 `BSP_XXX_H` / `APP_XXX_H`（移除前导双下划线，符合 C11 7.1.3）。

### MODIFIED: soc_ocv 校正权重精度

系统 SHALL 将 `calc_correction_weight()` 中 `weight_time` 的计算逻辑改为先计算 `rest_s = rest_ms / 1000` 再做比例缩放，消除 `(rest_ms/1000)*1000` 在 `rest_ms < 1000` 时恒为 0 的精度丢失。

#### Scenario: 亚秒级静置时间正确贡献权重
- GIVEN rest_ms = 1500（即 1.5 秒），|I| < 200mA
- WHEN 计算 `calc_correction_weight(1500, 100)`
- THEN weight_time = (1 × 1000) / 300 = 3（千分比），而非旧公式的 0

## ADDED Requirements

### ADDED: 通用环形缓冲区模块

系统 SHALL 提供 `BSP/Inc/ring_buf.h` 定义 `ring_buf_t` 结构体及 `ring_buf_init` / `ring_buf_put` / `ring_buf_get` / `ring_buf_available` 四个操作。

### ADDED: 通用 BSP 状态枚举

系统 SHALL 提供 `BSP/Inc/bsp_common.h` 定义 `bsp_status_t` 枚举（OK/ERROR/BUSY/TIMEOUT），各 BSP 模块保留自身的类型别名（`typedef bsp_status_t can_drv_status_t`）。

### ADDED: 保护操作请求结构体

系统 SHALL 定义 `prot_result_t`，包含 `level`（保护等级）、`active_faults`（活跃故障位掩码）、`request_power_off`（需断电标志）。

### ADDED: CAN action 请求类型

系统 SHALL 定义 `can_action_t` 枚举和 `can_action_req_t` 结构体，用于 can_cmd 模块向任务层传递操作意图。

---

## 不改的内容

| 模块 | 理由 |
|------|------|
| soc_ocv 算法核心 | 已是项目中分层最好的模块，仅修一个精度问题 |
| 任务优先级和周期 | 现有 6 任务设计合理（acq 100ms / prot 10ms / soc 1000ms / can_tx 100ms / wdg 500ms / can_rx 事件驱动） |
| bms_shared 双 mutex 设计 | data_mutex + settings_mutex 分离合理 |
| BSP 模块间依赖关系 | systick → i2c_sw → bq76940 链式依赖是 DAG，无环，合理 |
| TIM2 回调分发模式 | `bsp_timer_irq_handler()` 在 main.c 中分发已被验证可行 |
| CubeMX 外设初始化顺序 | HAL_Init → SystemClock → MX_GPIO/CAN/I2C/... → bms_app_init 顺序不变 |

## 数据结构

### prot_result_t（新增）

```c
typedef struct {
    prot_level_t   level;
    uint16_t       active_faults;
    bool           request_power_off;
} prot_result_t;
```

### can_action_req_t（新增）

```c
typedef enum {
    CAN_ACTION_NONE = 0,
    CAN_ACTION_CLEAR_FAULT,
    CAN_ACTION_FET_CHG_ON,   CAN_ACTION_FET_CHG_OFF,
    CAN_ACTION_FET_DSG_ON,   CAN_ACTION_FET_DSG_OFF,
    CAN_ACTION_BALANCE_SET,  CAN_ACTION_BALANCE_OFF,
    CAN_ACTION_SHUTDOWN,
} can_action_t;

typedef struct {
    can_action_t action;
    uint16_t     balance_mask;
} can_action_req_t;
```

### ring_buf_t（新增）

```c
typedef struct {
    uint8_t *buf;
    uint16_t head, tail, capacity;
} ring_buf_t;

void     ring_buf_init(ring_buf_t *rb, uint8_t *buf, uint16_t cap);
uint16_t ring_buf_available(const ring_buf_t *rb);
uint8_t  ring_buf_put(ring_buf_t *rb, uint8_t byte);
uint8_t  ring_buf_get(ring_buf_t *rb, uint8_t *byte);
```

## 修改清单

| 文件 | 操作 | 改动量 |
|------|------|--------|
| `Core/Inc/FreeRTOSConfig.h` | 修改 | 1 行 |
| `Core/Src/freertos.c` | 修改 | ~10 行 |
| `Core/Src/stm32f1xx_it.c` | 修改 | ~20 行 |
| `CMakeLists.txt` | 修改 | ~5 行 |
| `BSP/Inc/ring_buf.h` | **新增** | ~20 行 |
| `BSP/Inc/bsp_common.h` | **新增** | ~12 行 |
| `BSP/Src/bq76940.c` | 修改 | ~30 行 |
| `BSP/Src/can_drv.c` | 修改 | ~15 行 |
| `BSP/Src/usart_drv.c` | 修改 | ~10 行 |
| `BSP/Src/i2c_sw.c` | 修改 | 1 行 |
| `App/Inc/data_report.h` | **删除** | — |
| `App/Src/data_report.c` | **删除** | — |
| `App/Inc/protection.h` | 修改 | ~15 行 |
| `App/Src/protection.c` | 修改 | ~40 行 |
| `App/Inc/can_cmd.h` | 修改 | ~25 行 |
| `App/Src/can_cmd.c` | 修改 | ~50 行 |
| `App/Src/bms_app.c` | 修改 | ~40 行 |
| `App/Src/soc_ocv.c` | 修改 | ~5 行 |
| 14 个头文件卫 | 修改 | 各 2 行 |

## 资源

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| FLASH | ~26.1 KB | ~25.0 KB（删 data_report 节省 ~1KB，新增代码 ~200 行 ≈ 0.8KB） |
| RAM | ~9.1 KB | ~9.9 KB（堆扩大 12KB → 更多动态可用，静态栈不变） |
| 编译警告/错误 | 0 / 0 | 目标 0 / 0 |
| App 模块数 | 6（含死代码） | 5（活跃） |
| BSP 模块数 | 8 | 9（+ ring_buf 共用） |
| 分层违规 | 4 处 | 0 处 |

## 风险

| 风险 | 缓解 |
|------|------|
| 重构后保护任务多一级跳转（check → task → BSP），极端情况断电延迟增加 | 软件保护 10ms 周期不变；BQ76940 硬件保护 μs 级响应不受影响 |
| CAN TX 快照拷贝增加栈用量（~64 字节） | can_tx_task 栈 1024 字节，足够 |
| 删 data_report 后若有外部工具依赖其 CAN ID | CAN ID 与 can_cmd 一致，仅 0x112 语义略有调整（现在显式命名为 CELL_VOLT3） |
| BQ76940 多字节读加 CRC 增加 I2C 通信时间 | CRC 是 TI datasheet 推荐的标准做法，增加 ~1ms |
| can_send 改为阻塞可能增加 CAN TX 任务耗时 | 有 50ms 超时保护，且不再持 mutex 发送，不影响其他任务 |
