# stm32f103 Pin Definitions

> **PURPOSE:** Every package pin -- name, number(s), type, and functional description. Used to cross-check schematic connections and GPIO assignments.

> **⚠️ NOT FOUND IN THIS DOCUMENT** -- This is the RM0008 Reference Manual. Pin definitions and package information are in the device-specific datasheets (e.g., STM32F103x8/xB datasheet). Refer to the STM32F103xx datasheet on www.st.com for pinout information. Key documents: STM32F103x8/xB datasheet (medium-density), STM32F103xC/xD/xE datasheet (high-density).

## GPIO Alternate Function Overview

From RM0008 Chapter 9: The STM32F103 supports up to 7 GPIO ports (GPIOA-GPIOG), each with 16 pins. Most pins have alternate functions mapped via the AFIO registers. Refer to the device datasheet for pin-specific alternate function assignments.

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
