# bq76940 Electrical Characteristics

> **PURPOSE:** Absolute maximum ratings, DC characteristics, and operating conditions — used as **hard constraints** during code validation.

## Using This File for Validation

Every parameter below is a **hard boundary**. When validating generated or user-written driver code:
- If a configured voltage/current exceeds the max -> VIOLATION
- If a configured value is exactly at a boundary -> BORDERLINE
- If within range -> COMPLIANT

---

## Absolute Maximum Ratings

Over-operating free-air temperature range (unless otherwise noted).

| Parameter | Condition | Min | Max | Unit |
|-----------|-----------|-----|-----|------|
| Supply voltage BAT (BAT-VSS) | BQ76940 | -0.3 | 36 | V |
| Supply voltage (BAT-VC10x), (VC10x-VC5x), (VC5x-VSS) | BQ76940 | | | |
| Input voltage (VCn-VSS) n=1..5 | BQ76920, BQ76930, BQ76940 | -0.3 | n x 7.2 | V |
| Input voltage (VCn-VC5x) n=6..10 | BQ76930, BQ76940 | | (n-5) x 7.2 | V |
| Input voltage (VCn-VC10x) n=11..15 | BQ76940 | | (n-10) x 7.2 | V |
| Cell input differential (VCn-VCn-1) n=1..15 | All | -0.3 | 9 | V |
| SRN, SRP, SCL, SDA | All | -0.3 | 3.6 | V |
| REGSRC | All | -0.3 | 36 | V |
| REGOUT, ALERT | All | -0.3 | 3.6 | V |
| DSG | All | -0.3 | 20 | V |
| CHG | All | -0.3 | VCHGCLAMP | V |
| Cell balancing current (per cell) CB | BQ76920 | | 70 | mA |
| | BQ76930, BQ76940 | | 5 | mA |
| Discharge pin input current when disabled DSG | All | | 7 | mA |
| Storage temperature T_STG | All | -65 | 150 | degC |
| Lead temperature (soldering, 10 s) | All | | 300 | degC |

## ESD Ratings

| Parameter | Condition | Value | Unit |
|-----------|-----------|-------|------|
| HBM ESD stress voltage | JEDEC JEP155 | +/-2 | kV |
| CDM ESD stress voltage | JEDEC JEP157 | +/-500 | V |

## Recommended Operating Conditions

Over-operating free-air temperature range (unless otherwise noted).

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Supply voltage BAT (BAT-VSS) | BQ76920 | 6 | | 25 | V |
| Supply voltage (BAT-VC5x), (VC5x-VSS) | BQ76930 | | | | V |
| Supply voltage (BAT-VC10x), (VC10x-VC5x), (VC5x-VSS) | BQ76940 | | | | V |
| Cell input differential (VCn-VCn-1) | In-use cells only | 2 | | 5 | V |
| SRP | | -10 | | 10 | mV |
| SRN | | -200 | | 200 | mV |
| SCL, SDA | | 0 | | 3.6 | V |
| REGSRC | | 6 | | 25 | V |
| CHG, DSG | | 0 | | 16 | V |
| REGOUT, ALERT | | 0 | | 3.6 | V |
| Cell balancing current (internal, per cell) | BQ76920 | 0 | | 50 | mA |
| | BQ76930, BQ76940 | 0 | | 5 | mA |
| External cell input Rc resistance | BQ76920 | 40 | 100 | 1K | ohm |
| | BQ76930, BQ76940 | 500 | 1K | 1K | ohm |
| External cell input capacitance Cc | All | 0.1 | 1 | 10 | uF |
| External supply filter resistance Rf | All | 40 | 100 | 1K | ohm |
| External supply filter capacitance Cf | All | 1 | 10 | 40 | uF |
| Sense resistor filter resistance R_FILT | All | | 100 | 1K | ohm |
| ALERT pin to VSS resistor R_ALERT | All | | 1M | | ohm |
| REGOUT loading capacitance C_L | All | 1 | | 4.7 | uF |
| REGSRC, CAP1, CAP2, CAP3 output capacitance C_CAP | All | | 1 | | uF |
| External thermistor nominal resistance (103AT) at 25C R_TS | All | | 10K | | ohm |
| Operating free-air temperature T_OPR | All | -40 | | 85 | degC |

## Thermal Information

| Thermal Metric | BQ76920 20-PIN (PW) | BQ76930 30-PIN (DBT) | BQ76940 44-PIN (DBT) | Unit |
|----------------|---------------------|----------------------|----------------------|------|
| R_thetaJA (High K) | 93.7 | 86.5 | 70.1 | degC/W |
| R_thetaJC(top) | 28.7 | 19.4 | 17.5 | degC/W |
| R_thetaJB | 44.6 | 41.3 | 33.9 | degC/W |
| psi_JT | 1.3 | 0.5 | 0.5 | degC/W |
| psi_JB | 44.1 | 40.6 | 33.4 | degC/W |
| R_thetaJC(bottom) | n/a | n/a | n/a | degC/W |

## Electrical Characteristics

Typical conditions: 25 degC, nominal BAT voltages (BQ76940 = 48 V), V_CELL = 4 V. Min/max over -40 to +85 degC.

