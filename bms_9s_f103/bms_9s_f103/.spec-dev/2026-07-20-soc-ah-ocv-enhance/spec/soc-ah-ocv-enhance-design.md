---
spec_dev:
  feature: soc-ah-ocv-enhance
  covers:
    - App/Src/soc_ocv.c
    - App/Inc/soc_ocv.h
  status: draft
---

# 增强型安时积分 + OCV 查表 SOC 估算

## 目标

改进现有 `soc_ocv.c` 的 SOC 估算精度，从当前 ±5-10% 提升到 ±3-5%，不引入电池建模或矩阵运算。

## 术语

| 术语 | 定义 | Avoid |
|------|------|-------|
| SOC | State of Charge，电池剩余电量百分比 (0-100%) | 电量、荷电状态 |
| OCV | Open Circuit Voltage，开路电压 (mV) | 开路电压 |
| 安时积分 | Coulomb Counting，对电流时间积分得到容量变化 | 库仑计数、库仑积分 |
| 零漂 | 电流传感器在零电流输入时的输出偏置 (mA) | offset、偏置 |
| 静置 | 电池电流接近零的状态，|I| < 阈值 | rest、空闲 |

## 关键决策

| 决策 | 理由 |
|------|------|
| 不建电池模型 (ECM/EKF) | STM32F103 资源有限，用户选择轻量方案 |
| 在现有 soc_ocv.c 上增强而非重写 | API 不变，下游零修改 |
| 温度仅补偿容量，不做 R0/R1 温补 | 不需要电池模型参数 |
| OCV 表扩至 21 点 (5% 间隔) | 线性插值误差从 ±1.5% 降到 ±0.4% |

## 增强项

### 1. 温度补偿容量

### Requirement: SOC 计算应使用温度补偿后的有效容量
系统 SHALL 在每次库仑积分时，根据当前温度查表获取容量系数，用 `C_nominal × 系数 / 100` 作为有效容量。

#### Scenario: 低温下容量缩减
- GIVEN 标称容量 20000mAh，当前温度 -10°C
- WHEN 执行库仑积分
- THEN 使用有效容量 20000 × 78 / 100 = 15600mAh

#### Scenario: 常温下不缩减
- GIVEN 标称容量 20000mAh，当前温度 25°C
- WHEN 执行库仑积分
- THEN 使用有效容量 20000mAh (系数 100%)

温度-容量系数表：

```
Temp(°C):  -20  -10    0   10   20   25   30   40   50   60
系数(%):    65   78   88   94   98  100  102  105  107  108
```

中间温度线性插值。

### 2. 动态 OCV 校正权重

### Requirement: OCV 校正权重应随电流大小和静置时长连续变化
系统 SHALL 根据 |I| 和静置持续时间动态计算校正权重，而非使用二值判断和固定权重。

#### Scenario: 完全静置 5 分钟后达到满权重
- GIVEN |I| < 200mA 且 rest_duration = 300s
- WHEN 计算 OCV 校正权重
- THEN weight = 1.0，直接信任 OCV 查表值

#### Scenario: 中等负载下不做校正
- GIVEN |I| = 600mA
- WHEN 计算 OCV 校正权重
- THEN weight = 0，不执行校正

#### Scenario: 短暂静置后做小幅度校正
- GIVEN |I| < 200mA 且 rest_duration = 30s
- WHEN 计算 OCV 校正权重
- THEN weight = 0.1，以 1/10 权重修正 SOC

权重公式：`weight = clamp(rest_s / 300) × clamp(1 - |I|/500)`，范围 [0, 1]。

### 3. 电流零漂自估计

### Requirement: 长期静置时应自动估计并扣除电流传感器零漂
系统 SHALL 在 |I| < 200mA 且 rest_duration > 5min 时，用指数滑动平均 (EMA, α=0.01) 估计电流偏置，并在库仑积分前从测量值中扣除。

#### Scenario: 检测到 10mA 零漂
- GIVEN 实际电流为 0，传感器读数 = 10mA，持续静置 10 分钟
- WHEN 零漂估计收敛
- THEN offset_est ≈ 10mA，库仑积分使用 I_corrected = I_raw - 10mA

#### Scenario: 零漂未收敛时不扣除
- GIVEN 零漂估计尚未收敛 (rest_duration < 5min)
- WHEN 执行库仑积分
- THEN 使用原始电流值 I_raw

