---
spec_dev:
  version: 1
  feature: afe-disconnect-can-report
  status: draft
  covers:
    - "bms_9s_f103/App/Src/bms_app.c"
    - "bms_9s_f103/App/Src/can_cmd.c"
    - "bms_9s_f103/App/Inc/can_cmd.h"
    - "BmsHostApp/protocol/BmsSnapshot.h"
    - "BmsHostApp/protocol/BmsProtocolDecoder.cpp"
    - "BmsHostApp/can/CanWorker.h"
    - "BmsHostApp/can/CanWorker.cpp"
    - "BmsHostApp/model/BmsDataModel.cpp"
  sync_commit: null
---

# AFE 断连检测与 CAN 上报

## 背景与目标

当前 BMS 固件在 BQ76940 电池监视器 (AFE) I2C 通信失败时，仍持续通过 CAN 发送周期帧（携带过期/零值数据），上位机无法区分"CAN 总线正常但电池监视器离线"与"一切正常"。同时上位机存在 CAN 重连 bug：500ms 超时断连后即使帧恢复也不再显示"已连接"。

**目标**：在 0x120 STATUS 帧中增加 AFE 在线标志位，固件据此上报 AFE 连接状态；上位机解析后显示"电池未接入"；修复重连 bug。

**成功标准**（典型值，受 100ms 采样周期 + CAN 上报周期 + 30ms 渲染影响）：
1. 拔掉 BQ76940 I2C 线后，上位机 ≤500ms 内显示"⚠ 电池未接入"
2. 重新接上 I2C 线后，上位机 ≤250ms 内恢复"● 已连接"
3. 拔掉 CAN 线后上位机显示"⚠ 已断开"，重新接上后自动恢复"● 已连接"

## 非目标

- 不修改 BQ76940 驱动层 (`bq76940.c`/`bq76940.h`)
- 不新增 CAN 帧 ID（复用现有 0x120）
- 不修改 QML UI 布局
- 不引入 AFE 离线状态持久化（掉电不保存）

## 术语表

- **AFE (Analog Front End)**：BQ76940 电池监视器芯片，通过 I2C 与 MCU 通信，负责采集电芯电压、温度、电流、保护状态
- **上位机 (Host)**：BmsHostApp Qt6 桌面应用，通过 USB-CAN 适配器与 BMS 板通信
- **ctrl 字节**：0x120 STATUS 帧 data[5]，位编码字段，承载 prot_level、FET 状态、均衡状态，本次新增 AFE 在线标志

## 影响面

| 模块 | 文件 | 改动性质 |
|------|------|---------|
| 固件-CAN协议 | `App/Inc/can_cmd.h` | 新增宏定义 |
| 固件-主应用 | `App/Src/bms_app.c` | 新增全局变量 + 采样逻辑 |
| 固件-周期上报 | `App/Src/can_cmd.c` | can_pub() 读取并填充 AFE 位 |
| 上位机-快照 | `protocol/BmsSnapshot.h` | 新增 afe_online 字段 |
| 上位机-协议解码 | `protocol/BmsProtocolDecoder.cpp` | 解析 ctrl bit5 |
| 上位机-CAN驱动 | `can/CanWorker.h`, `can/CanWorker.cpp` | 修复重连 bug |
| 上位机-数据模型 | `model/BmsDataModel.cpp` | applySnapshot 消费 afe_online |

## 已确认的关键决策

- **使用缓存全局变量 `g_afe_online` 而非调用 `bq76940_is_online()`**：后者每次执行 I2C 探针，在 100ms 周期路径中产生不必要总线流量；缓存标志由采样任务更新，零额外开销
- **AFE 离线映射到 DISCONNECTED 状态**：用户明确要求"连接显示断开"，通过 statusText 区分原因（"⚠ 电池未接入" vs "⚠ 已断开"）
- **AFE 恢复即时生效（不复用 3 次去抖）**：去抖仅用于故障保护（避免误触发 FET 关断），连接状态显示应即时响应用户操作

---

## ADDED Requirements

### Requirement: AFE 在线状态上报

固件 SHALL 在每 100ms 周期上报的 0x120 STATUS 帧 ctrl 字节 bit5 中指示 BQ76940 AFE 是否在线（1=在线，0=离线）。

#### Scenario: AFE 正常响应时 bit5=1

