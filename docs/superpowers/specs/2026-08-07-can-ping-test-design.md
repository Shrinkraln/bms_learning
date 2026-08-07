# CAN 通道验证脚本 (bms_ping.py) 设计文档

**日期**: 2026-08-07
**状态**: 已确认
**类型**: 独立测试工具

## 1. 目的

创建独立 Python 脚本 `bms_ping.py`，通过 PCAN-USB 向 BMS 发送 CAN 查询帧并验证响应，确认 QT 上位机与 BMS 之间的 CAN 总线物理链路和应用层协议均已打通。

## 2. 约束

- **不修改任何现有代码**：BMS 固件和 QT 上位机代码不受影响
- **独立运行**：脚本仅依赖 `python-can` + PCAN 驱动，无需 QT 运行时
- **即用即走**：发送查询 → 等待响应 → 打印结果 → 退出，不持续占用 CAN 总线

## 3. 架构

```
┌──────────────┐    CAN 0x200 (QUERY)     ┌──────────────────┐
│  bms_ping.py  │ ──────────────────────→ │  BMS (STM32F103) │
│  (python-can  │                          │  task_can_rx     │
│   PCAN bkd)   │ ←────────────────────── │  can_cmd_dispatch │
│               │     CAN 0x100 (RESP)     │  can_send(resp)  │
└──────────────┘                          └──────────────────┘
```

## 4. CAN 协议 (复用现有)

| 方向 | CAN ID | 描述 |
|------|--------|------|
| Host→BMS | `0x200` | QUERY 指令，data[0]=子命令 |
| BMS→Host | `0x100` | BMS_STATUS 响应 (SOC/电压/电流/保护等级) |
| BMS→Host | `0x110` | CELL_VOLT_1_4 响应 |
| BMS→Host | `0x120` | STATUS 响应 (含 FET/均衡/AFE 在线状态) |
| BMS→Host | `0x130` | SOC_OCV 响应 |

### 子命令定义

| sub_cmd | 含义 | 响应帧 ID |
|---------|------|----------|
| `0x00` | QUERY_ALL | 0x100 |
| `0x01` | QUERY_STATUS | 0x100 |
| `0x02` | QUERY_CELLS | 0x110 |
| `0x03` | QUERY_PROTECTION | 0x100 |
| `0x04` | QUERY_SOC | 0x130 |

## 5. 测试流程

1. 打开 PCAN-USB 通道 (`PCAN_USBBUS1`, 500kbps)
2. 发送 QUERY_ALL 帧: `ID=0x200, data=[0x00]`
3. 超时 1000ms 等待响应帧
4. 若收到 0x100/0x110/0x120/0x130 → 解码数据并打印 → 报告成功
5. 若超时 → 报告失败，提示检查接线/波特率/终端电阻

## 6. 输出格式

```
🔌 打开 PCAN-USB (500kbps)... OK
📤 发送 PING: CAN ID=0x200 data=[00 00 00 00 00 00 00 00]
📥 收到响应: CAN ID=0x100 len=8 data=[64 0D 48 FF F6 00 00 00]
   ┌─────────────────────────────┐
   │ SOC:      100%              │
   │ Pack V:   3400 mV           │
   │ Current:  -100 mA           │
   │ Prot Lv:  0 (正常)          │
   └─────────────────────────────┘
✅ CAN 通道验证通过! BMS 在线, 通信正常.
```

## 7. 错误处理

| 错误 | 原因 | 建议 |
|------|------|------|
| PCAN device not found | USB 未插 / 驱动未装 | 检查设备管理器 |
| No response (timeout) | BMS 未上电 / CAN 接线 / 终端电阻 | 检查硬件连接 |
| DLC too short | 协议不匹配 / 固件版本不一致 | 检查响应帧 len |

## 8. 文件清单

| 文件 | 位置 | 描述 |
|------|------|------|
| `bms_ping.py` | `tools/bms_ping.py` | 主测试脚本 |
| `README.md` | `tools/README.md` | 使用说明 (可选) |

## 9. 依赖

- Python 3.8+
- `python-can` >= 4.0 (`pip install python-can`)
- Peak PCAN driver (PCANBasic.dll)
