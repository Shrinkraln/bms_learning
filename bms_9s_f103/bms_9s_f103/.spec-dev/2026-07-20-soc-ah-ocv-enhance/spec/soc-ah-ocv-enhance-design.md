---
spec_dev:
  feature: soc-ah-ocv-enhance
  covers:
    - App/Src/soc_ocv.c
    - App/Inc/soc_ocv.h
    - App/Src/bms_app.c
  status: active
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
| 静置 | 电池电流接近零的状态 | rest、空闲 |
| 电流符号 | `current_ma > 0` = 充电, `current_ma < 0` = 放电 | — |

## 关键决策

| 决策 | 理由 |
|------|------|
| 不建电池模型 (ECM/EKF) | STM32F103 资源有限，用户选择轻量方案 |
| 在现有 soc_ocv.c 上增强而非重写 | 改动半径最小 |
| 温度仅补偿容量，不做 OCV-SOC 温补 | 不需要多温度点 OCV 表，做多温补需要大量标定数据 |
| OCV 表扩至 21 点 (5% 间隔) | 插值误差显著降低 |

## 增强项

### 1. 温度补偿容量

### Requirement: SOC 计算应使用温度补偿后的有效容量
系统 SHALL 在每次库仑积分时，根据传入的温度参数查表获取容量系数，用 `C_nominal × 系数 / 100` 作为有效容量。

#### Scenario: 低温下容量缩减
- GIVEN 标称容量 20000mAh，当前温度 -10°C
- WHEN 执行库仑积分
- THEN 使用有效容量 20000 × 78 / 100 = 15600mAh

#### Scenario: 常温下容量不缩减
- GIVEN 标称容量 20000mAh，当前温度 25°C
- WHEN 执行库仑积分
- THEN 使用有效容量 20000mAh (系数 100%)

温度-容量系数表（`static const`，FLASH 存储）：

```
Temp(°C):  -20  -10    0   10   20   25   30   40   50   60
系数(%):    65   78   88   94   98  100  100  100  100  100
```

中间温度线性插值。>25°C 时容量系数钳位在 100%——高温下有效容量不随温度继续增长。

### 2. 动态 OCV 校正权重

### Requirement: OCV 校正权重应随电流大小和静置时长连续变化
系统 SHALL 根据 |I| 和静置持续时间动态计算校正权重，权重 = `clamp(rest_s / 300, 0, 1) × clamp((500 − |I|_mA) / 300, 0, 1)`。
电流因子: |I| < 200mA → 1.0; |I| 200-500mA → 线性衰减; |I| > 500mA → 0。

#### 静置计时器状态机

```
entry:  |I| < 200mA 且持续 3 个连续采样周期 → rest_timer 开始累加
exit:   |I| ≥ 500mA 持续 1 个采样周期 → rest_timer 归零
hysteresis: 200mA ≤ |I| < 500mA → rest_timer 暂停但不归零
```

`soc_task` 以 1Hz 固定周期调用 `soc_ocv_update`，因此 3 个采样周期 = 3 秒。

#### Scenario: 完全静置 5 分钟后达到满权重
- GIVEN rest_seconds = 300, |I| < 200mA
- WHEN 计算 OCV 校正权重
- THEN weight = 1.0，直接信任 OCV 查表值做全量校正

#### Scenario: 中等负载下不做校正
- GIVEN |I| = 600mA
- WHEN 计算 OCV 校正权重
- THEN weight = 0，不执行校正

#### Scenario: 短暂静置后做小幅度校正
- GIVEN rest_seconds = 30, |I| < 200mA
- WHEN 计算 OCV 校正权重
- THEN weight = 0.1，以 1/10 权重修正 SOC

### 3. 电流零漂自估计

### Requirement: 长期静置时应自动估计并扣除电流传感器零漂
系统 SHALL 在 |I| < 200mA 且 rest_seconds > 300 (5min) 时，用指数滑动平均 (EMA, α=0.01) 估计电流偏置，并在库仑积分前从测量值中扣除。

#### Scenario: 检测到 10mA 零漂并自动扣除
- GIVEN 实际电流为 0，传感器读数 = 10mA，持续静置 10 分钟
- WHEN 零漂估计收敛
- THEN offset_est ≈ 10mA，库仑积分使用 I_corrected = I_raw − offset_est

#### 零漂状态管理

- rest 退出 (|I| ≥ 500mA): EMA **冻结**（保留当前估计值）
- rest 恢复: EMA 从冻结值**继续**更新
- 冻结超时: 若连续 1 小时未回到 rest 状态，冻结的 offset_est **清零**（传感器温漂可能导致旧值失效）

### 4. 上电初始化策略

### Requirement: 上电时应用 OCV 查表初始化 SOC，不做复杂检测
系统 SHALL 在 `soc_ocv_init` 后首次 `update` 时，直接用当前 `cell_min_mv` 经 OCV 查表初始化 SOC。该初始值作为 EKF 的起点，后续动态 OCV 校正会在静置时自动修正偏差。

#### Scenario: 静置上电
- GIVEN 上电后电池处于静置状态 (|I| < 200mA)
- WHEN 首次 `update`
- THEN 用 cell_min_mv → OCV 查表 → 直接赋值 SOC。静置条件下 OCV 准确，初始 SOC 可靠。

#### Scenario: 带载上电
- GIVEN 上电后电池在负载下
- WHEN 首次 `update`
- THEN 仍用 OCV 查表初始化（IR drop 导致初始值偏低）。首次静置检测触发后，动态校正权重会将 SOC 拉回正确值。

