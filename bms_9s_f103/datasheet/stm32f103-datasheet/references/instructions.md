# stm32f103 Instruction Set / CPU Reference

> **PURPOSE:** CPU instruction set reference for the ARM Cortex-M3 core.

> **⚠️ NOT COVERED IN RM0008** -- The STM32F103 uses the ARM Cortex-M3 processor core with the Thumb-2 instruction set. The RM0008 Reference Manual covers peripherals and memory, NOT the CPU instruction set. For CPU instructions, refer to:
> - **PM0056**: STM32F10xxx Cortex-M3 Programming Manual (available from www.st.com)
> - **ARM Cortex-M3 Technical Reference Manual**
> - **ARMv7-M Architecture Reference Manual**

## Key CPU Features (Cortex-M3)

- 32-bit ARM Cortex-M3 core, up to 72 MHz
- Thumb-2 instruction set (mix of 16-bit and 32-bit instructions)
- Single-cycle multiply, hardware divide
- Nested Vectored Interrupt Controller (NVIC) with up to 68 maskable interrupt channels
- Memory Protection Unit (MPU) -- optional
- 3-stage pipeline with branch speculation
- Debug support: SWD/JTAG, DWT, ITM, FPB

## SysTick Calibration Value

From RM0008 Section 10.1.1: The SysTick calibration value is fixed to **9000** (0x2328), which gives a 10 ms interval when the SysTick clock is set to 9 MHz (HCLK/8, with HCLK = 72 MHz).
