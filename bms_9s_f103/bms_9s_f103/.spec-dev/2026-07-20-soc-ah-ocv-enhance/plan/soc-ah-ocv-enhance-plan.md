# 实施计划: 增强型安时积分 + OCV 查表

## 文件清单

| # | 文件 | 操作 | 内容 |
|---|------|------|------|
| 1 | `App/Inc/soc_ocv.h` | 修改 | 更新 ctx 结构体 (新增 2 字段)、API 签名新增 `temp_mdeg` |
| 2 | `App/Src/soc_ocv.c` | 修改 | 5 项增强实现 |
| 3 | `App/Src/bms_app.c` | 修改 | `soc_task` 传温度参数 (1 行) |

## 实施步骤

### Step 1: 修改 `soc_ocv.h` — 数据结构 + API

- `soc_ocv_ctx_t` 新增: `current_offset_ma` (int32_t), `rest_exit_ms` (uint32_t)
- `rest_start_ms` → 重命名为 `rest_timer_ms` (语义更准确)
- `soc_ocv_update` 签名: 新增 `int16_t temp_mdeg` 参数

**验证**: 头文件语法检查 (编译 App 库)

### Step 2: 修改 `soc_ocv.c` — 5 项增强

按顺序实现:

**2a. OCV-SOC 表扩展** (21 点, `static const`)
- 替换现有 11 点表
- `soc_ocv_lookup()` 边界 clamp 逻辑: 低于 3.00V → 0‰, 高于 4.20V → 1000‰

**2b. 温度补偿容量**
- 新增 `static const` 温度-容量系数表 (10 点)
- `soc_ocv_update` 中根据 `temp_mdeg` 查表插值
- 有效容量 = `nominal_mah × 系数(temp) / 100`

**2c. 动态 OCV 校正权重**
- 实现静置计时器状态机 (entry/exit/hysteresis)
- 权重公式: `clamp(rest_s/300,0,1) × clamp((500−|I|)/300,0,1)`
- 替换现有二值判断逻辑

**2d. 电流零漂自估计**
- EMA filter: `offset = offset + (I_raw − offset) / 100` (α=0.01)
- 冻结逻辑: |I|≥500mA → 停止更新, 记录 `rest_exit_ms`
- 超时清零: 连续 1h 未恢复静置 → `offset_est = 0`

**2e. 上电初始化简化**
- 首次 `update` 直接用 OCV 查表设 SOC (无需 3 次采样检测)
- 移除 `init_confidence` / `init_samples` 字段

**验证**: 编译 `App` 库, 0 警告 0 错误

### Step 3: 修改 `bms_app.c` — 传温度参数

- `soc_task` 中: 从 `bq_data` 取 `temps.ts_mdeg_c[0]` (最高温度)
- 传给 `soc_ocv_update(cell_min, current, temp, dt_ms)`

**验证**: 全量编译 + 链接, 0 警告 0 错误

### Step 4: CAN 温度上报验证 (不需要改代码)

- `can_tx_cells()` 已在 0x112 帧上报 3 路温度 (每路 1 字节, °C 单位)
- 确认上位机能解析

## 编译验证

```bash
cmake --preset Debug && cmake --build build/Debug
```

预期: FLASH +1KB, RAM +8B, 0 警告 0 错误。

## 回滚策略

所有修改限于 `soc_ocv.h/c` + `bms_app.c` 1 行。如需回滚，`git revert` 即可。无数据库/配置迁移。
