# stm32f103 Electrical Characteristics

> **PURPOSE:** Absolute maximum ratings, DC characteristics, and operating conditions -- used as **hard constraints** during code validation.

> **⚠️ NOT FOUND IN THIS DOCUMENT** -- This is the RM0008 Reference Manual. Electrical characteristics, absolute maximum ratings, and operating conditions are in the device-specific datasheets. Refer to the STM32F103xx datasheet on www.st.com. The reference manual does cover power supply architecture (Chapter 5: PWR) and ADC reference voltage requirements.

## Power Supply Architecture (from RM0008 Chapter 5)

The STM32F103 requires:
- **VDD**: 2.0-3.6V (main power supply)
- **VDDA**: 2.0-3.6V (analog supply, must be same potential as VDD)
- **VBAT**: 1.8-3.6V (backup domain)
- Voltage regulator output: 1.8V (internal, requires external decoupling capacitor)

## Programmable Voltage Detector (PVD) Thresholds

From RM0008 PWR_CR register (Section 5.4.1):

| PLS[2:0] | Threshold |
|----------|-----------|
| 000      | 2.2V      |
| 001      | 2.3V      |
| 010      | 2.4V      |
| 011      | 2.5V      |
| 100      | 2.6V      |
| 101      | 2.7V      |
| 110      | 2.8V      |
| 111      | 2.9V      |

## Temperature Sensor

From RM0008 Section 11.10: Connected to ADC1_IN16. The temperature sensor voltage varies linearly with temperature. Refer to the device datasheet for exact conversion formula and slope.

For detailed electrical limits, see the device-specific datasheet.
