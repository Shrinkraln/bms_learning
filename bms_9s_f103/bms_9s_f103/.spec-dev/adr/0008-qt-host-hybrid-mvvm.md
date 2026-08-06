# ADR-0008: QT 上位机混合 Q_PROPERTY + QAbstractListModel 架构

## 背景

QT 上位机需要在 QML 界面中展示 BMS 遥测数据。数据分为两类：标量值（总压、电流、SOC、保护等级等 ~12 个字段）和列表值（9 节电芯电压、3 路温度、活跃故障列表）。需要选择数据模型向 QML 暴露的方式。

## 决定

标量值使用 `Q_PROPERTY` + NOTIFY 信号暴露在单一 `BmsDataModel` QObject 上；列表值使用独立 `QAbstractListModel` 子类（`CellVoltageModel`、`TemperatureModel`、`FaultListModel`）作为 `BmsDataModel` 的子属性暴露。上游数据通过 30ms QTimer 批量快照写入，逐字段比较新旧值，仅变更时 `setProperty` + emit NOTIFY。

## 理由

- 标量走 Q_PROPERTY：QML 绑定语法简洁（`text: bms.packVoltage.toFixed(1) + " mV"`），单属性单信号精确定位变更源头
- 列表走 QAbstractListModel：ListView 懒加载、role-based 数据访问、`dataChanged` 仅通知变更行，QML 端无需全量重建。9 节电芯用 Repeater + model 即可
- 30ms 批量快照：12 个属性若逐字段发射 NOTIFY = 120 次信号/秒（@100ms 周期），批量比较后仅发射 1 次快照信号 = 33 次/秒，避免 QML 绑定链的级联重评估
- 分层隔离：`BmsDataModel` 不感知 CAN 帧格式，`BmsProtocolDecoder` 不感知 QML——两者仅通过 `BmsSnapshot` 结构体交换数据，单元测试无需启动 QML 引擎

## 被否方案

- **纯 Q_PROPERTY（30+ 个属性）**: 每个数据字段一个属性。9 节电芯 = 9 个属性 × NOTIFY = 扩展性差（108 节电池需 108 个属性）。信号洪峰在高频更新下压垮 QML 绑定引擎
- **纯 QAbstractTableModel（单一巨型 model）**: 标量值走 model role 访问语法繁琐（`model.data(model.index(0,0), PACK_VOLTAGE_ROLE)`），QML 端无直接属性绑定，需 JS 函数中转。table model 对"一条记录多列"强，对"一组标量"弱