### Supply Currents

| Parameter | Test Condition | Min | Typ | Max | Unit |
|-----------|---------------|-----|-----|-----|------|
| I_DD (NORMAL mode) | ADC off, CC off | | 40 | 60 | uA |
| | ADC on, CC off | | 60 | 90 | uA |
| | ADC off, CC on | | 110 | 165 | uA |
| | ADC on, CC on | | 130 | 195 | uA |
| I_CC_BAT (NORMAL) | ADC off | | 30 | 45 | uA |
| | ADC on | | 50 | 75 | uA |
| I_CC_REGSRC (NORMAL) | CC off | | 10 | 15 | uA |
| | CC on | | 80 | 120 | uA |
| I_SHIP (SHIP/SHUTDOWN mode) | Full shutdown | | 0.6 | 1.8 | uA |

### Leakage and Offset Currents

| Parameter | Test Condition | Min | Typ | Max | Unit |
|-----------|---------------|-----|-----|-----|------|
| NORMAL mode supply dI_NOM current offset | Measured into VC5x and VC10x | -5 | +/-2.5 | 5 | uA |
| SHIP mode supply current dI_SHIP offset | | -1.0 | +/-0.1 | 1.0 | uA |
| Supply current when ALERT active dI_ALERT | Measured into VC5x or added to BAT | | 15 | 25 | uA |
| Cell measurement input dI_CELL | Measured into VC0-VC15 except VC5,VC10,VC15 | -0.3 | +/-0.1 | 0.3 | uA |
| | Measured into VC5, VC10, VC15 | | 0.5 | | uA |
| Terminal input leakage I_LKG | | | | 1 | uA |

### Internal Power Control (Startup and Shutdown)

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Analog POR threshold V_PORA | V rising, BAT | | 4 | 5 | V |
| Shutdown voltage V_SHUT | V falling, BAT | | 3.6 | | V |
| Time delay after boot signal on TS1 before I2C communications allowed t_I2CSTARTUP | | | 1 | | ms |
| Device boot startup delay t_BOOTREADY | Delay after boot signal when device has completed full boot-up sequence | | 10 | | ms |
| Thermal shutdown temperature T_SHUTD | | | 100 | 150 | degC |

### Measurement Schedule

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Cell voltage measurement interval t_VCELL | BQ76920, BQ76930, BQ76940 | | 250 | | ms |
| Individual cell measurement time t_INDCELL | Per cell, balancing off | | 50 | | ms |
| | Per cell, balancing on | | 12.5 | | ms |
| Cell balancing relaxation time t_CB_RELAX | Before cell voltage measured | | 12.5 | | ms |
| Temperature measurement decimation time t_TEMP_DEC | | | 12.5 | | ms |
| Pack voltage calculation interval t_BAT | | | 250 | | ms |
| Temperature measurement interval t_TEMP | Period for TS1/TS2/TS3 or die temp | | 2 | | s |

### 14-Bit ADC (Cell Voltage and Temperature)

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| ADC measurement range | Cell voltage | 2 | | 5 | V |
| | TS/Temp measurements | 0.3 | | 3 | V |
| ADC LSB value | | | 382 | | uV |
| ADC cell voltage accuracy at 25C | V=3.6V-4.3V | | +/-10 | | mV |
| | V=3.2V-4.6V | | +/-15 | | mV |
| | V=2.0V-5.0V | | +/-25 | | mV |
| ADC cell voltage accuracy 0C to 60C | V=3.6V-4.3V | | +/-20 | | mV |
| | V=3.2V-4.6V | | +/-25 | | mV |
| | V=2.0V-5.0V | | +/-35 | | mV |
| ADC cell voltage accuracy -40C to 85C | V=3.6V-4.3V | -40 | | 40 | mV |
| | V=3.2V-4.6V | -40 | | 40 | mV |
| | V=2.0V-5.0V | -50 | | 50 | mV |

### 16-Bit CC (Pack Current Measurement)

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| CC input voltage range | | -200 | | 200 | mV |
| CC full scale range | | -270 | | 270 | mV |
| CC LSB value | CC running constantly | | 8.44 | | uV |
| CC conversion time t_CC_READ | Single conversion | | 250 | | ms |
| CC integral nonlinearity INL | 16-bit, best fit over +/-200 mV | | +/-2 | +/-40 | LSB |
| CC offset error OFFSET | | | +/-1 | +/-3 | LSB |
| CC gain error GAIN | Over input voltage range | | +/-0.5% | +/-1.5% | FSR |
| CC gain error drift GAINDRIFT | Over input voltage range | | 150 | | ppm/degC |
| CC effective input resistance R_IN | | | 2.5 | | Mohm |

### Thermistor Bias

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Pull-up resistance R_TS | T=25C | 9.85 | 10 | 10.15 | kohm |
| Pull-up resistance drift across temp R_TSDRIFT | T=-40C to 85C | 9.7 | | 10.3 | kohm |

### DIETEMP

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Die temperature voltage V_DIETEMP25 | T=25C | | 1.20 | | V |
| Die temperature voltage drift V_DIETEMPDRIFT | | | -4.2 | | mV/degC |

