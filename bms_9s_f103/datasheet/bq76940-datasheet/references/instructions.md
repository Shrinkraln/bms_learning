# bq76940 Instruction Set / Command Table

> **PURPOSE:** Every I2C command/subcommand the chip recognizes — register addresses, parameters, and expected responses.

## Encoding Convention

- Register addresses are given in **hex** (e.g., `0x00`).
- I2C 7-bit device addresses: 0x08 (default) or 0x18 with CRC option.
- Subcommands are written to 0x00-0x01 with data bytes following.

---

## I2C Interface Overview

The BQ769x0 AFE implements a standard **100-kHz I2C interface** and acts as a slave device. The I2C device address is 7-bits and is factory programmed (see Device Comparison Table).

### I2C Transaction Types

**Single-byte write:**
```
START + SlaveAddr(W) + ACK + RegAddr + ACK + Data + ACK + [CRC + ACK] + STOP
```

**Block write (multiple data bytes):**
```
START + SlaveAddr(W) + ACK + RegAddr + ACK + Data1 + ACK + Data2 + ACK + ... + [CRC + ACK] + STOP
```

**Read with Repeated Start (standard):**
```
START + SlaveAddr(W) + ACK + RegAddr + ACK + START + SlaveAddr(R) + ACK + Data + ACK + [CRC + NACK] + STOP
```

**Read without Repeated Start:**
```
START + SlaveAddr(W) + ACK + RegAddr + ACK + STOP + START + SlaveAddr(R) + ACK + Data + ACK + [CRC + NACK] + STOP
```

The I2C block auto-increments the register address after each data byte.

### CRC (Optional)

When enabled (CRC-enabled device option):

**Polynomial:** x^8 + x^2 + x + 1
**Initial value:** 0x00

- **Single-byte write CRC:** Calculated over slave address + register address + data byte.
- **Block write CRC (first byte):** Slave address + register address + data byte.
- **Block write CRC (subsequent bytes):** Data byte only.
- **Single-byte read CRC:** Calculated after second start using slave address + data byte.
- **Block read CRC (first byte):** Calculated after second start using slave address + data byte.
- **Block read CRC (subsequent bytes):** Data byte only.

When the slave detects a bad CRC, it NACKs the CRC byte and goes to idle.

---

## Register Map (Command Table)

