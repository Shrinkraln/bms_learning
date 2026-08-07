# bq76940 Register Map

> **PURPOSE:** This file documents every register bit-field — name, position, access type, reset value, and functional description.

## Reading This File

Each register is documented as a table. Bit positions use 0-indexed numbering (bit 0 = LSB). Access types: R = read-only, W = write-only, R/W = read/write, W1C = write-1-to-clear, VOL = volatile (may change between reads).

---

## SYS_STAT (0x00) — System Status

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | CC_READY | RSVD | DEVICE_XREADY | OVRD_ALERT | UV | OV | SCD | OCD |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | RW | RW | RW | RW | RW | RW | RW | RW |

> Bits in SYS_STAT may be cleared by writing a "1" to the corresponding bit. Writing a "0" does not change the state.

**CC_READY (Bit 7)**: Indicates that a fresh coulomb counter reading is available. If not cleared between two adjacent CC readings, remains latched to 1.
- 0 = Fresh CC reading not yet available or cleared by host
- 1 = Fresh CC reading is available. Remains latched high until cleared by host

**DEVICE_XREADY (Bit 5)**: Internal chip fault indicator. May be set due to excessive system transients.
- 0 = Device is OK
- 1 = Internal chip fault detected. Recommend host clear this bit after waiting a few seconds

**OVRD_ALERT (Bit 4)**: External pull-up on the ALERT pin indicator. Only active when ALERT pin is not already being driven high by AFE.
- 0 = No external override detected
- 1 = External override detected. Latched high until cleared by host

**UV (Bit 3)**: Undervoltage fault event indicator.
- 0 = No UV fault detected
- 1 = UV fault detected. Latched high until cleared by host

**OV (Bit 2)**: Overvoltage fault event indicator.
- 0 = No OV fault detected
- 1 = OV fault detected. Latched high until cleared by host

**SCD (Bit 1)**: Short circuit in discharge fault event indicator.
- 0 = No SCD fault detected
- 1 = SCD fault detected. Latched high until cleared by host

**OCD (Bit 0)**: Over current in discharge fault event indicator.
- 0 = No OCD fault detected
- 1 = OCD fault detected. Latched high until cleared by host

---

## CELLBAL1 (0x01) — Cell Balancing 1 (VC1-VC5)

For BQ76920, BQ76930, and BQ76940.

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | — | CB5 | CB4 | CB3 | CB2 | CB1 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | R | RW | RW | RW | RW | RW |

**CBx (Bits 4-0)**: Cell balancing control for cells 1-5.
- 0 = Cell balancing on Cell "x" disabled
- 1 = Cell balancing on Cell "x" enabled

---

## CELLBAL2 (0x02) — Cell Balancing 2 (VC6-VC10)

For BQ76930 and BQ76940 only.

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | — | CB10 | CB9 | CB8 | CB7 | CB6 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | R | RW | RW | RW | RW | RW |

**CBx (Bits 4-0)**: Cell balancing control for cells 6-10.
- 0 = Cell balancing disabled
- 1 = Cell balancing enabled

---

## CELLBAL3 (0x03) — Cell Balancing 3 (VC11-VC15)

For BQ76940 only.

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | — | CB15 | CB14 | CB13 | CB12 | CB11 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | R | RW | RW | RW | RW | RW |

**CBx (Bits 4-0)**: Cell balancing control for cells 11-15.
- 0 = Cell balancing disabled
- 1 = Cell balancing enabled

---

## SYS_CTRL1 (0x04) — System Control 1

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | LOAD_PRESENT | — | — | ADC_EN | TEMP_SEL | RSVD | SHUT_A | SHUT_B |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | R | RW | RW | RW | RW | RW |

**LOAD_PRESENT (Bit 7)**: Valid only when [CHG_ON] = 0. High if CHG pin exceeds V_LOAD_DETECT.
- 0 = CHG pin < V_LOAD_DETECT or [CHG_ON] = 1
- 1 = CHG pin > V_LOAD_DETECT, while [CHG_ON] = 0

**ADC_EN (Bit 4)**: ADC enable command.
- 0 = Disable voltage and temperature ADC (also disables OV protection)
- 1 = Enable voltage and temperature ADC (also enables OV protection)