### 4. 首次上电智能检测

### Requirement: 上电时应自动判别静置/带载启动，选择最佳初始化策略
系统 SHALL 在 `soc_ocv_init` 后首次 `update` 时，检测连续 3 次采样的 |I| 是否均 < 200mA，判断为静置启动或带载启动。

#### Scenario: 静置上电
- GIVEN 上电后连续 3 次采样 |I| < 200mA
- WHEN 执行 SOC 初始化
- THEN 直接用 OCV 查表赋值 SOC，标记高置信度

#### Scenario: 带载上电
- GIVEN 上电后 3 次采样中任一次 |I| ≥ 200mA
- WHEN 执行 SOC 初始化
- THEN 仍用 OCV 初始化，但标记低置信度，首次 OCV 校正权重翻倍以加速收敛

### 5. OCV-SOC 表加密

### Requirement: OCV-SOC 查表应提供 5% SOC 间隔的 21 点数据
系统 SHALL 将 OCV-SOC 查找表从当前 11 点 (10% 间隔) 扩展为 21 点 (5% 间隔)，使用线性插值。

#### Scenario: 查表精度
- GIVEN OCV = 3.72V
- WHEN 执行 soc_ocv_lookup(3720)
- THEN 返回 SOC ≈ 450‰ (45%)，插值误差 < ±0.4% SOC

OCV-SOC 表 (25°C, NMC 典型值)：

```
SOC%:   0   5  10  15  20  25  30  35  40  45  50  55  60  65  70  75  80  85  90  95 100
OCV: 3.00 3.30 3.42 3.50 3.55 3.60 3.64 3.67 3.70 3.72 3.75 3.78 3.81 3.85 3.90 3.96 4.02 4.08 4.15 4.18 4.20 (V)
```

## MODIFIED Requirements

以下是对现有行为的修改：

### MODIFIED: soc_ocv_update 函数
原函数使用固定 nominal_mah、二值 OCV 校正、无零漂补偿。修改为包含温度补偿容量、动态权重、零漂扣除的版本。函数签名不变。

### MODIFIED: soc_ocv_init 函数
原函数仅设置 nominal_mah 和初始化状态。修改为增加上电检测标记和零漂估计器初始化。函数签名不变。

## 数据结构

```c
// soc_ocv_ctx_t 新增字段
typedef struct {
    // ... 原有字段不变 ...
    int32_t  current_offset_ma;   // 零漂估计值 (mA)
    uint32_t rest_start_ms;       // 静置开始时间 (原已有，保留)
    uint8_t  init_confidence;     // 0=低置信(带载启动), 1=高置信(静置启动)
    uint8_t  init_samples;        // 初始化采样计数
} soc_ocv_ctx_t;
```

## 公共 API (不变)

```c
void     soc_ocv_init(int32_t nominal_mah);
void     soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma, uint32_t dt_ms);
uint16_t soc_ocv_get_soc(void);
uint16_t soc_ocv_get_ocv(void);
int32_t  soc_ocv_get_remaining_mah(void);
uint16_t soc_ocv_lookup(uint16_t ocv_mv);
const soc_ocv_ctx_t *soc_ocv_get_ctx(void);
```

## 修改清单

| 文件 | 操作 |
|------|------|
| `App/Inc/soc_ocv.h` | 修改 — 扩展 ctx 结构体，API 不变 |
| `App/Src/soc_ocv.c` | 修改 — 实现 5 项增强 |

无其他文件变更。`bms_app.c`、`bms_shared.c`、`can_cmd.c` 均不修改。

## 资源

| 指标 | 当前 | 升级后 |
|------|------|--------|
| FLASH | ~1.4 KB (.text) | ~2.4 KB |
| RAM | 24 B (.bss) | ~200 B |
| 执行时间 | <0.5ms @72MHz | <1ms @72MHz |
| 精度 | ±5-10% | ±3-5% |

## 风险

| 风险 | 缓解 |
|------|------|
| 温度系数表基于 NMC 典型值，可能不匹配具体电芯 | 系数表定义为 `static const` 数组，易于替换 |
| 零漂估计需要 >5min 静置 | 初始阶段不扣除零漂，仅用库仑积分运行 |
| 带载启动时初始 SOC 可能偏差较大 | 标记低置信度，首次校正权重翻倍加速收敛 |