> 去掉了原设计中的 `init_confidence` 标志和"首次校正权重翻倍"——动态权重公式本身就能处理收敛问题。带载上电时 rest_timer 为 0，权重为 0，不会错误校正。一旦进入静置，权重自然增长，自动修正。

### 5. OCV-SOC 表扩展

### Requirement: OCV-SOC 查表应提供 5% SOC 间隔的 21 点数据
系统 SHALL 将 OCV-SOC 查找表从当前 11 点 (10% 间隔) 扩展为 21 点 (5% 间隔)，使用线性插值。

#### Scenario: 查表精度（线性区 15%-90% SOC）
- GIVEN OCV = 3.72V (在线性区)
- WHEN 执行 soc_ocv_lookup(3720)
- THEN 返回 SOC ≈ 450‰ (45%)，线性插值误差 < ±0.5% SOC

OCV-SOC 表 (25°C, NMC 典型值, `static const` 存于 FLASH)：

```
SOC%:   0   5  10  15  20  25  30  35  40  45  50  55  60  65  70  75  80  85  90  95 100
OCV: 3.00 3.30 3.42 3.50 3.55 3.60 3.64 3.67 3.70 3.72 3.75 3.78 3.81 3.85 3.90 3.96 4.02 4.08 4.15 4.18 4.20 (V)
```

**已知局限**：OCV-SOC 曲线在 SOC < 10% 和 SOC > 90% 时非线性剧烈，5% 间隔的线性插值误差会增大到 ±1-2% SOC。这两个区域不是常规工作区间，对整体精度影响有限。

## MODIFIED Requirements

### MODIFIED: soc_ocv_update 函数签名
原签名 `(uint16_t cell_min_mv, int32_t current_ma, uint32_t dt_ms)`。修改为增加温度参数：

```c
void soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma,
                     int16_t temp_mdeg, uint32_t dt_ms);
```

`temp_mdeg`: 电池温度，单位 0.1°C。取 BQ76940 三个温度传感器中的最高值（最热电芯温度）。

### MODIFIED: soc_ocv_init 函数
增加零漂估计器和静置计时器的初始化。其他不变。

## 数据结构

```c
// soc_ocv_ctx_t — 修改后
typedef struct {
    uint16_t  soc_permil;         // SOC 0-1000 (保留)
    uint16_t  ocv_mv;             // OCV 参考值 mV (保留)
    int32_t   remaining_mah;      // 剩余容量 mAh (保留)
    int32_t   nominal_mah;        // 标称容量 mAh (保留)
    uint32_t  last_sample_ms;     // 上次时间戳 (保留)
    uint32_t  rest_timer_ms;      // 静置累计时间 ms (原 rest_start_ms, 语义变更)
    int32_t   current_offset_ma;  // [新增] 零漂估计值 mA
    uint32_t  rest_exit_ms;       // [新增] 退出静置的时间戳，用于 1h 超时
    uint8_t   initialized;        // 初始化标记 (保留)
    // 总大小: 原有 24B → 新增 8B → 共 32B
} soc_ocv_ctx_t;
```

## 公共 API

```c
void     soc_ocv_init(int32_t nominal_mah);
void     soc_ocv_update(uint16_t cell_min_mv, int32_t current_ma,
                         int16_t temp_mdeg, uint32_t dt_ms);  // 新增 temp_mdeg
uint16_t soc_ocv_get_soc(void);
uint16_t soc_ocv_get_ocv(void);
int32_t  soc_ocv_get_remaining_mah(void);
uint16_t soc_ocv_lookup(uint16_t ocv_mv);
const soc_ocv_ctx_t *soc_ocv_get_ctx(void);
```

## 修改清单

| 文件 | 操作 | 改动量 |
|------|------|--------|
| `App/Inc/soc_ocv.h` | 修改 — 扩展 ctx，更新 API 签名 | ~10 行 |
| `App/Src/soc_ocv.c` | 修改 — 实现 5 项增强 | ~150 行 |
| `App/Src/bms_app.c` | 修改 — `soc_task` 传温度参数 | 1 行 |

`bms_shared.c`、`can_cmd.c`、`protection.c` 不修改。

## 资源

| 指标 | 当前 | 升级后 |
|------|------|--------|
| FLASH (soc_ocv) | ~1.4 KB | ~2.4 KB |
| RAM (soc_ocv_ctx) | 24 B | ~32 B |
| 执行时间 | <0.5ms @72MHz | <1ms @72MHz |
| 精度 | ±5-10% | ±3-5% |

## 风险

| 风险 | 缓解 |
|------|------|
| 温度系数表基于 NMC 典型值，可能不匹配具体电芯 | 系数表 `static const`，易于替换 |
| 零漂估计需要 >5min 静置 | 初始阶段不扣除零漂，仅库仑积分运行 |
| 带载上电初始 SOC 偏低 (IR drop) | 静置检测触发后动态校正自动修正 |
| OCV 表 25°C 固定，低/高温下 OCV-SOC 曲线漂移 | 误差贡献 <2% SOC @ 0°C，仍在 ±3-5% 目标内 |
| OCV 未建模充/放电迟滞 (10-30mV) | 迟滞贡献 ±1-3% SOC，在 ±3-5% 目标内；未来可加方向感知平均 |
| OCV 表在 SOC <10% 和 >90% 区间插值误差增大 | 这两段不是常规工作区间；保护系统在 <10% 时已触发欠压告警 |
| 零漂估计冻结 1h 超时后清零 | 传感器零漂温漂通常 <1mA/°C；1h 不清零的风险 > 清零后短期无补偿的风险 |