### Integrated Hardware Protections

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| OV threshold range | ADC code | 0x2008 | | 0x2FF8 | ADC |
| UV threshold range | ADC code | 0x1000 | | 0x1FF0 | ADC |
| OV and UV threshold step size | | | 16 | | LSB |
| UV minimum qualify value UV_MINQUAL | | | 0x0518 | | ADC |
| OV delay = 1s | | 0.7 | 1 | 1.75 | s |
| OV delay = 2s | | 1.6 | 2 | 2.75 | s |
| OV delay = 4s | | 3.5 | 4 | 5 | s |
| OV delay = 8s | | 7 | 8 | 10 | s |
| UV delay = 1s | | 0.7 | 1 | 1.75 | s |
| UV delay = 4s | | 3.5 | 4 | 5 | s |
| UV delay = 8s | | 7 | 8 | 10 | s |
| UV delay = 16s | | 14 | 16 | 20 | s |
| OCD threshold range | Measured across (SRP-SRN) | 8 | | 100 | mV |
| OCD threshold step size (RSNS=0) | | | 2.78 | | mV |
| OCD threshold step size (RSNS=1) | | | 5.56 | | mV |
| OCD delay range | | 8 | | 1280 | ms |
| SCD threshold range | Measured across (SRP-SRN) | 22 | | 200 | mV |
| SCD threshold step size (RSNS=0) | | | 11.1 | | mV |
| SCD threshold step size (RSNS=1) | | | 22.2 | | mV |
| SCD delay options | | 35 | | 520 | us |
| Delay accuracy for OCD/SCD t_PROTACC | | -20% | | 20% | |
| OCD and SCD voltage offset OC_OFFSET | | -2.5 | | 2.5 | mV |
| OCD and SCD scale accuracy OC_SCALEERR | | -10% | | 10% | |

### Charge and Discharge Drivers

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| CHG and DSG ON voltage V_FETON | REGSRC >= 12V, 10M ohm load | 10 | 12 | 14 | V |
| | REGSRC < 12V, 10M ohm load | REGSRC-2 | REGSRC-1 | REGSRC | V |
| CHG and DSG ON rise time t_FET_ON | CHG/DSG driving 10nF, 10% to 90% | | 200 | 250 | us |
| DSG pull-down OFF fall time t_DSG_OFF | DSG driving 10nF, 90% to 10% | | 60 | 90 | us |
| CHG pull-down OFF resistance R_CHG_OFF to VSS | CHG disabled, CHG held at 12V | 750 | 1000 | 1250 | kohm |
| DSG pull-down OFF resistance R_DSG_OFF to VSS | DSG disabled, DSG held at 12V | 1.75 | 2.50 | 4.25 | kohm |
| Load detection threshold V_LOAD_DETECT | | 0.4 | 0.7 | 1.0 | V |
| CHG clamp voltage V_CHG_CLAMP | CHG pin externally pulled high, 500-uA max sink | 18 | 20 | 22 | V |

### ALERT Pin

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| ALERT output voltage high V_ALERT_OH | I=1mA | REGOUT x 0.75 | | | V |
| ALERT output voltage low V_ALERT_OL | Unloaded | | | REGOUT x 0.25 | V |
| ALERT input high V_ALERT_IH | ALERT externally forced high when internally low | 1 | | 1.5 | V |
| ALERT pin weak pulldown R_ALERT_PD when driven low | Measured into ALERT with ALERT=REGOUT | 0.8 | 2.5 | 8 | Mohm |

### Cell Balancing Driver

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Internal cell balancing R_DSFET driver resistance | V_CELL=3.6V | 1 | 5 | 10 | ohm |
| Cell balancing duty cycle X_BAL when enabled | Every 250ms | | 70% | | |

### External Regulator

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| External LDO voltage options | Factory programmed, unloaded | 2.45 | 2.50 | 2.55 | V |
| | | 3.20 | 3.30 | 3.40 | V |
| Line regulation V_EXTLDO_LN | REGSRC 6V to 25V, 10mA load, 100us | | | 100 | mV |
| Load regulation V_EXTLDO_LD | I=0mA to 10mA REGOUT | | -4% | 4% | |
| Ext LDO min voltage under DC load (2.5V version) | REGOUT=10mA DC | | 2.4 | | V |
| | REGOUT=20mA DC | | 2.3 | | V |
| Ext LDO min voltage under DC load (3.3V version) | REGOUT=10mA DC | | 3.15 | | V |
| | REGOUT=20mA DC | | 3.05 | | V |
| External LDO current limit I_EXTLDO_LIMIT | REGOUT=0V | 30 | 38 | 45 | mA |

### Boot Detector

| Parameter | Condition | Min | Typ | Max | Unit |
|-----------|-----------|-----|-----|-----|------|
| Boot threshold voltage V_BOOT | Measured at TS1 pin in SHIP mode | | | 1000 | mV |
| Boot threshold application time t_BOOT_max | Measured at TS1 pin | 10 | | 2000 | us |

(All tables sourced from datasheet Section 7 - Specifications, Section 7.5 Electrical Characteristics)