- **GIVEN** BQ76940 已初始化且 I2C 通信正常
- **WHEN** `task_sample` 通过 `bq76940_read_all()` 成功读取数据
- **THEN** 下一帧 0x120 data[5] bit5 = 1

#### Scenario: AFE 通信丢失时 bit5=0

- **GIVEN** BQ76940 I2C 连续 3 次读取失败（每次间隔约 100ms）
- **WHEN** `comm_err_cnt` 达到 3
- **THEN** 下一帧 0x120 data[5] bit5 = 0

#### Scenario: AFE 从离线恢复时 bit5 立即恢复

- **GIVEN** AFE 此前离线（`g_afe_online = 0`，bit5=0 已在上报帧中）
- **WHEN** 下一次 `bq76940_read_all()` 返回 OK
- **THEN** `g_afe_online` 立即置 1，下一帧 0x120 data[5] bit5 = 1（不复用 3 次去抖）

### Requirement: 上位机解析 AFE 在线标志

上位机 SHALL 从 0x120 STATUS 帧 ctrl 字节 bit5 解析 AFE 在线状态，并写入 `BmsSnapshot::afe_online`。

#### Scenario: 解析 bit5=1

- **GIVEN** 收到 0x120 帧，data[5] bit5 = 1
- **WHEN** `BmsProtocolDecoder::decodeStatusFrame()` 处理该帧
- **THEN** 产出 `BmsSnapshot::afe_online = true`

#### Scenario: 解析 bit5=0

- **GIVEN** 收到 0x120 帧，data[5] bit5 = 0
- **WHEN** `BmsProtocolDecoder::decodeStatusFrame()` 处理该帧
- **THEN** 产出 `BmsSnapshot::afe_online = false`

---

## MODIFIED Requirements

### Requirement: 上位机连接状态显示

上位机 SHALL 根据 CAN 帧接收状态和 AFE 在线状态综合判断连接状态，并通过 `statusText` 属性显示对应文案。（原行为：仅根据 CAN 超时判断，无 AFE 感知）

#### Scenario: CAN 连接正常且 AFE 在线

- **GIVEN** CAN 帧持续到达，`BmsSnapshot::afe_online = true`
- **WHEN** `BmsDataModel::applySnapshot()` 执行
- **THEN** `connectionStatus = CONNECTED`，`statusText = "● 已连接"`

#### Scenario: CAN 连接正常但 AFE 离线

- **GIVEN** CAN 帧持续到达，`BmsSnapshot::afe_online = false`
- **WHEN** `BmsDataModel::applySnapshot()` 执行
- **THEN** `connectionStatus = DISCONNECTED`，`statusText = "⚠ 电池未接入"`

#### Scenario: AFE 从离线恢复

- **GIVEN** 当前状态为 DISCONNECTED（因 AFE 离线），未发送过关机命令
- **WHEN** 收到 `afe_online = true` 的快照
- **THEN** `connectionStatus = CONNECTED`，`statusText = "● 已连接"`

#### Scenario: CAN 超时断连

- **GIVEN** CAN 帧超过 500ms 未到达
- **WHEN** `CanWorker::checkTimeout()` 触发
- **THEN** `connectionStatus = DISCONNECTED`，`statusText = "⚠ 已断开"`

### Requirement: CAN 重连自动恢复

上位机 SHALL 在 CAN 帧超时断连后，收到新帧时自动恢复已连接状态。（原行为：超时后即使帧恢复也不再更新连接状态——`onFramesReceived` 不 emit `connectionStatusChanged(true)`）

#### Scenario: 超时后帧恢复

- **GIVEN** 此前 CAN 超时，`connectionStatus = DISCONNECTED`
- **WHEN** `CanWorker::onFramesReceived()` 收到有效 CAN 帧批次
- **THEN** emit `connectionStatusChanged(true)`，上层恢复 CONNECTED（前提：AFE 在线）

---

## 方案设计

### 协议变更：0x120 ctrl 字节 bit5

```
0x120 STATUS 帧 data[5] (ctrl):
  bits[1:0] = prot_level    (现有)
  bit[2]    = CHG_FET 开启   (现有)
  bit[3]    = DSG_FET 开启   (现有)
  bit[4]    = 均衡激活       (现有)
  bit[5]    = AFE 在线       (新增)  1=AFE正常响应, 0=AFE离线
  bits[7:6] = 保留
```

