---
name: stm32f103-datasheet
description: >
  STM32F103xx Advanced ARM Cortex-M3 32-bit MCU (RM0008 Reference Manual) datasheet skill. Provides complete peripheral register maps, clock configuration, timing parameters, and operation sequences for AI-assisted embedded driver development. When the user mentions STM32F103, STM32F103xx, STM32F10xxx, STM32, STM32 MCU, STM32F1, Cortex-M3, RM0008 in the context of embedded programming, driver development, register configuration, or hardware validation, use this skill. Even if the user does not explicitly name stm32f103, but describes a device matching its capabilities, suggest this skill.
compatibility: requires_read_permission
---

# STM32F103xx Advanced ARM Cortex-M3 32-bit MCU (RM0008 Reference Manual) Datasheet Skill

This skill provides the complete RM0008 Reference Manual for **STM32F103xx Advanced ARM Cortex-M3 32-bit MCU** structured for AI-assisted embedded development.

## IMPORTANT: Reference Manual vs Datasheet

This skill is based on RM0008 (Reference Manual), which covers **peripheral registers and functional descriptions**. It does NOT contain:
- Pin definitions -> see device-specific datasheet
- Electrical characteristics -> see device-specific datasheet
- CPU instruction set -> see PM0056 (Cortex-M3 Programming Manual)

## Three-Way Operation

When the user asks a question related to stm32f103, determine which mode(s) apply:

### Mode 1: Reference Query

Triggered by questions like "what is register X?", "what does bit Y do?", "how do I configure peripheral Z?".

**Workflow:**
1. Identify the peripheral -> `references/registers.md` for register definitions
2. For clock config -> also read `references/timing.md`
3. For init sequences -> also read `references/operations.md`
4. Read the relevant reference file(s)
5. Return the exact datasheet definition with register/bit-field reference

### Mode 2: Code Generation

Triggered by requests like "write a driver for USART", "generate GPIO init code", "implement SPI read/write".

**Workflow:**
1. Read `references/registers.md` for the peripheral's register definitions
2. Read `references/operations.md` for initialization sequences
3. Read `references/timing.md` for clock configuration requirements
4. Generate code following the exact register settings from the reference manual
5. Annotate generated code with register/bit-field references

### Mode 3: Parameter Validation

Triggered by questions like "can I run at X MHz?", "is this clock config valid?", "check this GPIO setting".

**Workflow:**
1. Identify the parameter category:
   - Clock frequency / prescaler -> `references/timing.md`
   - GPIO / peripheral config -> `references/registers.md`
   - Boot / power mode -> `references/operations.md`
2. Read the relevant reference file(s)
3. Compare the user's value against the reference manual limits
4. Return exactly one of:
   - **COMPLIANT** -- value safely within allowed limits
   - **BORDERLINE** -- value meets but does not exceed the limit; marginal for production
   - **VIOLATION** -- value exceeds allowed limits; may cause malfunction

## Reference Files

Read reference files on demand -- never load everything at once:

| File | When to Read |
|------|-------------|
| [references/pinout.md](references/pinout.md) | Pin function, GPIO assignment questions (redirects to device datasheet) |
| [references/electrical.md](references/electrical.md) | Voltage, current, power constraint validation (redirects to device datasheet) |
| [references/instructions.md](references/instructions.md) | CPU instruction set questions (redirects to PM0056) |
| [references/registers.md](references/registers.md) | Register bit-field definitions for all peripherals |
| [references/timing.md](references/timing.md) | Clock tree, peripheral clock limits, Flash latency |
| [references/operations.md](references/operations.md) | Boot, system init, Flash programming, low-power sequences |

## Peripheral Index

| Peripheral | RM0008 Chapter | Base Address | Register File |
|------------|----------------|-------------|---------------|
| RCC | 7/8 | 0x4002 1000 | references/registers.md |
| GPIO/AFIO | 9 | 0x4001 0800/0000 | references/registers.md |
| NVIC/EXTI | 10 | 0xE000E000 / 0x4001 0400 | references/registers.md |
| ADC | 11 | 0x4001 2400 | references/registers.md |
| DAC | 12 | 0x4000 7400 | references/registers.md |
| DMA | 13 | 0x4002 0000 | references/registers.md |
| TIM1/TIM8 | 14 | 0x4001 2C00/3400 | references/registers.md |
| TIM2-TIM5 | 15 | 0x4000 0000-0C00 | references/registers.md |
| TIM9-TIM14 | 16 | Various | references/registers.md |
| TIM6/TIM7 | 17 | 0x4000 1000/1400 | references/registers.md |
| RTC | 18 | 0x4000 2800 | references/registers.md |
| IWDG | 19 | 0x4000 3000 | references/registers.md |
| WWDG | 20 | 0x4000 2C00 | references/registers.md |
| FSMC | 21 | 0xA000 0000 | references/registers.md |
| USB | 22/23 | 0x4000 5C00 | references/registers.md |
| SDIO | 22 | 0x4001 8000 | references/registers.md |
| CAN | 24 | 0x4000 6400/6800 | references/registers.md |
| SPI/I2S | 25 | 0x4001 3000 | references/registers.md |
| I2C | 26 | 0x4000 5400/5800 | references/registers.md |
| USART/UART | 27 | 0x4001 3800 | references/registers.md |
| PWR | 5 | 0x4000 7000 | references/registers.md |
| BKP | 6 | 0x4000 6C00 | references/registers.md |
| CRC | 4 | 0x4002 3000 | references/registers.md |
| Flash IF | 3 | 0x4002 2000 | references/registers.md |

## Key System Constraints

| Parameter | Limit | Source |
|-----------|-------|--------|
| SYSCLK max | 72 MHz | timing.md |
| APB1 (PCLK1) max | 36 MHz | timing.md |
| APB2 (PCLK2) max | 72 MHz | timing.md |
| ADCCLK max | 14 MHz | timing.md |
| USB clock | 48 MHz | timing.md |
| VDD range | 2.0-3.6V | electrical.md |
| Flash wait states | 0WS @ <=24MHz, 1WS @ <=48MHz, 2WS @ <=72MHz | timing.md |
| PLL output max | 72 MHz | timing.md |