| Addr | Name        | D7           | D6        | D5             | D4          | D3         | D2         | D1         | D0         | Access |
|------|-------------|--------------|-----------|----------------|-------------|------------|------------|------------|------------|--------|
| 0x00 | SYS_STAT    | CC_READY     | RSVD      | DEVICE_XREADY  | OVRD_ALERT  | UV         | OV         | SCD        | OCD        | RW     |
| 0x01 | CELLBAL1    | RSVD         | RSVD      | RSVD           | CB5         | CB4        | CB3        | CB2        | CB1        | RW(1)  |
| 0x02 | CELLBAL2(3) | RSVD         | RSVD      | RSVD           | CB10        | CB9        | CB8        | CB7        | CB6        | RW(1)  |
| 0x03 | CELLBAL3(4) | RSVD         | RSVD      | RSVD           | CB15        | CB14       | CB13       | CB12       | CB11       | RW(1)  |
| 0x04 | SYS_CTRL1   | LOAD_PRESENT | RSVD      | RSVD           | ADC_EN      | TEMP_SEL   | RSVD       | SHUT_A     | SHUT_B     | RW(2)  |
| 0x05 | SYS_CTRL2   | DELAY_DIS    | CC_EN     | CC_ONESHOT     | RSVD        | RSVD       | RSVD       | DSG_ON     | CHG_ON     | RW     |
| 0x06 | PROTECT1    | RSNS         | RSVD      | RSVD           | SCD_D1      | SCD_D0     | SCD_T2     | SCD_T1     | SCD_T0     | RW     |
| 0x07 | PROTECT2    | RSVD         | OCD_D2    | OCD_D1         | OCD_D0      | OCD_T3     | OCD_T2     | OCD_T1     | OCD_T0     | RW     |
| 0x08 | PROTECT3    | UV_D1        | UV_D0     | OV_D1          | OV_D0       | RSVD       | RSVD       | RSVD       | RSVD       | RW     |
| 0x09 | OV_TRIP     | OV_T7        | OV_T6     | OV_T5          | OV_T4       | OV_T3      | OV_T2      | OV_T1      | OV_T0      | RW     |
| 0x0A | UV_TRIP     | UV_T7        | UV_T6     | UV_T5          | UV_T4       | UV_T3      | UV_T2      | UV_T1      | UV_T0      | RW     |
| 0x0B | CC_CFG      | RSVD         | RSVD      | CC_CFG5        | CC_CFG4     | CC_CFG3    | CC_CFG2    | CC_CFG1    | CC_CFG0    | RW     |
| 0x0C | VC1_HI      | RSVD         | RSVD      | VC1_D13        | VC1_D12     | VC1_D11    | VC1_D10    | VC1_D9     | VC1_D8     | R      |
| 0x0D | VC1_LO      | VC1_D7       | VC1_D6    | VC1_D5         | VC1_D4      | VC1_D3     | VC1_D2     | VC1_D1     | VC1_D0     | R      |
| 0x0E | VC2_HI      | RSVD         | RSVD      | VC2_D13        | VC2_D12     | VC2_D11    | VC2_D10    | VC2_D9     | VC2_D8     | R      |
| 0x0F | VC2_LO      | VC2_D7       | VC2_D6    | VC2_D5         | VC2_D4      | VC2_D3     | VC2_D2     | VC2_D1     | VC2_D0     | R      |
| 0x10 | VC3_HI      | RSVD         | RSVD      | VC3_D13        | VC3_D12     | VC3_D11    | VC3_D10    | VC3_D9     | VC3_D8     | R      |
| 0x11 | VC3_LO      | VC3_D7       | VC3_D6    | VC3_D5         | VC3_D4      | VC3_D3     | VC3_D2     | VC3_D1     | VC3_D0     | R      |
| 0x12 | VC4_HI      | RSVD         | RSVD      | VC4_D13        | VC4_D12     | VC4_D11    | VC4_D10    | VC4_D9     | VC4_D8     | R      |
| 0x13 | VC4_LO      | VC4_D7       | VC4_D6    | VC4_D5         | VC4_D4      | VC4_D3     | VC4_D2     | VC4_D1     | VC4_D0     | R      |
| 0x14 | VC5_HI      | RSVD         | RSVD      | VC5_D13        | VC5_D12     | VC5_D11    | VC5_D10    | VC5_D9     | VC5_D8     | R      |
| 0x15 | VC5_LO      | VC5_D7       | VC5_D6    | VC5_D5         | VC5_D4      | VC5_D3     | VC5_D2     | VC5_D1     | VC5_D0     | R      |
| 0x16 | VC6_HI(3)   | RSVD         | RSVD      | VC6_D13        | VC6_D12     | VC6_D11    | VC6_D10    | VC6_D9     | VC6_D8     | R      |
| 0x17 | VC6_LO(3)   | VC6_D7       | VC6_D6    | VC6_D5         | VC6_D4      | VC6_D3     | VC6_D2     | VC6_D1     | VC6_D0     | R      |
| 0x18 | VC7_HI(3)   | RSVD         | RSVD      | VC7_D13        | VC7_D12     | VC7_D11    | VC7_D10    | VC7_D9     | VC7_D8     | R      |
| 0x19 | VC7_LO(3)   | VC7_D7       | VC7_D6    | VC7_D5         | VC7_D4      | VC7_D3     | VC7_D2     | VC7_D1     | VC7_D0     | R      |
| 0x1A | VC8_HI(3)   | RSVD         | RSVD      | VC8_D13        | VC8_D12     | VC8_D11    | VC8_D10    | VC8_D9     | VC8_D8     | R      |
| 0x1B | VC8_LO(3)   | VC8_D7       | VC8_D6    | VC8_D5         | VC8_D4      | VC8_D3     | VC8_D2     | VC8_D1     | VC8_D0     | R      |
| 0x1C | VC9_HI(3)   | RSVD         | RSVD      | VC9_D13        | VC9_D12     | VC9_D11    | VC9_D10    | VC9_D9     | VC9_D8     | R      |
| 0x1D | VC9_LO(3)   | VC9_D7       | VC9_D6    | VC9_D5         | VC9_D4      | VC9_D3     | VC9_D2     | VC9_D1     | VC9_D0     | R      |
| 0x1E | VC10_HI(3)  | RSVD         | RSVD      | VC10_D13       | VC10_D12    | VC10_D11   | VC10_D10   | VC10_D9    | VC10_D8    | R      |
| 0x1F | VC10_LO(3)  | VC10_D7      | VC10_D6   | VC10_D5        | VC10_D4     | VC10_D3    | VC10_D2    | VC10_D1    | VC10_D0    | R      |
| 0x20 | VC11_HI(4)  | RSVD         | RSVD      | VC11_D13       | VC11_D12    | VC11_D11   | VC11_D10   | VC11_D9    | VC11_D8    | R      |
| 0x21 | VC11_LO(4)  | VC11_D7      | VC11_D6   | VC11_D5        | VC11_D4     | VC11_D3    | VC11_D2    | VC11_D1    | VC11_D0    | R      |
| 0x22 | VC12_HI(4)  | RSVD         | RSVD      | VC12_D13       | VC12_D12    | VC12_D11   | VC12_D10   | VC12_D9    | VC12_D8    | R      |
| 0x23 | VC12_LO(4)  | VC12_D7      | VC12_D6   | VC12_D5        | VC12_D4     | VC12_D3    | VC12_D2    | VC12_D1    | VC12_D0    | R      |
| 0x24 | VC13_HI(4)  | RSVD         | RSVD      | VC13_D13       | VC13_D12    | VC13_D11   | VC13_D10   | VC13_D9    | VC13_D8    | R      |
| 0x25 | VC13_LO(4)  | VC13_D7      | VC13_D6   | VC13_D5        | VC13_D4     | VC13_D3    | VC13_D2    | VC13_D1    | VC13_D0    | R      |
| 0x26 | VC14_HI(4)  | RSVD         | RSVD      | VC14_D13       | VC14_D12    | VC14_D11   | VC14_D10   | VC14_D9    | VC14_D8    | R      |
| 0x27 | VC14_LO(4)  | VC14_D7      | VC14_D6   | VC14_D5        | VC14_D4     | VC14_D3    | VC14_D2    | VC14_D1    | VC14_D0    | R      |
| 0x28 | VC15_HI(4)  | RSVD         | RSVD      | VC15_D13       | VC15_D12    | VC15_D11   | VC15_D10   | VC15_D9    | VC15_D8    | R      |
| 0x29 | VC15_LO(4)  | VC15_D7      | VC15_D6   | VC15_D5        | VC15_D4     | VC15_D3    | VC15_D2    | VC15_D1    | VC15_D0    | R      |
| 0x2A | BAT_HI      | BAT_D15      | BAT_D14   | BAT_D13        | BAT_D12     | BAT_D11    | BAT_D10    | BAT_D9     | BAT_D8     | R      |
| 0x2B | BAT_LO      | BAT_D7       | BAT_D6    | BAT_D5         | BAT_D4      | BAT_D3     | BAT_D2     | BAT_D1     | BAT_D0     | R      |
| 0x2C | TS1_HI      | RSVD         | RSVD      | TS1_D13        | TS1_D12     | TS1_D11    | TS1_D10    | TS1_D9     | TS1_D8     | R      |
| 0x2D | TS1_LO      | TS1_D7       | TS1_D6    | TS1_D5         | TS1_D4      | TS1_D3     | TS1_D2     | TS1_D1     | TS1_D0     | R      |
| 0x2E | TS2_HI(3)   | RSVD         | RSVD      | TS2_D13        | TS2_D12     | TS2_D11    | TS2_D10    | TS2_D9     | TS2_D8     | R      |
| 0x2F | TS2_LO(3)   | TS2_D7       | TS2_D6    | TS2_D5         | TS2_D4      | TS2_D3     | TS2_D2     | TS2_D1     | TS2_D0     | R      |
| 0x30 | TS3_HI(4)   | RSVD         | RSVD      | TS3_D13        | TS3_D12     | TS3_D11    | TS3_D10    | TS3_D9     | TS3_D8     | R      |
| 0x31 | TS3_LO(4)   | TS3_D7       | TS3_D6    | TS3_D5         | TS3_D4      | TS3_D3     | TS3_D2     | TS3_D1     | TS3_D0     | R      |
| 0x32 | CC_HI       | CC_D15       | CC_D14    | CC_D13         | CC_D12      | CC_D11     | CC_D10     | CC_D9      | CC_D8      | R      |
| 0x33 | CC_LO       | CC_D7        | CC_D6     | CC_D5          | CC_D4       | CC_D3      | CC_D2      | CC_D1      | CC_D0      | R      |
| 0x50 | ADCGAIN1    | RSVD         | RSVD      | RSVD           | RSVD        | ADCGAIN4   | ADCGAIN3   | RSVD       | RSVD       | R      |
| 0x51 | ADCOFFSET   | ADCOFFSET7   | ADCOFFSET6| ADCOFFSET5     | ADCOFFSET4  | ADCOFFSET3 | ADCOFFSET2 | ADCOFFSET1 | ADCOFFSET0 | R      |
| 0x59 | ADCGAIN2    | ADCGAIN2     | ADCGAIN1  | ADCGAIN0       | RSVD        | RSVD       | RSVD       | RSVD       | RSVD       | R      |

