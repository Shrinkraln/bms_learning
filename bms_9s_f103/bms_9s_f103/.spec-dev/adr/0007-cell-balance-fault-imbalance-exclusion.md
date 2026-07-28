# ADR-0007: CELL_IMBALANCE 故障不屏蔽均衡

## 背景

task_balance 需要在故障时停止均衡以避免安全风险（如 OV 故障时继续充电会加剧过压）。然而，`FAULT_CELL_IMBALANCE` 故障本身定义为电芯压差过大——而均衡恰恰是消除电芯压差的唯一自动修复手段。

## 决策

`FAULT_CELL_IMBALANCE` 从均衡屏蔽故障掩码中排除。

`FAULT_MASK_BLOCK_BALANCE = ALL_FAULTS & ~FAULT_CELL_IMBALANCE`

## 理由

- **死锁预防**：若 CELL_IMBALANCE 阻止均衡，而均衡是其唯一修复手段，则系统陷入无法自动恢复的死锁
- **语义正确**：均衡是 CELL_IMBALANCE 的**修复动作**，而非其同级的互斥操作。其他故障（OV/UV/OC/OT）与均衡是互斥的——均衡不能修复过压
- **安全无虞**：均衡是被动耗散（5mA），即使 CELL_IMBALANCE 已触发，开启均衡不会加剧故障（与 OV 下继续充电不同）

## 备选方案（被否决）

- **CELL_IMBALANCE 也阻止均衡**：压差 >500mV 时需人工介入。否决：失去了自动恢复的最后手段，且正常工况下均衡会在 50mV 启动、远早于 500mV 的 CELL_IMBALANCE 阈值
