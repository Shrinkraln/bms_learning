#!/usr/bin/env python3
"""
bms_ping.py — CAN 通道验证脚本
发送 QUERY 指令到 BMS (STM32F103 + BQ76940), 等待响应帧, 验证通信是否打通。

用法:
    python bms_ping.py                          # 默认 PCAN_USBBUS1, 500kbps
    python bms_ping.py --channel PCAN_USBBUS2   # 指定通道
    python bms_ping.py --bitrate 250000         # 指定波特率
    python bms_ping.py --subcmd 0x02            # 查询电芯电压
    python bms_ping.py --list                   # 列出可用 CAN 接口

依赖:
    pip install python-can
    Peak PCAN driver (PCANBasic.dll)
"""

import argparse
import struct
import sys
import time
from typing import Optional

try:
    import can
except ImportError:
    print("请安装 python-can: pip install python-can")
    sys.exit(1)


# ============================================================
# CAN 协议常量 (与 BMS 固件 can_cmd.h 保持一致)
# ============================================================

CAN_ID_QUERY       = 0x200   # Host → BMS 查询指令
CAN_ID_BMS_STATUS  = 0x100   # BMS → Host 状态帧
CAN_ID_CELL_VOLT_1 = 0x110   # BMS → Host 电芯 1-4 电压
CAN_ID_CELL_VOLT_5  = 0x111  # BMS → Host 电芯 5-8 电压
CAN_ID_STATUS2     = 0x120   # BMS → Host 综合状态
CAN_ID_TEMPERATURE = 0x121   # BMS → Host 温度
CAN_ID_SOC_OCV     = 0x130   # BMS → Host SOC/OCV

# 查询子命令
QUERY_ALL        = 0x00
QUERY_STATUS     = 0x01
QUERY_CELLS      = 0x02
QUERY_PROTECTION = 0x03
QUERY_SOC        = 0x04

# 保护等级名称
PROT_LEVEL_NAMES = {
    0: "正常",
    1: "警告",
    2: "保护",
    3: "故障",
}

# 故障位名称 (bit 0-11)
FAULT_NAMES = [
    "单芯过压", "单芯欠压", "总压过压", "总压欠压",
    "放电过流", "充电过流", "短路",   "过温",
    "欠温",     "压差过大", "通信丢失", "看门狗",
]


# ============================================================
# 响应解码
# ============================================================

def decode_status_frame(data: bytes) -> dict:
    """解码 0x100/0x120 BMS_STATUS 帧"""
    if len(data) < 8:
        return {}
    soc_pct  = data[0]
    pack_mv  = (data[1] << 8) | data[2]
    cur_raw  = (data[3] << 8) | data[4]
    cur_ma   = struct.unpack('<h', data[3:5])[0] * 10  # signed int16 * 10
    ctrl     = data[5]
    cell9_mv = (data[6] << 8) | data[7]

    prot_lv  = ctrl & 0x03
    fet_chg  = (ctrl >> 2) & 0x01
    fet_dsg  = (ctrl >> 3) & 0x01
    balancing = (ctrl >> 4) & 0x01
    afe_online = (ctrl >> 5) & 0x01

    return {
        "soc_pct":     soc_pct,
        "pack_mv":     pack_mv,
        "pack_v":      f"{pack_mv / 1000:.2f}V",
        "current_ma":  cur_ma,
        "current_a":   f"{cur_ma / 1000:.2f}A",
        "prot_level":  prot_lv,
        "prot_name":   PROT_LEVEL_NAMES.get(prot_lv, f"未知({prot_lv})"),
        "fet_chg":     bool(fet_chg),
        "fet_dsg":     bool(fet_dsg),
        "balancing":   bool(balancing),
        "afe_online":  bool(afe_online),
        "cell9_mv":    cell9_mv,
    }


