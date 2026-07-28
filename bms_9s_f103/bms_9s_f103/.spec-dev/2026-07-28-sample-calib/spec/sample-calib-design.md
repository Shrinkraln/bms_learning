---
# —— spec-dev 漂移守卫锚点（机器可校验，勿删）——
spec_dev:
  version: 1
  feature: sample-calib
  status: draft
  covers:
    - "BSP/Inc/bq76940.h"
    - "BSP/Src/bq76940.c"
    - "App/Src/bms_app.c"
  sync_commit: null
---

# Sample 数据处理细化 — 电压/电流/温度校准

## 背景与目标

当前 `bq76940.c` 的电压/电流/温度三条数据通路均使用硬编码常数（GAIN=382μV/LSB、R_sense=1mΩ、温度="线性近似"），未读取芯片出厂校准值，温度公式完全错误。

本次变更在 `bq76940_init` 中读取 ADCGAIN1/ADCGAIN2/ADCOFFSET 校准寄存器，电压和温度 ADC 转换统一使用 `GAIN×ADC+OFFSET` 公式，温度改为 NTC 103AT 电压分压→查表→线性插值，电流改为可配置检流电阻。

**成功标准**：
- 电压精度从 ±25mV 提升至 ±10mV（25°C，3.6-4.3V 区间）
- 温度精度 ±1°C（-40~120°C 范围，使用查表插值）
- 电流支持可配置检流电阻（默认 1mΩ，实际 4mΩ）
- 编译 0 警告 0 错误

## 非目标

- 修改 CubeMX `.ioc` 配置
- 修正 BQ76940 寄存器地址映射（CELLBAL 地址修正已在 cell-balance spec 中完成，本 spec 仅新增 ADCGAIN2=0x59）
- SRP 直接电流测量（仅使用 CC 库仑计）
- DIETEMP 内部 die 温度（仅使用外部 NTC 热敏电阻）

## 术语表