Notes:
1. Bits 4-0 only (bits 7-5 are read-only/RSVD).
2. Bit 7 (LOAD_PRESENT) is read-only.
3. These registers are only valid for BQ76930 and BQ76940.
4. These registers are only valid for BQ76940.

## Address Space Summary

| Address Range | Content |
|--------------|---------|
| 0x00 | SYS_STAT (System Status) |
| 0x01-0x03 | CELLBAL1-3 (Cell Balancing Control) |
| 0x04-0x05 | SYS_CTRL1-2 (System Control) |
| 0x06-0x08 | PROTECT1-3 (Protection Configuration) |
| 0x09-0x0A | OV_TRIP, UV_TRIP (Voltage Thresholds) |
| 0x0B | CC_CFG (Coulomb Counter Config) |
| 0x0C-0x29 | VC1_HI through VC15_LO (Cell Voltage Readings) |
| 0x2A-0x2B | BAT_HI, BAT_LO (Pack Voltage) |
| 0x2C-0x31 | TS1_HI through TS3_LO (Temperature Readings) |
| 0x32-0x33 | CC_HI, CC_LO (Coulomb Counter Reading) |
| 0x50 | ADCGAIN1 (ADC Gain MSB) |
| 0x51 | ADCOFFSET (ADC Offset) |
| 0x59 | ADCGAIN2 (ADC Gain LSB) |

## Key I2C Command Sequences

**SHIP Mode Entry (consecutive writes to SYS_CTRL1, addr 0x04):**
```
Write #1: SYS_CTRL1 = xxxx x001  (SHUT_A=0, SHUT_B=1)
Write #2: SYS_CTRL1 = xxxx x010  (SHUT_A=1, SHUT_B=0)
```
Starting from SHUT_A=0, SHUT_B=0 state.

**CC One-Shot Reading:**
```
Write SYS_CTRL2 (0x05) with CC_ONESHOT=1, CC_EN=0
Wait 250ms
Read CC_HI (0x32) and CC_LO (0x33)
```

**CC Continuous Mode:**
```
Write SYS_CTRL2 (0x05) with CC_EN=1
// CC readings update every 250ms, ALERT toggles on each completion
```

**Cell Voltage Reading (atomic read):**
```
Write VC1_HI address (0x0C)
Repeated Start
Read VC1_HI, VC1_LO (auto-increment), then subsequent cell registers
```

(All information sourced from datasheet Sections 8.3.1.4, 8.4, and 8.5)