**TEMP_SEL (Bit 3)**: TSx_HI and TSx_LO temperature source.
- 0 = Store internal die temperature voltage in TSx_HI/LO
- 1 = Store thermistor reading in TSx_HI/LO (all thermistor ports)

**RSVD (Bit 2)**: Reserved, do not set to 1.

**SHUT_A, SHUT_B (Bits 1-0)**: Shutdown command from host. Must be written in specific sequence:
- Starting from: SHUT_A=0, SHUT_B=0
- Write #1: SHUT_A=0, SHUT_B=1
- Write #2: SHUT_A=1, SHUT_B=0

---

## SYS_CTRL2 (0x05) — System Control 2

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | DELAY_DIS | CC_EN | CC_ONESHOT | RSVD | RSVD | RSVD | DSG_ON | CHG_ON |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | RW | RW | RW | RW | RW | RW | RW | RW |

**DELAY_DIS (Bit 7)**: Disable OV, UV, OCD, and SCD delays for faster production testing.
- 0 = Normal delay settings
- 1 = OV, UV, OCD, and SCD delay circuit bypassed (~250 ms)

**CC_EN (Bit 6)**: Coulomb counter continuous operation enable.
- 0 = Disable CC continuous readings
- 1 = Enable CC continuous readings (ignores CC_ONESHOT)

**CC_ONESHOT (Bit 5)**: Single 250-ms CC reading trigger.
- 0 = No action
- 1 = Enable single CC reading (only if CC_EN=0 and CC_READY=0). Cleared at conclusion of reading.

**DSG_ON (Bit 1)**: Discharge FET driver control.
- 0 = DSG is off
- 1 = DSG is on

**CHG_ON (Bit 0)**: Charge FET driver control.
- 0 = CHG is off
- 1 = CHG is on

---

## PROTECT1 (0x06) — Protection Configuration 1 (SCD)

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | RSNS | — | RSVD | SCD_D1 | SCD_D0 | SCD_T2 | SCD_T1 | SCD_T0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | RW | R | RW | RW | RW | RW | RW | RW |

**RSNS (Bit 7)**: Doubles OCD and SCD thresholds simultaneously.
- 0 = OCD and SCD thresholds at lower input range
- 1 = OCD and SCD thresholds at upper input range

**SCD_D1:0 (Bits 4-3)**: Short circuit in discharge delay setting.

| Code | Delay (us) |
|------|-----------|
| 0x0  | 70 |
| 0x1  | 100 |
| 0x2  | 200 |
| 0x3  | 400 (recommended only with max Rc of 1kohm) |

**SCD_T2:0 (Bits 2-0)**: Short circuit in discharge threshold setting.

| Code | RSNS=1 (mV) | RSNS=0 (mV) |
|------|-------------|-------------|
| 0x0  | 44 | 22 |
| 0x1  | 67 | 33 |
| 0x2  | 89 | 44 |
| 0x3  | 111 | 56 |
| 0x4  | 133 | 67 |
| 0x5  | 155 | 78 |
| 0x6  | 178 | 89 |
| 0x7  | 200 | 100 |

---

## PROTECT2 (0x07) — Protection Configuration 2 (OCD)

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | OCD_D2 | OCD_D1 | OCD_D0 | OCD_T3 | OCD_T2 | OCD_T1 | OCD_T0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | RW | RW | RW | RW | RW | RW | RW |

**OCD_D2:0 (Bits 6-4)**: Overcurrent in discharge delay setting.

| Code | Delay (ms) |
|------|-----------|
| 0x0  | 8 |
| 0x1  | 20 |
| 0x2  | 40 |
| 0x3  | 80 |
| 0x4  | 160 |
| 0x5  | 320 |
| 0x6  | 640 |
| 0x7  | 1280 |

**OCD_T3:0 (Bits 3-0)**: Overcurrent in discharge threshold setting.

| Code | RSNS=1 (mV) | RSNS=0 (mV) |
|------|-------------|-------------|
| 0x0  | 17 | 8 |
| 0x1  | 22 | 11 |
| 0x2  | 28 | 14 |
| 0x3  | 33 | 17 |
| 0x4  | 39 | 19 |
| 0x5  | 44 | 22 |
| 0x6  | 50 | 25 |
| 0x7  | 56 | 28 |
| 0x8  | 61 | 31 |
| 0x9  | 67 | 33 |
| 0xA  | 72 | 36 |
| 0xB  | 78 | 39 |
| 0xC  | 83 | 42 |
| 0xD  | 89 | 44 |
| 0xE  | 94 | 47 |
| 0xF  | 100 | 50 |