### 数据流

```
task_sample (100ms)
  ├─ bq76940_read_all() OK  → g_afe_online = 1
  └─ comm_err_cnt >= 3      → g_afe_online = 0
         │
         ▼
task_can_tx (100ms) → can_pub()
  └─ ctrl |= (g_afe_online ? CAN_STATUS_AFE_ONLINE : 0)
         │
         ▼  CAN 0x120 frame
         │
BmsHostApp: CanWorker → BmsProtocolDecoder
  └─ snap.afe_online = (ctrl & 0x20) != 0
         │
         ▼
BmsDataModel::applySnapshot()
  └─ afe_online ? "● 已连接" : "⚠ 电池未接入"
```

### 关键接口

**固件端**：
- `g_afe_online` (`uint8_t`)：全局标志，`bms_app.c` 定义，`can_cmd.c` 通过 extern 读取
- `CAN_STATUS_AFE_ONLINE` (`1U << 5U`)：宏定义，`can_cmd.h`

**上位机端**：
- `BmsSnapshot::afe_online` (`bool`)：新增字段，默认 `true`（用于从未收到 0x120 帧的路径，如仅收到故障帧时保持乐观）
- `CanWorker::m_timedOut` (`bool`)：新增成员，追踪超时状态

### 错误处理

- **AFE 离线 + CAN 超时同时发生**：CAN 超时优先（statusText="⚠ 已断开"）。帧恢复后，`CanWorker` 先 emit `connectionStatusChanged(true)`（此时 model 可能短暂显示"● 已连接"），随后下一批快照到达时 `applySnapshot` 依据 `afe_online=false` 立即纠正为"⚠ 电池未接入"——两个信号之间最多间隔 30ms（model timer）+ 100ms（CAN 周期），中间态用户不可见。
- **旧版固件（无 bit5）**：旧固件 ctrl 字节 bit5 恒为 0，若上位机单独升级会误显示"⚠ 电池未接入"。固件与上位机须同步发布，不保证跨版本兼容。
- **I2C 探针调用失败**：不影响——使用缓存 `g_afe_online`，不直接调用 `bq76940_is_online()`

---

## 测试与验收策略

| Scenario / 检查项 | 维度 | 执行方式 | 验收证据 |
|-------------------|------|---------|---------|
| AFE 正常响应时 bit5=1 | unit | 任务内 TDD | 固件编译通过，can_pub 输出 bit5=1 |
| AFE 通信丢失时 bit5=0 | unit | 任务内 TDD | 固件编译通过，can_pub 输出 bit5=0 |
| AFE 从离线恢复时 bit5 立即恢复 | unit | 任务内 TDD | 固件编译通过 |
| 解析 bit5=1 → afe_online=true | unit | 任务内 TDD | 上位机编译通过 |
| 解析 bit5=0 → afe_online=false | unit | 任务内 TDD | 上位机编译通过 |
| CAN 连接正常且 AFE 在线 → "● 已连接" | integration | 验收任务 | 实机验证截图 |
| CAN 连接正常但 AFE 离线 → "⚠ 电池未接入" | integration | 验收任务 | 实机验证截图 |
| CAN 超时断连 → "⚠ 已断开" | integration | 验收任务 | 实机验证截图 |
| 超时后帧恢复 → "● 已连接" | integration | 验收任务 | 实机验证截图 |

## 风险与边缘情况

- **固件未初始化时 g_afe_online=0**：初始值为 0，BQ76940 初始化成功前上位机将显示"⚠ 电池未接入"——符合预期，初始化通常在 500ms 内完成
- **AFE 快速插拔抖动**：AFE 恢复无去抖，可能在边界条件下快速切换状态文本——UI 层面每 30ms 更新一次，人眼可接受
- **控制按钮联动禁用**：AFE 离线时 `connectionStatus = DISCONNECTED`，`ControlPanel.qml` 和 `ConfigDialog.qml` 中所有按钮 `enabled: bms.connectionStatus === 1` 将自动禁用（FET 控制、均衡、配置、查询）。此为预期行为——电池监视器离线时不应下发控制指令。关机按钮不受此限制（始终可用）。
- **BmsHostApp 未连接 CAN 设备时**：不进入 applySnapshot 路径，状态保持初始值"⚠ 已断开"，不受影响

## 开放问题

- 无
