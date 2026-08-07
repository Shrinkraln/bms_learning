# bq76940 Operation Sequences & State Machines

> **PURPOSE:** Canonical sequences for initialization, boot, SHIP mode entry/exit, cell balancing, and protection recovery.

## Sequence Format

```
Step 1: Send I2C command 0x?? -> Device responds with [expected]
Step 2: Configure register 0x?? to value 0x??
Step N: ...
```

Use these sequences verbatim when generating driver code. Do NOT reorder or omit steps; most chips require exact ordering for correct operation.

---

## Power Modes

| Mode   | Description |
| ------ | ----------- |
| NORMAL | Fully operational. ADC and CC may be on/off. OV/UV enabled if ADC on. OCD and SCD always enabled. |
| SHIP   | Lowest power state. Must see BOOT signal (>1V) on TS1 to exit. Device always enters SHIP upon POR. |

---

## 1. Boot Sequence (SHIP -> NORMAL)

The device enters SHIP mode automatically upon POR. To transition to NORMAL mode:

```
Step 1: Apply a rising edge (voltage > V_BOOT threshold) to the TS1 pin
        - V_BOOT min: 300 mV, max: 1000 mV
        - Pulse duration (t_BOOT_max): 10 us to 2000 us

Step 2: Wait for t_BOOTREADY (10 ms typical) - device completes full boot-up

Step 3: Wait for t_I2CSTARTUP (1 ms typical) - I2C communication allowed

Step 4: (Optional but recommended) Program CC_CFG (0x0B) to 0x19 for optimal performance

Step 5: Configure protection thresholds (OV_TRIP, UV_TRIP, PROTECT1, PROTECT2, PROTECT3)

Step 6: Configure ADC_EN bit in SYS_CTRL1 to enable ADC if OV/UV protection needed

Step 7: Enable FETs by setting DSG_ON and/or CHG_ON bits in SYS_CTRL2 as needed

Step 8: Wait for initial voltage measurements to stabilize before reading:
        - BQ76920: 250 ms
        - BQ76930: 400 ms
        - BQ76940: 800 ms
```

---

## 2. SHIP Mode Entry (NORMAL -> SHIP)

To put the device into its lowest power state:

```
Prerequisites: SHUT_A and SHUT_B bits in SYS_CTRL1 (0x04) must both be 0

Write #1 to SYS_CTRL1 (0x04):
  - SHUT_A = 0
  - SHUT_B = 1
  (Other bits unchanged)

Write #2 to SYS_CTRL1 (0x04):
  - SHUT_A = 1
  - SHUT_B = 0
  (Other bits unchanged)

If the sequence is entered correctly, the device transitions into SHIP mode.
Any other sequence is ignored.

Warning: DO NOT operate the device below POR voltage. If BAT-VC10x, VC10x-VC5x,
or VC5x-VSS falls below V_SHUT (3.6V typical), a full device reset is required:
power down all three intermediate voltages below V_SHUT and reboot via TS1.
```

---

## 3. Initialization Sequence (After Boot)

Complete initialization after booting the device:

```
Step 1: Read ADCGAIN1 (0x50), ADCGAIN2 (0x59), and ADCOFFSET (0x51)
        to obtain factory-calibrated GAIN and OFFSET values

Step 2: Write CC_CFG (0x0B) = 0x19  // Optimal CC performance

Step 3: Write OV_TRIP (0x09) = OV_THRESH  // Set overvoltage threshold
        // OV threshold mapping: 10-OV_T<7:0>-1000
        // OV_TRIP_FULL = (OV_voltage - ADCOFFSET) / ADCGAIN
        // Extract middle 8 bits: OV_T = (OV_TRIP_FULL >> 4) & 0xFF

Step 4: Write UV_TRIP (0x0A) = UV_THRESH  // Set undervoltage threshold
        // UV threshold mapping: 01-UV_T<7:0>-0000
        // UV_TRIP_FULL = (UV_voltage - ADCOFFSET) / ADCGAIN
        // Extract middle 8 bits: UV_T = (UV_TRIP_FULL >> 4) & 0xFF

Step 5: Write PROTECT1 (0x06) = value  // Configure SCD and RSNS

Step 6: Write PROTECT2 (0x07) = value  // Configure OCD

Step 7: Write PROTECT3 (0x08) = value  // Configure OV and UV delays

Step 8: Write SYS_CTRL1 (0x04) with ADC_EN = 1  // Enable ADC for OV/UV protection

Step 9: Write SYS_CTRL2 (0x05) with DSG_ON and/or CHG_ON as needed
```