---

## PROTECT3 (0x08) — Protection Configuration 3 (UV/OV Delays)

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | UV_D1 | UV_D0 | OV_D1 | OV_D0 | RSVD | RSVD | RSVD | RSVD |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | RW | RW | RW | RW | RW | RW | RW | RW |

**UV_D1:0 (Bits 7-6)**: Undervoltage delay setting.

| Code | Delay (s) |
|------|-----------|
| 0x0  | 1 |
| 0x1  | 4 |
| 0x2  | 8 |
| 0x3  | 16 |

**OV_D1:0 (Bits 5-4)**: Overvoltage delay setting.

| Code | Delay (s) |
|------|-----------|
| 0x0  | 1 |
| 0x1  | 2 |
| 0x2  | 4 |
| 0x3  | 8 |

**RSVD (Bits 3-0)**: TI internal debug use only. Must be configured to default settings.

---

## OV_TRIP (0x09) — Overvoltage Trip Threshold

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | OV_T7 | OV_T6 | OV_T5 | OV_T4 | OV_T3 | OV_T2 | OV_T1 | OV_T0 |
| RESET | 1 | 0 | 1 | 0 | 1 | 1 | 0 | 0 |
| ACCESS | RW | RW | RW | RW | RW | RW | RW | RW |

**OV_T7:0 (Bits 7-0)**: Middle 8 bits of the 14-bit ADC mapping for OV protection threshold.
- Upper 2 MSB preset to "10"
- Lower 4 LSB preset to "1000"
- Full mapping: 10-OV_T<7:0>-1000
- Default: 0xAC
- Programmable range: ~3.15V to ~4.7V (subject to GAIN/OFFSET variation)

> To convert desired OV voltage to register value:
> 1. OV_TRIP_FULL = (OV_voltage - ADCOFFSET) / ADCGAIN
> 2. Extract bits 11:4 (shift right 4 bits, mask upper 2 bits)

---

## UV_TRIP (0x0A) — Undervoltage Trip Threshold

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | UV_T7 | UV_T6 | UV_T5 | UV_T4 | UV_T3 | UV_T2 | UV_T1 | UV_T0 |
| RESET | 1 | 0 | 0 | 1 | 0 | 1 | 1 | 1 |
| ACCESS | RW | RW | RW | RW | RW | RW | RW | RW |

**UV_T7:0 (Bits 7-0)**: Middle 8 bits of the 14-bit ADC mapping for UV protection threshold.
- Upper 2 MSB preset to "01"
- Lower 4 LSB preset to "0000"
- Full mapping: 01-UV_T<7:0>-0000
- Default: 0x97
- Programmable range: ~1.58V to ~3.1V (subject to GAIN/OFFSET variation)

> To convert desired UV voltage to register value:
> 1. UV_TRIP_FULL = (UV_voltage - ADCOFFSET) / ADCGAIN
> 2. Extract bits 11:4 (shift right 4 bits, mask upper 2 bits)

---

## CC_CFG (0x0B) — Coulomb Counter Configuration

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | CC_CFG5 | CC_CFG4 | CC_CFG3 | CC_CFG2 | CC_CFG1 | CC_CFG0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | RW | RW | RW | RW | RW | RW |

**CC_CFG5:0 (Bits 5-0)**: For optimal performance, program to 0x19 upon device startup.

---

## Cell Voltage Registers (Read-Only)

Format for VC1_HI/LO through VC15_HI/LO.

HI register (0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28):

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | D13 | D12 | D11 | D10 | D9 | D8 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | R | R | R | R | R | R |

LO register (0x0D, 0x0F, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x29):

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| ACCESS | R | R | R | R | R | R | R | R |

