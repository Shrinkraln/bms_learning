# bq76940 Pin Definitions

> **PURPOSE:** Every package pin — name, number(s), type, and functional description. Used to cross-check schematic connections and GPIO assignments.

## Pin Type Codes

| Code | Meaning |
|------|---------|
| I    | Digital input |
| O    | Digital output |
| I/O  | Bidirectional |
| P    | Power supply |
| G    | Ground |
| NC   | No connect |
| AI   | Analog input |
| AO   | Analog output |

---

## BQ76940 Pin Diagram (44-TSSOP)

```
DSG     1   44  ALERT
CHG     2   43  SRN
VSS     3   42  SRP
SDA     4   41  VC0
SCL     5   40  VC1
TS1     6   39  VC2
CAP1    7   38  VC3
REGOUT  8   37  VC4
REGSRC  9   36  VC5
VC5x    10  35  VC5B
NC      11  34  VC6
NC      12  33  VC7
TS2     13  32  VC8
CAP2    14  31  VC9
VC10x   15  30  VC10
NC      16  29  VC10B
NC      17  28  VC11
TS3     18  27  VC12
CAP3    19  26  VC13
BAT     20  25  VC14
NC      21  24  VC15
NC      22  23  NC
```

## BQ76940 Pin Functions

| PIN | NAME   | TYPE | DESCRIPTION                              |
| --- | ------ | ---- | ---------------------------------------- |
| 1   | DSG    | O    | Discharge FET driver                     |
| 2   | CHG    | O    | Charge FET driver                        |
| 3   | VSS    | —    | Chip VSS                                 |
| 4   | SDA    | I/O  | I2C communication to the host controller |
| 5   | SCL    | I    | I2C communication to the host controller |
| 6   | TS1    | I    | Thermistor #1 positive terminal(1)       |
| 7   | CAP1   | O    | Capacitor to VSS                         |
| 8   | REGOUT | P    | Output LDO                               |
| 9   | REGSRC | I    | Input source for output LDO              |
| 10  | VC5X   | P    | Thermistor #2 negative terminal          |
| 11  | NC     | —    | No connect (short to CAP2)               |
| 12  | NC     | —    | No connect (short to CAP2)               |
| 13  | TS2    | I    | Thermistor #2 positive terminal(1)       |
| 14  | CAP2   | O    | Capacitor to VC5X                        |
| 15  | VC10X  | P    | Thermistor #3 negative terminal          |
| 16  | NC     | —    | No connect (short to CAP3)               |
| 17  | NC     | —    | No connect (short to CAP3)               |
| 18  | TS3    | I    | Thermistor #3 positive terminal(1)       |
| 19  | CAP3   | O    | Capacitor to VC10X                       |
| 20  | BAT    | P    | Battery (top-most) terminal              |
| 21  | NC     | —    | No connect                               |
| 22  | NC     | —    | No connect                               |
| 23  | NC     | —    | No connect                               |
| 24  | VC15   | I    | Sense voltage for 15th cell positive terminal |
| 25  | VC14   | I    | Sense voltage for 14th cell positive terminal |
| 26  | VC13   | I    | Sense voltage for 13th cell positive terminal |
| 27  | VC12   | I    | Sense voltage for 12th cell positive terminal |
| 28  | VC11   | I    | Sense voltage for 11th cell positive terminal |
| 29  | VC10B  | I    | Sense voltage for 11th cell negative terminal |
| 30  | VC10   | I    | Sense voltage for 10th cell positive terminal |
| 31  | VC9    | I    | Sense voltage for 9th cell positive terminal  |
| 32  | VC8    | I    | Sense voltage for 8th cell positive terminal  |
| 33  | VC7    | I    | Sense voltage for 7th cell positive terminal  |
| 34  | VC6    | I    | Sense voltage for 6th cell positive terminal  |
| 35  | VC5B   | I    | Sense voltage for 6th cell negative terminal  |
| 36  | VC5    | I    | Sense voltage for 5th cell positive terminal  |
| 37  | VC4    | I    | Sense voltage for 4th cell positive terminal  |
| 38  | VC3    | I    | Sense voltage for 3rd cell positive terminal  |
| 39  | VC2    | I    | Sense voltage for 2nd cell positive terminal  |
| 40  | VC1    | I    | Sense voltage for 1st cell positive terminal  |
| 41  | VC0    | I    | Sense voltage for 1st cell negative terminal  |
| 42  | SRP    | I    | Negative current sense (nearest VSS)          |
| 43  | SRN    | I    | Positive current sense                        |
| 44  | ALERT  | I/O  | Alert output and override input               |

**Notes:**
1. If not used, pull down to group ground reference (VSS for TS1, VC5X for TS2, and VC10X for TS3) with a 10-kohm nominal resistor.

## Package Information