**Example initialization for a typical BQ76930 8-cell configuration:**
```
// Sense resistor: 5 mohm
// OV threshold: 4.30 V, OV delay: 2 s
// UV threshold: 2.5 V, UV delay: 4 s
// SCD threshold: 125 mV (use 111mV setting), SCD delay: 100 us
// OCD threshold: 75 mV (use 72mV setting), OCD delay: 320 ms

Write CC_CFG (0x0B) = 0x19
Write OV_TRIP (0x09) = 0xBF     // For GAIN=382, OFFSET=0, OV=4.30V
Write UV_TRIP (0x0A) = 0x99     // For GAIN=382, OFFSET=0, UV=2.5V
Write PROTECT1 (0x06) = 0x8B    // RSNS=1, SCD_DELAY=100us, SCD_THRESH=111mV
Write PROTECT2 (0x07) = 0x5A    // OCD_DELAY=320ms, OCD_THRESH=72mV (RSNS=1)
Write PROTECT3 (0x08) = 0x50    // UV_DELAY=4s, OV_DELAY=2s
Write SYS_CTRL1 (0x04) |= 0x10 // ADC_EN=1
Write SYS_CTRL2 (0x05) = 0x03  // DSG_ON=1, CHG_ON=1
```

---

## 4. Cell Voltage Reading Sequence

```
Step 1: Write the starting register address (e.g., 0x0C for VC1_HI)
Step 2: Repeated Start
Step 3: Read the HI byte (address auto-increments)
Step 4: Read the LO byte (address auto-increments)
Step 5: Continue reading subsequent cell registers if needed
Step 6: Convert 14-bit ADC reading:
        V(cell) = GAIN x ADC(cell) + OFFSET
        where GAIN = 365 + ADCGAIN<4:0> (uV/LSB)
        and OFFSET = ADCOFFSET<7:0> as 2's complement (mV)
```

**Atomic read example (VC1-VC5):**
```
Write 0x0C (VC1_HI address)
START, send 0x0C, STOP
START, read VC1_HI, ACK
        read VC1_LO, ACK
        read VC2_HI, ACK
        read VC2_LO, ACK
        read VC3_HI, ACK
        read VC3_LO, ACK
        read VC4_HI, ACK
        read VC4_LO, ACK
        read VC5_HI, ACK
        read VC5_LO, NACK
STOP
```

---

## 5. Coulomb Counter Operation

### One-Shot Mode

```
Step 1: Read CC_HI (0x32) and CC_LO (0x33) to clear any prior reading
Step 2: Write SYS_CTRL2 (0x05) with CC_ONESHOT=1, CC_EN=0
Step 3: Wait 250 ms for conversion
Step 4: Read CC_HI (0x32) and CC_LO (0x33) atomically
Step 5: Convert: CC(uV) = [16-bit 2's complement] x 8.44 uV/LSB
Step 6: Calculate current: I = CC(uV) / R_sense(ohm) in uA
```

### Continuous Mode

```
Step 1: Write SYS_CTRL2 (0x05) with CC_EN=1
Step 2: CC runs continuously, updating every 250 ms
Step 3: ALERT pin toggles high when CC_READY bit is set
Step 4: On ALERT interrupt:
  a. Read CC_HI and CC_LO atomically
  b. Clear CC_READY bit by writing 1 to bit 7 of SYS_STAT (0x00)
Step 5: CC reading available every 250 ms
```

---

## 6. Cell Balancing Sequence

```
Step 1: Ensure ADC is enabled (ADC_EN=1 in SYS_CTRL1)
Step 2: Write CELLBAL1 (0x01), CELLBAL2 (0x02), and/or CELLBAL3 (0x03)
        with appropriate bits set for cells to balance:
        - Bit (x-1) = 1 enables balancing for cell x
        - Example: CELLBAL1 = 0x01 enables CB1 (cell 1)
Step 3: Balancing runs with ~70% duty cycle per 250 ms
        (balancing suspended briefly during ADC measurement)

IMPORTANT RULES:
- Adjacent cells must NOT be balanced simultaneously within:
  - VC1-VC5 group
  - VC6-VC10 group
  - VC11-VC15 group
- Cell balancing bits are automatically cleared on:
  - DEVICE_XREADY event
  - Entry into NORMAL from SHIP mode
- Internal balancing (BQ76930/40): max 5 mA per cell
- External balancing: recommended for BQ76930 and BQ76940
```

---

## 7. Protection Fault Recovery

When a protection fault occurs:
- Affected FET(s) are disabled (see table below)
- ALERT pin is driven high
- SYS_STAT register bit is set

| Fault  | CHG_ON | DSG_ON |
|--------|--------|--------|
| OV     | Set to 0 | —    |
| UV     | —      | Set to 0 |
| OCD    | —      | Set to 0 |
| SCD    | —      | Set to 0 |
| OVRD_ALERT | Set to 0 | Set to 0 |
| DEVICE_XREADY | Set to 0 | Set to 0 |
| Enter SHIP from NORMAL | Set to 0 | Set to 0 |