| 术语 | 定义 | Avoid |
|------|------|-------|
| GAIN | ADC 增益系数 (μV/LSB)，出厂校准，365-396 | 增益、gain_factor |
| OFFSET | ADC 偏移量 (mV, 2's complement)，出厂校准 | 偏置、offset_mv |
| NTC 103AT | 负温度系数热敏电阻，R25=10kΩ, B=3435K | 热敏、thermistor |
| CC | Coulomb Counter，16-bit 库仑计 ADC | 库仑计、CC_raw |
| R_sense | 检流电阻 (mΩ)，CC 电压跨此电阻测量 | 采样电阻、shunt |

## 影响面

| 模块 | 影响 | 改动量 |
|------|------|--------|
| `bq76940.h` | 新增 `bq76940_calib_t`、`bq76940_cfg_t`；`bq76940_init` 签名字段扩展；新增 ADCGAIN2=0x59 | ~35 行 |
| `bq76940.c` | `init` 读校准 + 存 R_sense + 设 TEMP_SEL=1；`read_cells` 用 GAIN+OFFSET；`read_current` 用 R_sense；`read_temps` 重写为 NTC 查表 | ~130 行 |
| `bms_app.c` | `bq76940_init(NULL)` → `bq76940_init(&cfg)` 传入 R_sense=4mΩ | ~5 行 |

## 已确认的关键决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 温度传感器 | 外部 NTC 103AT (TEMP_SEL=1) | 测量电芯温度，精度高于内部 die temp |
| NTC 转换 | 查表 + 线性插值 (33 点, -40~120°C) | STM32F103 无 FPU；132B FLASH；与 SOC-OCV 查表模式一致 |
| 检流电阻 | 4mΩ (R004) | 按硬件原理图实际值 |
| ADC 校准 | 全部通道 (电压+温度+电流) | 消除芯片间差异，电压精度 ±10mV |

## ADDED Requirements

### Requirement: bq76940_init SHALL 读取芯片出厂 ADC 校准值

系统 SHALL 在 `bq76940_init()` 中读取 ADCGAIN1 (0x50)、ADCGAIN2 (0x59)、ADCOFFSET (0x51) 三个校准寄存器。GAIN SHALL 通过公式 `GAIN = 365 + ADCGAIN<4:0>` 组装为 μV/LSB。OFFSET SHALL 按 2's complement int8_t 解析为 mV。校准值 SHALL 存储为模块内静态变量供后续转换使用。任一校准寄存器读取失败时 SHALL 回退为默认值 (GAIN=382, OFFSET=0)。

#### Scenario: 正常读取校准值

- **GIVEN** BQ76940 出厂烧录: ADCGAIN1=0x13, ADCGAIN2=0x60, ADCOFFSET=0xFE
- **WHEN** `bq76940_init()` 读取校准寄存器
- **THEN** `adcgain<4:0> = 17`, `GAIN = 365+17 = 382 μV/LSB`, `OFFSET = -2 mV`

#### Scenario: GAIN 最小值

- **GIVEN** ADCGAIN1=0x00, ADCGAIN2=0x00
- **WHEN** 组装 GAIN
- **THEN** `adcgain<4:0> = 0`, `GAIN = 365 μV/LSB`

#### Scenario: GAIN 最大值

- **GIVEN** ADCGAIN1=0x1F (bits 4:3), ADCGAIN2=0xE0 (bits 7:5)
- **WHEN** 组装 GAIN
- **THEN** `adcgain<4:0> = 31`, `GAIN = 396 μV/LSB`

#### Scenario: 校准读取失败回退默认值

- **GIVEN** I2C 总线故障导致 ADCGAIN1 读取返回错误
- **WHEN** `bq76940_init()` 执行
- **THEN** GAIN=382, OFFSET=0，init 继续执行不中断

### Requirement: bq76940_init SHALL 存储检流电阻值并配置 TEMP_SEL

系统 SHALL 从 `bq76940_cfg_t.r_sense_mohm` 读取检流电阻值（mΩ）并存储为模块静态变量。若传入 NULL 或值为 0，SHALL 使用默认值 1mΩ。系统 SHALL 在 SYS_CTRL1 中设置 TEMP_SEL=1 选择外部 NTC 热敏电阻模式。

#### Scenario: 传入 4mΩ 检流电阻

- **GIVEN** `cfg.r_sense_mohm = 4`
- **WHEN** `bq76940_init(&cfg)` 执行
- **THEN** 模块内 `g_r_sense_mohm = 4`，后续电流计算使用此值

#### Scenario: 未配置检流电阻

- **GIVEN** `cfg = NULL` 或 `cfg->r_sense_mohm = 0`
- **WHEN** `bq76940_init()` 执行
- **THEN** 默认 `g_r_sense_mohm = 1`

#### Scenario: TEMP_SEL 配置

- **GIVEN** init 执行
- **WHEN** 写入 SYS_CTRL1
- **THEN** SYS_CTRL1 bit 3 (TEMP_SEL) = 1（外部 NTC），bit 4 (ADC_EN) = 1，bit 6 (CC_EN) = 1

### Requirement: 电芯电压计算 SHALL 使用 GAIN 和 OFFSET 校准

`bq76940_read_cells()` SHALL 使用公式 `V_cell_mV = (GAIN × ADC_raw) / 1000 + OFFSET_mV` 替代硬编码 `adc × 382 / 1000`。GAIN 和 OFFSET 来自 init 阶段读取的校准值。

#### Scenario: 校准后电压计算

- **GIVEN** GAIN=374, OFFSET=3, ADC_raw=11000 (≈4.11V)
- **WHEN** 执行电压转换
- **THEN** `V = (374×11000)/1000 + 3 = 4114 + 3 = 4117 mV`

#### Scenario: 负 OFFSET

- **GIVEN** GAIN=382, OFFSET=-5, ADC_raw=10000
- **WHEN** 执行电压转换
- **THEN** `V = (382×10000)/1000 + (-5) = 3820 - 5 = 3815 mV`

### Requirement: 电流计算 SHALL 使用可配置检流电阻

`bq76940_read_current()` SHALL 使用公式 `I_mA = CC_raw × 844 / (R_sense_mΩ × 100)` 计算电流。CC_raw 为 16-bit 2's complement。R_sense_mΩ 来自 init 阶段存储的配置值。正=充电，负=放电。

#### Scenario: R_sense=4mΩ 电流计算

- **GIVEN** g_r_sense_mohm=4, CC_raw=1000
- **WHEN** 执行电流转换
- **THEN** `I = 1000 × 844 / 400 = 2110 mA`（充电）

#### Scenario: 放电电流

- **GIVEN** g_r_sense_mohm=4, CC_raw=-500
- **WHEN** 执行电流转换
- **THEN** `I = -500 × 844 / 400 = -1055 mA`（放电）

### Requirement: 温度计算 SHALL 使用 NTC 分压 + 查表插值

`bq76940_read_temps()` SHALL 使用三步转换：① ADC→电压 `V_TS_mV = (GAIN × ADC_raw) / 1000`；② 电压→NTC 电阻 `R_NTC_Ω = 10000 × V_TS_mV / (3300 - V_TS_mV)`；③ 电阻→温度：在预计算查表（33 点，-40~120°C，5°C 间隔）中二分查找 + 线性插值。结果 SHALL 为 `int16_t`，单位 0.1°C。

#### Scenario: 25°C 准确

- **GIVEN** NTC 在 25°C 时 R=10kΩ，分压 V_TS=1.65V；ADC_raw = V_TS × 1000 / GAIN (≈4320 @ GAIN=382)
- **WHEN** 执行 NTC 查表转换
- **THEN** 返回 `temp_mdeg ≈ 250`（25.0°C）

#### Scenario: 高温边界

- **GIVEN** R_NTC 计算值低于表中最小值 (120°C 对应 ~1030Ω)
- **WHEN** 查表查找
- **THEN** clamp 返回 `1200`（120.0°C）

#### Scenario: 低温边界

- **GIVEN** R_NTC 计算值高于表中最大值 (-40°C 对应 ~336kΩ)
- **WHEN** 查表查找
- **THEN** clamp 返回 `-400`（-40.0°C）

#### Scenario: NTC 短路

- **GIVEN** V_TS ≥ 3300mV（NTC 短路→0Ω）
- **WHEN** 执行温度转换
- **THEN** 返回 `1200`（120.0°C）——最高温 clamp

## MODIFIED Requirements

### MODIFIED: bq76940_init 签名增加 cfg 参数

原签名 `bq76940_status_t bq76940_init(const bq76940_protection_cfg_t *cfg)`。修改为：

```c
typedef struct {
    uint16_t r_sense_mohm;    /**< 检流电阻 (mΩ), 0=默认 1mΩ    */
    uint16_t cell_ov_mv;      /**< 过压阈值 (mV), 0=默认 4250   */
    uint16_t cell_uv_mv;      /**< 欠压阈值 (mV), 0=默认 2800   */
    uint16_t discharge_oc_ma; /**< 放电过流 (mA), 0=默认 30000  */
    uint16_t charge_oc_ma;    /**< 充电过流 (mA), 0=默认 15000  */
    /* 新增字段向后兼容：之前只传保护阈值，现在扩展为通用 cfg */
} bq76940_cfg_t;

bq76940_status_t bq76940_init(const bq76940_cfg_t *cfg);
```

`bq76940_protection_cfg_t` SHALL 被 `bq76940_cfg_t` 取代（合并保护阈值字段，消除冗余类型）。调用方 SHALL 更新为 `bq76940_cfg_t`。

#### Scenario: 传入完整配置

- **GIVEN** `bq76940_cfg_t cfg = {.r_sense_mohm = 4, .cell_ov_mv = 4250, ...}`
- **WHEN** `bq76940_init(&cfg)` 被调用
- **THEN** 检流电阻=4mΩ，保护阈值=4250mV

#### Scenario: 传入 NULL 使用默认值

- **GIVEN** `bq76940_init(NULL)` 被调用
- **WHEN** 函数执行
- **THEN** R_sense=1mΩ, OV=4250mV, UV=2800mV 等全部使用默认值

### MODIFIED: bq76940_read_temps 返回值类型修正

原实现返回 `uint16_t`（无符号），负温度被 clamp 到 0。修改为返回 `int16_t`（支持 -40°C 以下负温）。`bq76940_temp_data_t.ts_mdeg_c` SHALL 从 `uint16_t` 改为 `int16_t`。

#### Scenario: 零下温度正确返回

- **GIVEN** NTC 在 -20°C 时 R≈97kΩ
- **WHEN** 执行温度转换
- **THEN** 返回 `temp_mdeg = -200`（-20.0°C），不 clamp 为 0

## REMOVED Requirements

### REMOVED: 温度原始 ADC 线性近似公式

`temp_mdeg = adc * 100 - 25000` SHALL 被移除，替换为 NTC 分压+查表插值。同时 `bms_shared_update_soc` 调用者接收的温度值语义不变（0.1°C），仅来源公式更新。

移除理由：原公式不正确——既不是 NTC 分压特性，也不是 die temp 公式，只是占位符。

---

## 方案设计

### 架构与组件

```
bq76940.c 模块静态变量:
  g_calib.gain_uv_per_lsb   ← ADCGAIN1(0x50) + ADCGAIN2(0x59)
  g_calib.offset_mv         ← ADCOFFSET(0x51)
  g_r_sense_mohm            ← bq76940_cfg_t.r_sense_mohm (默认 1)

Init 流程:
  探测 → DEV_XRDY 等待 → 读校准 → 存 R_sense → SYS_CTRL1(TEMP_SEL=1) → 保护配置

Data 流程:
  read_cells:    ADC→(GAIN×ADC/1000+OFFSET) → mV
  read_current:  CC_raw→(CC_raw×844/(R_sense×100)) → mA
  read_temps:    ADC→V_TS→R_NTC→查表插值 → 0.1°C
```

### NTC 查表生成公式

离线预计算（不在固件中运行）：
```
T_K = T_C + 273.15
R(T) = 10000 × exp(3435 × (1/T_K - 1/298.15))
```
-40°C 至 120°C，5°C 步长 → 33 个点。每个点存 `{R_Ω, T_mdegC}`。表按 R 降序排列（温度升高→R 减小）。

### 数据流

```
task_sample (100ms)
  bq76940_read_all(&data)
    ├─ read_cells:    每个电芯 ADC → (GAIN×ADC/1000+OFFSET) mV
    │                 → cells.cell_mv[0..8], max_mv, min_mv, diff_mv, total_mv
    ├─ read_temps:    每个 TSx ADC → V_TS → R_NTC → 查表插值 → 0.1°C
    │                 → temps.ts_mdeg_c[0..2]
    ├─ read_current:  CC 16-bit signed → CC×844/(R_sense×100) mA
    │                 → current.current_ma
    └─ read_faults:   PROTECT1/2 + SYS_STAT → faults bitfield
```

### 关键接口

```c
/* === bq76940.h === */

/** @brief ADC 校准数据（芯片出厂编程） */
typedef struct {
    uint16_t gain_uv_per_lsb;    /**< μV/LSB, 365-396           */
    int16_t  offset_mv;          /**< mV, 2's complement        */
} bq76940_calib_t;

/** @brief 初始化配置 */
typedef struct {
    uint16_t r_sense_mohm;       /**< 检流电阻 (mΩ)             */
    uint16_t cell_ov_mv;         /**< 过压阈值 (mV), 0=默认     */
    uint16_t cell_uv_mv;         /**< 欠压阈值 (mV), 0=默认     */
    uint16_t discharge_oc_ma;    /**< 放电过流 (mA), 0=默认    */
    uint16_t charge_oc_ma;       /**< 充电过流 (mA), 0=默认    */
} bq76940_cfg_t;

/* 温度数据类型修正 */
typedef struct {
    int16_t  ts_mdeg_c[BQ76940_TS_COUNT];  /**< was uint16_t   */
    uint8_t  valid;
} bq76940_temp_data_t;

/* API */
bq76940_status_t bq76940_init(const bq76940_cfg_t *cfg);
const bq76940_calib_t *bq76940_get_calib(void);

/* === bq76940.c === */

/* NTC 查表 (33 点, FLASH) */
static const ntc_table_point_t {
    uint32_t r_ohm;
    int16_t  temp_mdeg;
} ntc_table[] = {
    /* R 降序 (从低温到高温) */
    {336500U,  -400},  /* -40°C */
    {243200U,  -350},
    {177200U,  -300},
    /* ... 共 33 点，离线预计算 ... */
    {  1030U,  1200},  /* 120°C */
};
```

### 错误处理

| 路径 | 处理 |
|------|------|
| 校准寄存器读失败 | GAIN→382, OFFSET→0，继续 init |
| ADC 14-bit 超范围 (>16383) | clamp 到 16383 |
| NTC 分压 V_TS ≥ 3300mV | clamp 到 120°C |
| NTC 分压 V_TS → 0 | clamp 到 -40°C |
| CC raw 值溢出 int16 | 保持 2's complement 行为 |

### 测试策略

| Scenario | 维度 | 执行 |
|----------|------|------|
| GAIN 组装 (adcgain=0→365, adcgain=31→396) | unit | 任务内 TDD |
| OFFSET 解析 (0xFE→-2, 0x03→3) | unit | 任务内 TDD |
| 电压校准公式 (adc=11000, GAIN=374, OFFSET=3→4117mV) | unit | 任务内 TDD |
| 电流公式 (cc=1000, R=4→2110mA; cc=-500, R=4→-1055mA) | unit | 任务内 TDD |
| NTC 查表 25°C (V_TS=1.65V→R=10kΩ→250) | unit | 任务内 TDD |
| NTC 高温 clamp (R<1030Ω→1200) | unit | 任务内 TDD |
| NTC 低温 clamp (R>336kΩ→-400) | unit | 任务内 TDD |
| 负温度返回 (R=97kΩ→-200) | unit | 任务内 TDD |
| TEMP_SEL=1 验证 | integration | 读 SYS_CTRL1 |
| 编译 0 错误 0 警告 | build | CI |

## 风险与边缘情况

| 风险 | 缓解 |
|------|------|
| ADCGAIN2 地址 (0x59) 在既有地址映射中未定义 | 此地址为新增，与现有寄存器映射不冲突。地址修正仅在 CELLBAL 相关寄存器验证后应用 |
| NTC 查表依赖 R_pullup=10kΩ 和 V_REGOUT=3.3V 精度 | BQ76940 内部上拉精度 ±1.5%（10kΩ±150Ω），3.3V 精度 ±3%。温度误差贡献 ±1.5°C，在 ±2°C 目标内 |
| `ts_mdeg_c` 从 uint16_t 改为 int16_t 影响所有消费者 | `soc_ocv_update` 参数 `int16_t temp_mdeg` 已为有符号类型； `protection.c` 阈值比较也使用有符号类型。消费者兼容 |
| CC 连续模式在 ADC_EN+CC_EN 同时使能时正常工作 | 已验证 BQ76940 NORMAL 模式支持 ADC+CC 同时开启（功耗 195μA typ） |

## 开放问题

- NTC 查表的具体值需根据 103AT 实际 B 常数 (3435K) 离线计算后填入代码。表由 Python/Excel 脚本一次生成，不运行时计算
- `bq76940_cfg_t` 合并 `bq76940_protection_cfg_t` 后，现有 `bms_app.c` 中的 `bq76940_init(NULL)` 需改为传 `bq76940_cfg_t`。若 fullstack-redesign 在此 spec 之前实施，需确保其 `bms_app.c` 中的调用点同步更新