| Part Number    | Package    | Body Size (NOM)    |
| -------------- | ---------- | ------------------ |
| BQ76940        | TSSOP (44) | 11.00 mm x 4.40 mm |

## Device Versions

| Device    | Cells     | Package       | I2C Address (7-bit) | LDO (V) | CRC   |
| --------- | --------- | ------------- | ------------------- | ------- | ----- |
| BQ7694000 | 9-15      | 44-TSSOP (DBT)| 0x08                | 2.5     | No    |
| BQ7694001 | 9-15      | 44-TSSOP (DBT)| 0x08                | 2.5     | Yes   |
| BQ7694002 | 9-15      | 44-TSSOP (DBT)| 0x08                | 3.3     | No    |
| BQ7694003 | 9-15      | 44-TSSOP (DBT)| 0x08                | 3.3     | Yes   |
| BQ7694006 | 9-15      | 44-TSSOP (DBT)| 0x18                | 2.5     | No    |

## Pin Usage Recommendations

| PIN NAME    | RECOMMENDATION                                                                                                                                                                      |
| ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| DSG, CHG    | DSG and CHG are outputs and may be left unconnected if not used.                                                                                                                    |
| VSS         | Must be used                                                                                                                                                                        |
| SDA, SCL    | Must be used                                                                                                                                                                        |
| TSn         | Must have a thermistor or pull down resistor to the group reference. TS1 must have a rising edge to boot the part.                                                                  |
| CAPn        | A capacitor must be installed.                                                                                                                                                      |
| REGOUT      | REGOUT also supplies internal circuits. A capacitor must be installed even if REGOUT is not used for external circuitry.                                                            |
| REGSRC      | Must be supplied                                                                                                                                                                    |
| BAT         | Primary power pin for the part, must be connected to the top cell through the power filter                                                                                          |
| VC5x, VC10x | Must connect to the appropriate cell through the power filter                                                                                                                       |
| NC          | Some pins named NC must be connected to the appropriate CAPn pin. See Pin Configuration and Functions.                                                                              |
| VCn         | Cell voltage sense input pins. Must be connected through the input filter to the cells. When not all cells are needed, connect as described in Configuring Alternative Cell Counts. |
| SRP, SRN    | Current sense inputs. When not used, connect to VSS.                                                                                                                                |
| ALERT       | When not used, a pulldown is recommended.                                                                                                                                           |

## Cell Connection Configurations for BQ76940

| Cell Input | 9 Cells | 10 Cells | 11 Cells | 12 Cells | 13 Cells | 14 Cells | 15 Cells |
| ---------- | ------- | -------- | -------- | -------- | -------- | -------- | -------- |
| VC15-VC14  | CELL 9  | CELL 10  | CELL 11  | CELL 12  | CELL 13  | CELL 14  | CELL 15  |
| VC14-VC13  | short   | short    | short    | short    | short    | short    | CELL 14  |
| VC13-VC12  | short   | short    | short    | CELL 11  | CELL 12  | CELL 13  | CELL 13  |
| VC12-VC11  | CELL 8  | CELL 9   | CELL 10  | CELL 10  | CELL 11  | CELL 12  | CELL 12  |
| VC11-VC10b | CELL 7  | CELL 8   | CELL 9   | CELL 9   | CELL 10  | CELL 11  | CELL 11  |
| VC10-VC9   | CELL 6  | CELL 7   | CELL 8   | CELL 8   | CELL 9   | CELL 10  | CELL 10  |
| VC9-VC8    | short   | short    | short    | short    | short    | CELL 9   | CELL 9   |
| VC8-VC7    | short   | short    | CELL 7   | CELL 7   | CELL 8   | CELL 8   | CELL 8   |
| VC7-VC6    | CELL 5  | CELL 6   | CELL 6   | CELL 6   | CELL 7   | CELL 7   | CELL 7   |
| VC6-VC5b   | CELL 4  | CELL 5   | CELL 5   | CELL 5   | CELL 6   | CELL 6   | CELL 6   |
| VC5-VC4    | CELL 3  | CELL 4   | CELL 4   | CELL 4   | CELL 5   | CELL 5   | CELL 5   |
| VC4-VC3    | short   | short    | short    | short    | CELL 4   | CELL 4   | CELL 4   |
| VC3-VC2    | short   | CELL 3   | CELL 3   | CELL 3   | CELL 3   | CELL 3   | CELL 3   |
| VC2-VC1    | CELL 2  | CELL 2   | CELL 2   | CELL 2   | CELL 2   | CELL 2   | CELL 2   |
| VC1-VC0    | CELL 1  | CELL 1   | CELL 1   | CELL 1   | CELL 1   | CELL 1   | CELL 1   |

(Tables referenced from datasheet Sections 6.4 and 9.1.8)