**D13:0**: 14-bit cell voltage ADC reading (2's complement format for signed values, straight binary for unsigned).
- Conversion: V(cell) = GAIN x ADC(cell) + OFFSET
- GAIN in uV/LSB, OFFSET in mV
- Always returned as atomic value if both HI and LO read in same transaction (auto-increment)

---

## BAT_HI (0x2A) and BAT_LO (0x2B) — Pack Voltage

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | D15 | D14 | D13 | D12 | D11 | D10 | D9 | D8 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

**D15:0**: Sum of all cell voltages divided by 4. Nominal LSB of 1.532 mV.
- Conversion: V(BAT) = 4 x GAIN x ADC(cell) + (#Cells x OFFSET)

---

## TS1_HI (0x2C) and TS1_LO (0x2D) — Temperature Sensor 1

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | D13 | D12 | D11 | D10 | D9 | D8 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

**D13:0**: TS1 thermistor or DIETEMP ADC reading (depending on TEMP_SEL).

---

## TS2_HI (0x2E) and TS2_LO (0x2F) — Temperature Sensor 2

Same format as TS1. Valid for BQ76930 and BQ76940 only.

---

## TS3_HI (0x30) and TS3_LO (0x31) — Temperature Sensor 3

Same format as TS1. Valid for BQ76940 only.

---

## CC_HI (0x32) and CC_LO (0x33) — Coulomb Counter

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | CC15 | CC14 | CC13 | CC12 | CC11 | CC10 | CC9 | CC8 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | CC7 | CC6 | CC5 | CC4 | CC3 | CC2 | CC1 | CC0 |
| RESET | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

**CC15:0**: 16-bit 2's complement coulomb counter reading.
- Conversion: CC(uV) = [16-bit 2's Complement Value] x (8.44 uV/LSB)

---

## ADCGAIN1 (0x50) — ADC Gain Upper Bits (Read-Only)

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | — | — | — | — | ADCGAIN4 | ADCGAIN3 | — | — |
| RESET | — | — | — | — | — | — | — | — |
| ACCESS | R | R | R | R | R | R | R | R |

---

## ADCOFFSET (0x51) — ADC Offset (Read-Only)

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | ADCOFFSET7 | ADCOFFSET6 | ADCOFFSET5 | ADCOFFSET4 | ADCOFFSET3 | ADCOFFSET2 | ADCOFFSET1 | ADCOFFSET0 |
| RESET | — | — | — | — | — | — | — | — |
| ACCESS | R | R | R | R | R | R | R | R |

**ADCOFFSET7:0**: 2's complement format in mV units. Range: -128 mV to +127 mV, 1 mV LSB.

| ADCOFFSET | Offset (mV) |
|-----------|-------------|
| 0x00 | 0 |
| 0x01 | 1 |
| 0x7F | 127 |
| 0x80 | -128 |
| 0x81 | -127 |
| 0xFF | -1 |

---

## ADCGAIN2 (0x59) — ADC Gain Lower Bits (Read-Only)

| BIT | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| NAME | ADCGAIN2 | ADCGAIN1 | ADCGAIN0 | — | — | — | — | — |
| RESET | — | — | — | — | — | — | — | — |
| ACCESS | R | R | R | R | R | R | R | R |

**ADCGAIN<4:0>**: Production-trimmed value for ADC transfer function, in uV/LSB.
- Range: 365 uV/LSB to 396 uV/LSB
- Steps: 1 uV/LSB
- Formula: GAIN = 365 uV/LSB + (ADCGAIN<4:0> in decimal) x (1 uV/LSB)

| ADCGAIN  | Gain (uV/LSB) | ADCGAIN | Gain (uV/LSB) |
|----------|---------------|---------|---------------|
| 0x00 | 365 | 0x10 | 381 |
| 0x01 | 366 | 0x11 | 382 |
| 0x02 | 367 | 0x12 | 383 |
| 0x03 | 368 | 0x13 | 384 |
| 0x04 | 369 | 0x14 | 385 |
| 0x05 | 370 | 0x15 | 386 |
| 0x06 | 371 | 0x16 | 387 |
| 0x07 | 372 | 0x17 | 388 |
| 0x08 | 373 | 0x18 | 389 |
| 0x09 | 374 | 0x19 | 390 |
| 0x0A | 375 | 0x1A | 391 |
| 0x0B | 376 | 0x1B | 392 |
| 0x0C | 377 | 0x1C | 393 |
| 0x0D | 378 | 0x1D | 394 |
| 0x0E | 379 | 0x1E | 395 |
| 0x0F | 380 | 0x1F | 396 |

(All information sourced from datasheet Section 8.5 - Register Maps and Section 8.5.1 - Register Details)