**Recovery sequence:**
```
Step 1: Read SYS_STAT (0x00) to identify the fault source
Step 2: Clear the fault bit by writing "1" to that bit position in SYS_STAT
        (ALERT pin clears automatically once all bits are cleared)
Step 3: Re-enable FETs by writing DSG_ON and/or CHG_ON in SYS_CTRL2
Step 4: For OCD/SCD recovery:
        - Ensure load is removed (check LOAD_PRESENT bit)
        - After load removal, LOAD_PRESENT reads 0
Step 5: For OV/UV recovery:
        - The fault condition must no longer be present
        - Otherwise the fault immediately retoggles
```

> Note: The host microcontroller must initiate ALL protection recovery. The
> BQ769x0 never automatically sets CHG_ON or DSG_ON to 1.

---

## 8. Front FET Driver Operations

```
Enable DSG:  Write SYS_CTRL2 (0x05) with DSG_ON = 1
Disable DSG:  Write SYS_CTRL2 (0x05) with DSG_ON = 0 (or fault event clears it)

Enable CHG:  Write SYS_CTRL2 (0x05) with CHG_ON = 1
Disable CHG:  Write SYS_CTRL2 (0x05) with CHG_ON = 0 (or fault/event clears it)
```

**Load detection** (when CHG is disabled):
- Read LOAD_PRESENT bit (Bit 7 of SYS_CTRL1, 0x04)
- 1 = CHG pin externally pulled high (load present)
- 0 = No load or CHG_ON=1

---

## 9. Temperature Measurement

### External Thermistor Mode (TEMP_SEL = 1)

```
Step 1: Set TEMP_SEL = 1 in SYS_CTRL1 (0x04)
Step 2: Wait 2 seconds for scheduler update
Step 3: Read TS1_HI (0x2C) and TS1_LO (0x2D)
        (also TS2 for BQ76930/40, TS3 for BQ76940)
Step 4: Convert:
        V_TSX = (ADC in Decimal) x 382 uV/LSB
        R_TS = (10,000 x V_TSX) / (3.3 - V_TSX)
Step 5: Consult thermistor datasheet for temperature from resistance
```

### Internal Die Temperature Mode (TEMP_SEL = 0)

```
Step 1: Set TEMP_SEL = 0 in SYS_CTRL1 (0x04)
Step 2: Wait 2 seconds for scheduler update
Step 3: Read TS1_HI (0x2C) and TS1_LO (0x2D)
        (also TS2, TS3 for multi-measurement)
Step 4: Convert:
        V_25 = 1.200 V (nominal)
        V_TSX = (ADC in Decimal) x 382 uV/LSB
        TEMP_DIE = 25 - ((V_TSX - V_25) / 0.0042)
```

---

## 10. External Real-Time Calibration Sequence

For systems requiring higher accuracy:

```
Step 1: Measure V_STACK externally:
        V_STACK = V_AD x (R1 + R2) / R1

Step 2: Read all cell ADC values from BQ769x0:
        V(n) = GAIN x ADC_n + OFFSET

Step 3: Sum all cell voltages: V_SUM = V(1) + V(2) + ... + V(N)

Step 4: Calculate GAIN2: GAIN2 = V_STACK / V_SUM

Step 5: Apply to each cell: V(cell) = GAIN2 x (GAIN x ADC(cell) + OFFSET)
```

> A new GAIN2 should be generated when cell voltages change by more than 100 mV.
> For systems not needing this calibration, GAIN2 = 1.

---

## System Event -> CHG/DSG Response

| EVENT                       | [CHG_ON] | [DSG_ON] |
|-----------------------------|----------|----------|
| OV Fault                    | Set to 0 | —        |
| UV Fault                    | —        | Set to 0 |
| OCD Fault                   | —        | Set to 0 |
| SCD Fault                   | —        | Set to 0 |
| ALERT Override (OVRD_ALERT) | Set to 0 | Set to 0 |
| DEVICE_XREADY set           | Set to 0 | Set to 0 |
| Enter SHIP from NORMAL      | Set to 0 | Set to 0 |

There are NO conditions under which the BQ769x0 automatically sets CHG_ON or DSG_ON to 1.

---

## Cell Balancing Auto-Clear Events

All cell balancing control bits in CELLBAL1, CELLBAL2, and CELLBAL3 are automatically cleared under:
1. DEVICE_XREADY is set
2. Device enters NORMAL mode from SHIP mode

The host microcontroller must explicitly rewrite the balancing bits after these events.

(All sequences sourced from datasheet Sections 8.3.1, 8.4, 9.1, and 9.2)
