# bq76940 AC / Timing Parameters

> **PURPOSE:** I2C timing, ADC conversion times, protection delay times, and all other timing characteristics.

## Validation Rules

Every timing parameter is validated against the datasheet minimum and maximum:
- `t_actual >= t_min` -> COMPLIANT
- `t_actual = t_min` -> BORDERLINE
- `t_actual < t_min` -> VIOLATION

---

## I2C Compatible Interface Timing

All I2C timing is for standard 100-kHz operation.

| Parameter | Description | Min | Typ | Max | Unit |
|-----------|-------------|-----|-----|-----|------|
| V_IL | Input low logic threshold | | | REGOUT x 0.25 | V |
| V_IH | Input high logic threshold | REGOUT x 0.75 | | | V |
| V_OL | Output low logic drive | | 0.20 | | V |
| t_f | SCL, SDA fall time | | | 0.40 | us |
| t_HIGH | SCL pulse width high | 4.0 | | | us |
| t_LOW | SCL pulse width low | 4.7 | | | us |
| t_SU;STA | Setup time for START condition | 4.7 | | | us |
| t_HD;STA | START condition hold time after which first clock pulse is generated | 4.0 | | | us |
| t_SU;DAT | Data setup time | 250 | | | ns |
| t_HD;DAT | Data hold time | 0 | | | us |
| t_SU;STO | Setup time for STOP condition | 4.0 | | | us |
| t_BUF | Time the bus must be free before new transmission can start | 4.7 | | | us |
| t_VD;DAT | Clock low to data out valid | | | 900 | ns |
| t_HD;DAT | Data out hold time after clock low | 0 | | | ns |
| f_SCL | Clock frequency | 0 | | 100 | kHz |

---

## Measurement Schedule Timing

| Parameter | Description | Min | Typ | Max | Unit |
|-----------|-------------|-----|-----|-----|------|
| t_VCELL | Cell voltage measurement interval | | 250 | | ms |
| t_INDCELL | Individual cell measurement time (balancing off) | | 50 | | ms |
| t_INDCELL | Individual cell measurement time (balancing on) | | 12.5 | | ms |
| t_CB_RELAX | Cell balancing relaxation time before cell voltage measured | | 12.5 | | ms |
| t_TEMP_DEC | Temperature measurement decimation time | | 12.5 | | ms |
| t_BAT | Pack voltage calculation interval | | 250 | | ms |
| t_TEMP | Temperature measurement interval (TS1/TS2/TS3 or die temp) | | 2 | | s |
| t_CC_READ | CC conversion time (single conversion) | | 250 | | ms |
| Device timeline accuracy | | | 3.5% | | |

---

## Power-On / Startup Timing

| Parameter | Description | Min | Typ | Max | Unit |
|-----------|-------------|-----|-----|-----|------|
| t_I2CSTARTUP | Delay after boot signal on TS1 before I2C communication allowed | | 1 | | ms |
| t_BOOTREADY | Delay after boot signal when device completes full boot-up | | 10 | | ms |
| NORMAL mode from SHIP (BQ76920) | Time before reading initial cell voltage | | 250 | | ms |
| NORMAL mode from SHIP (BQ76930) | Time before reading initial cell voltage | | 400 | | ms |
| NORMAL mode from SHIP (BQ76940) | Time before reading initial cell voltage | | 800 | | ms |
| t_BOOT_max | Boot threshold application time (at TS1 pin) | 10 | | 2000 | us |

---

## Protection Delay Timing

### Overvoltage (OV) Delays

| Code | Actual Delay | Min | Typ | Max | Unit |
|------|-------------|-----|-----|-----|------|
| 0x0 | 1 s | 0.7 | 1 | 1.75 | s |
| 0x1 | 2 s | 1.6 | 2 | 2.75 | s |
| 0x2 | 4 s | 3.5 | 4 | 5 | s |
| 0x3 | 8 s | 7 | 8 | 10 | s |

### Undervoltage (UV) Delays

| Code | Actual Delay | Min | Typ | Max | Unit |
|------|-------------|-----|-----|-----|------|
| 0x0 | 1 s | 0.7 | 1 | 1.75 | s |
| 0x1 | 4 s | 3.5 | 4 | 5 | s |
| 0x2 | 8 s | 7 | 8 | 10 | s |
| 0x3 | 16 s | 14 | 16 | 20 | s |

### Overcurrent in Discharge (OCD) Delays

| Code | Delay (ms) |
|------|-----------|
| 0x0 | 8 |
| 0x1 | 20 |
| 0x2 | 40 |
| 0x3 | 80 |
| 0x4 | 160 |
| 0x5 | 320 |
| 0x6 | 640 |
| 0x7 | 1280 |

Delay accuracy (t_PROTACC): -20% to +20%

### Short Circuit in Discharge (SCD) Delays

| Code | Delay (us) |
|------|-----------|
| 0x0 | 70 |
| 0x1 | 100 |
| 0x2 | 200 |
| 0x3 | 400 |

### Reduced Test Time

When [DELAY_DIS] = 1 in SYS_CTRL2:
- All OV, UV, OCD, and SCD delays are bypassed
- Fault condition registers within ~200 ms

---

## FET Driver Timing

| Parameter | Description | Min | Typ | Max | Unit |
|-----------|-------------|-----|-----|-----|------|
| t_FET_ON | CHG/DSG ON rise time (10nF load, 10% to 90%) | | 200 | 250 | us |
| t_DSG_OFF | DSG pull-down OFF fall time (10nF load, 90% to 10%) | | 60 | 90 | us |

---

## ADC and CC Timing

| Parameter | Description | Value | Unit |
|-----------|-------------|-------|------|
| 14-bit ADC LSB | | 382 | uV |
| 16-bit CC LSB | CC running constantly | 8.44 | uV |
| CC conversion time | Single conversion | 250 | ms |
| CC integration period | Always-on or 1-shot | 250 | ms |

---

## ALERT Pin Timing

| Parameter | Description | Min | Typ | Max | Unit |
|-----------|-------------|-----|-----|-----|------|
| ALERT output high | I=1mA | REGOUT x 0.75 | | | V |
| ALERT output low | Unloaded | | | REGOUT x 0.25 | V |
| ALERT input high threshold | ALERT externally forced high | 1 | | 1.5 | V |
| ALERT pulldown resistance | When driven low | 0.8 | 2.5 | 8 | Mohm |
| ALERT RC time constant (suggested) | For noise filtering | | 250 | | us |
| ALERT pulldown resistor (recommended) | External | | 500k-1M | | ohm |
| ALERT filter capacitor (suggested) | | | 470 | | pF |

---

## Boot Detector Timing

| Parameter | Description | Min | Typ | Max | Unit |
|-----------|-------------|-----|-----|-----|------|
| Boot threshold voltage | Measured at TS1 in SHIP mode | 300 | | 1000 | mV |
| Boot application time | At TS1 pin | 10 | | 2000 | us |

---

## Cell Balancing Timing

| Parameter | Description | Value |
|-----------|-------------|-------|
| Cell balancing duty cycle (when enabled) | Every 250 ms | 70% |
| Cell balancing relaxation time | Before cell voltage measurement | 12.5 ms |

---

## Thermistor Bias Timing

| Parameter | Description | Value |
|-----------|-------------|-------|
| Thermistor bias warm-up time | Before measurement begins | 37.5 ms |
| TS capacitor (recommended, EVM value) | Across thermistor | 4.7 nF or smaller |

(All information sourced from datasheet Sections 7.6, 7.5, 8.3.1.4, and 9.1)