def decode_cell_frame(data: bytes, start: int = 0) -> dict:
    """解码 0x110/0x111 电芯电压帧 (每帧 4 节电芯, big-endian uint16)"""
    cells = {}
    for i in range(min(4, len(data) // 2)):
        mv = (data[i * 2] << 8) | data[i * 2 + 1]
        cells[f"cell{start + i + 1}"] = mv
    return cells


def decode_soc_frame(data: bytes) -> dict:
    """解码 0x130 SOC_OCV 帧"""
    if len(data) < 8:
        return {}
    soc_permil   = (data[0] << 8) | data[1]
    real_soc     = (data[2] << 8) | data[3]
    remaining_mah = ((data[4] << 8) | data[5]) * 100
    q_max_mah     = ((data[6] << 8) | data[7]) * 100
    return {
        "soc_permil":     soc_permil,
        "soc_pct":        f"{soc_permil / 10:.1f}%",
        "real_soc":       f"{real_soc / 10:.1f}%",
        "remaining_mah":  remaining_mah,
        "q_max_mah":      q_max_mah,
    }


def decode_temperature_frame(data: bytes) -> dict:
    """解码 0x121 温度帧 (3 路 int16, 单位 0.1°C)"""
    temps = {}
    for i in range(min(3, len(data) // 2)):
        raw = struct.unpack('<h', data[i*2:i*2+2])[0]
        temps[f"temp{i+1}"] = f"{raw / 10:.1f}°C"
    return temps


def decode_fault_frame(data: bytes) -> dict:
    """解码 0x101 故障帧"""
    if len(data) < 8:
        return {}
    prot_level  = data[0]
    faults_raw  = (data[1] << 8) | data[2]
    cell_max_mv = (data[3] << 8) | data[4]
    cell_min_mv = (data[5] << 8) | data[6]
    max_temp_decic = struct.unpack('<b', data[7:8])[0] - 40
    max_temp_c = max_temp_decic

    active_faults = []
    for bit in range(12):
        if faults_raw & (1 << bit):
            name = FAULT_NAMES[bit] if bit < len(FAULT_NAMES) else f"未知({bit})"
            active_faults.append(name)

    return {
        "prot_level":   prot_level,
        "faults_raw":   f"0x{faults_raw:04X}",
        "active_faults": active_faults,
        "cell_max_mv":  cell_max_mv,
        "cell_min_mv":  cell_min_mv,
        "max_temp":     f"{max_temp_c}°C",
    }


# ============================================================
# CAN 接口
# ============================================================

def list_interfaces():
    """列出可用的 CAN 接口"""
    print("🔍 扫描可用 CAN 接口...")
    configs = can.detect_available_configs()
    if not configs:
        print("   未检测到 CAN 接口 (请检查驱动/USB 连接)")
        return []
    for cfg in configs:
        print(f"   {cfg['interface']}: {cfg['channel']}")
    return configs


def open_can(interface: str, channel: str, bitrate: int) -> Optional[can.Bus]:
    """打开 CAN 总线"""
    try:
        bus = can.interface.Bus(
            interface=interface,
            channel=channel,
            bitrate=bitrate,
        )
        return bus
    except can.exceptions.CanInterfaceNotImplementedError:
        print(f"   ❌ 不支持的 CAN 接口类型: {interface}")
        print(f"   已安装接口: {[c['interface'] for c in can.detect_available_configs()]}")
        return None
    except Exception as e:
        print(f"   ❌ 无法打开 CAN 设备: {e}")
        return None


def send_query(bus: can.Bus, sub_cmd: int) -> bool:
    """发送查询帧"""
    msg = can.Message(
        arbitration_id=CAN_ID_QUERY,
        data=[sub_cmd, 0, 0, 0, 0, 0, 0, 0],
        is_extended_id=False,
    )
    try:
        bus.send(msg)
        sub_names = {0x00: "QUERY_ALL", 0x01: "QUERY_STATUS", 0x02: "QUERY_CELLS",
                     0x03: "QUERY_PROTECTION", 0x04: "QUERY_SOC"}
        name = sub_names.get(sub_cmd, f"0x{sub_cmd:02X}")
        print(f"📤 发送 PING: CAN ID=0x{CAN_ID_QUERY:03X}  data=[{sub_cmd:02X}]  ({name})")
        return True
    except Exception as e:
        print(f"   ❌ 发送失败: {e}")
        return False


# ============================================================
# 主逻辑
# ============================================================

def print_separator(char="─", width=45):
    print(char * width)


def print_result(title: str, value, indent: int = 3):
    prefix = " " * indent
    print(f"{prefix}{title}: {value}")


def main():
    parser = argparse.ArgumentParser(
        description="BMS CAN 通道验证工具 — 发送查询帧, 验证 BMS 响应",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python bms_ping.py                           # 默认配置
  python bms_ping.py --subcmd 0x02             # 查询电芯电压
  python bms_ping.py --channel PCAN_USBBUS2    # 指定 CAN 通道
  python bms_ping.py --list                     # 列出可用接口
        """,
    )
    parser.add_argument("--interface", default="pcan",
                        help="CAN 接口类型 (默认: pcan)")
    parser.add_argument("--channel", default="PCAN_USBBUS1",
                        help="CAN 通道名 (默认: PCAN_USBBUS1)")
    parser.add_argument("--bitrate", type=int, default=500000,
                        help="CAN 波特率 (默认: 500000)")
    parser.add_argument("--subcmd", type=lambda x: int(x, 0), default=0x00,
                        help="查询子命令 0x00-0x04 (默认: 0x00=QUERY_ALL)")
    parser.add_argument("--timeout", type=float, default=2.0,
                        help="响应超时秒数 (默认: 2.0)")
    parser.add_argument("--list", action="store_true",
                        help="列出可用 CAN 接口后退出")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="显示所有收到的 CAN 帧 (包括周期性上报)")
    args = parser.parse_args()

    # --list 模式
    if args.list:
        list_interfaces()
        return 0

    print()
    print("╔═══════════════════════════════════════════════════╗")
    print("║       BMS CAN 通道验证工具  bms_ping.py           ║")
    print("╚═══════════════════════════════════════════════════╝")
    print()

    # 1. 打开 CAN
    print(f"🔌 打开 CAN 接口: {args.interface}:{args.channel} @ {args.bitrate // 1000}kbps...")
    bus = open_can(args.interface, args.channel, args.bitrate)
    if bus is None:
        print()
        print("💡 提示:")
        print("   1. 确认 PCAN-USB 已插入并安装驱动 (PCANBasic.dll)")
        print("   2. 运行 --list 查看可用接口")
        print("   3. 确认 BMS 板已上电, CAN 总线终端电阻正确")
        return 1
    print("   ✅ CAN 接口已打开")
    print()

    try:
        # 2. 清空接收缓冲区
        while bus.recv(timeout=0.05) is not None:
            pass

        # 3. 发送查询帧
        if not send_query(bus, args.subcmd):
            return 1
        print()

        # 4. 等待响应
        print(f"⏳ 等待响应 (超时 {args.timeout}s)...")
        deadline = time.monotonic() + args.timeout
        response_count = 0
        extra_count = 0
        responded = False

        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            msg = bus.recv(timeout=min(remaining, 0.5))
            if msg is None:
                continue

            fid = msg.arbitration_id

            # 响应帧 (查询的直接回复)
            if fid in (CAN_ID_BMS_STATUS, CAN_ID_CELL_VOLT_1,
                        CAN_ID_SOC_OCV, CAN_ID_STATUS2,
                        CAN_ID_TEMPERATURE):
                response_count += 1
                if not responded:
                    responded = True

                print(f"📥 收到响应 #{response_count}: CAN ID=0x{fid:03X}"
                      f"  len={msg.dlc}  data=[{' '.join(f'{b:02X}' for b in msg.data[:msg.dlc])}]")
                print_separator()

                # 解码
                decoded = {}
                if fid == CAN_ID_BMS_STATUS or fid == CAN_ID_STATUS2:
                    decoded = decode_status_frame(msg.data)
                    if decoded:
                        print_result("SOC",        f"{decoded['soc_pct']}%")
                        print_result("Pack V",     decoded['pack_v'])
                        print_result("Current",    decoded['current_a'])
                        print_result("Prot Level", f"{decoded['prot_level']} ({decoded['prot_name']})")
                        print_result("FET CHG",    "ON" if decoded['fet_chg'] else "OFF")
                        print_result("FET DSG",    "ON" if decoded['fet_dsg'] else "OFF")
                        print_result("Balancing",  "是" if decoded['balancing'] else "否")
                        print_result("AFE Online", "✅" if decoded['afe_online'] else "❌")
                        print_result("Cell 9",     f"{decoded['cell9_mv']} mV")
                        print_separator()

                elif fid == CAN_ID_CELL_VOLT_1:
                    decoded = decode_cell_frame(msg.data, start=0)
                    for key, val in decoded.items():
                        print_result(f"{key}", f"{val} mV")
                    print_separator()

                elif fid == CAN_ID_CELL_VOLT_5:
                    decoded = decode_cell_frame(msg.data, start=4)
                    for key, val in decoded.items():
                        print_result(f"{key}", f"{val} mV")
                    print_separator()

                elif fid == CAN_ID_SOC_OCV:
                    decoded = decode_soc_frame(msg.data)
                    if decoded:
                        print_result("SOC",          decoded['soc_pct'])
                        print_result("Real SOC",     decoded['real_soc'])
                        print_result("Remaining",    f"{decoded['remaining_mah']} mAh")
                        print_result("Q Max",        f"{decoded['q_max_mah']} mAh")
                        print_separator()

                elif fid == CAN_ID_TEMPERATURE:
                    decoded = decode_temperature_frame(msg.data)
                    for key, val in decoded.items():
                        print_result(f"{key}", val)
                    print_separator()

                print()

            # 故障帧 (0x101) — 事件驱动, 也可能紧随查询出现
            elif fid == 0x101:
                decoded = decode_fault_frame(msg.data)
                print(f"📥 故障帧: CAN ID=0x{fid:03X}"
                      f"  len={msg.dlc}  data=[{' '.join(f'{b:02X}' for b in msg.data[:msg.dlc])}]")
                print_separator()
                if decoded.get("active_faults"):
                    print_result("故障", ", ".join(decoded["active_faults"]))
                else:
                    print_result("故障", "无")
                print_separator()
                print()

            else:
                # 未知帧或周期帧 (0x111等)
                extra_count += 1
                if args.verbose:
                    print(f"📎 其他帧: CAN ID=0x{fid:03X}"
                          f"  data=[{' '.join(f'{b:02X}' for b in msg.data[:msg.dlc])}]")

        # 5. 结果判定
        print()
        if responded:
            print(f"✅ CAN 通道验证通过! BMS 在线, 通信正常. (收到 {response_count} 帧)")
            if extra_count:
                print(f"   (另有 {extra_count} 帧周期上报)")
            return 0
        else:
            print("❌ CAN 通道验证失败! 超时未收到 BMS 响应.")
            print()
            print("💡 排查建议:")
            print("   1. BMS 板是否已上电? LED 是否闪烁?")
            print("   2. CAN H / CAN L 接线是否正确?")
            print("   3. 终端电阻是否已接 (120Ω 在 CAN H-L 之间)?")
            print("   4. 波特率是否匹配? (BMS 固件默认 500kbps)")
            print("   5. 尝试: python bms_ping.py --list 确认 PCAN 设备已识别")
            if extra_count:
                print(f"   ℹ️  收到 {extra_count} 帧其他 CAN 消息,"
                      f" BMS 可能正在周期上报但未响应查询 (子命令错误?)")
            return 1

    except KeyboardInterrupt:
        print("\n⏹ 用户中断")
        return 130
    finally:
        bus.shutdown()
        print("🔌 CAN 接口已关闭")


if __name__ == "__main__":
    sys.exit(main())
