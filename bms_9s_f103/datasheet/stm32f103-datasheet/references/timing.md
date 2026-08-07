# stm32f103 AC / Timing Parameters

> **PURPOSE:** Clock configuration parameters, peripheral timing, and AC characteristics from the reference manual.

## Validation Rules

Every timing parameter is validated against the datasheet minimum and maximum:
- `f_actual <= f_max` and `t_actual >= t_min` -> **COMPLIANT**
- `f_actual = f_max` or `t_actual = t_min` -> **BORDERLINE**
- `f_actual > f_max` or `t_actual < t_min` -> **VIOLATION**

---

## 1. System Clock Tree

From RM0008 Chapter 7.2 (Low-, Medium-, High- and XL-density RCC):

### Clock Sources

| Clock | Type     | Frequency     | Stability    | Usage                                |
|-------|----------|---------------|--------------|--------------------------------------|
| HSI   | Internal RC | 8 MHz      | +/-1% after trim | System clock, PLL source        |
| HSE   | External crystal/clock | 4-16 MHz | High (crystal) | System clock, PLL source       |
| LSE   | External 32.768 kHz | 32.768 kHz | High (crystal) | RTC, RCC BDCR               |
| LSI   | Internal RC | ~40 kHz      | Low          | IWDG, RTC (when LSE not used)        |
| PLL   | Internal   | Up to 72 MHz | Depends on source | System clock, USB 48 MHz       |

### Maximum Clock Frequencies

| Clock Domain | Signal  | Max Frequency         | Notes                              |
|--------------|---------|-----------------------|------------------------------------|
| System       | SYSCLK  | 72 MHz                | Max from PLL, 0-72 MHz             |
| AHB          | HCLK    | 72 MHz                | SYSCLK / HPRE                      |
| APB1         | PCLK1   | **36 MHz**            | HCLK / PPRE1 (max 36 MHz!)         |
| APB2         | PCLK2   | 72 MHz                | HCLK / PPRE2                       |
| ADC          | ADCCLK  | **14 MHz**            | PCLK2 / ADCPRE (max 14 MHz!)       |
| USB          | USBCLK  | **48 MHz**            | PLL output / 1.5 (USBPRE=0)        |
| SPI1         | SPI_CK  | 36 MHz (master)       | PCLK2 / BR (max 36 MHz master)     |
| SPI2/3       | SPI_CK  | 18 MHz (master)       | PCLK1 / BR (max 18 MHz master)     |
| MCO output   | MCO     | 50 MHz                | Max IO speed on MCO pin            |

### Clock Signal Characteristics (from RM0008)

| Parameter                  | Conditions       | Min     | Typ   | Max    | Unit |
|----------------------------|------------------|---------|-------|--------|------|
| HSI frequency              | Factory trimmed  | -       | 8     | -      | MHz  |
| HSI trimming step          |                  | -       | 40    | -      | kHz  |
| HSI startup time           |                  | -       | 1     | -      | us   |
| HSE frequency range        |                  | 4       | 8     | 16     | MHz  |
| LSE frequency              |                  | -       | 32.768| -      | kHz  |
| LSI frequency              |                  | -       | 40    | -      | kHz  |
| PLL VCO output             |                  | -       | -     | 72     | MHz  |

## 2. Flash Memory Wait States

From RM0008 Section 3.3.3 (FLASH_ACR register):

| SYSCLK Range        | LATENCY Bits | Wait States | PRFTBE | HLFCYA |
|---------------------|--------------|-------------|--------|--------|
| 0 < SYSCLK <= 24 MHz | 000         | 0 WS        | Optional | Optional (>8MHz) |
| 24 MHz < SYSCLK <= 48 MHz | 001    | 1 WS        | Recommended | Off |
| 48 MHz < SYSCLK <= 72 MHz | 010    | 2 WS        | Required | Off |

**Critical Rules:**
- Prefetch buffer must be enabled when AHB prescaler != 1 (HPRE > 0)
- Half-cycle access only usable with SYSCLK = HCLK <= 8 MHz, no PLL
- Prefetch buffer must be toggled only when SYSCLK < 24 MHz and no prescaler

## 3. PLL Configuration

From RM0008 Section 7.3.2 (RCC_CFGR):

**PLL output formula:**
- PLLSRC=0 (HSI): **PLLCLK = (8 MHz / 2) * PLLMUL** = 4 MHz * PLLMUL
- PLLSRC=1 (HSE): **PLLCLK = HSE / PLLXTPRE * PLLMUL** (PLLXTPRE: 0=div1, 1=div2)

| PLLMUL[3:0] | Multiplication Factor |
|-------------|----------------------|
| 0000        | x2                   |
| 0001        | x3                   |
| 0010        | x4                   |
| 0011        | x5                   |
| 0100        | x6                   |
| 0101        | x7                   |
| 0110        | x8                   |
| 0111        | x9                   |
| 1000        | x10                  |
| 1001        | x11                  |
| 1010        | x12                  |
| 1011        | x13                  |
| 1100        | x14                  |
| 1101        | x15                  |
| 1110        | x16                  |
| 1111        | x16                  |

**Constraint:** PLL output must not exceed **72 MHz**.

## 4. USB Clock Configuration

From RM0008 Section 7.3.2 (RCC_CFGR):

- **USBPRE=0:** USB clock = PLL output / 1.5
- **USBPRE=1:** USB clock = PLL output / 1

**Constraint:** USB clock must be **48 MHz**. Therefore:
- USBPRE=0: PLL output must be **72 MHz** (72/1.5=48)
- USBPRE=1: PLL output must be **48 MHz** (48/1=48)

## 5. ADC Timing

From RM0008 Chapter 11:

- **Max ADCCLK:** 14 MHz
- **Conversion time:** Tconv = Sampling time + 12.5 ADC clock cycles
- **Sample time per channel:** Selectable via ADC_SMPR1/2 (1.5 to 239.5 cycles)
- **Stabilization time (t_STAB):** Required after ADON=1 before first conversion
- **ADC prescaler:** ADCPRE in RCC_CFGR -- divides PCLK2 by 2, 4, 6, or 8

Example: ADCCLK=14 MHz, sample time=1.5 cycles -> Tconv = 1.5+12.5 = 14 cycles = 1 us

## 6. USART Baud Rate

From RM0008 Chapter 27:

**Baud rate formula:** **Baud = fCK / (16 * USARTDIV)**

Where USARTDIV = DIV_Mantissa + DIV_Fraction/16 (from USART_BRR)

- USART1: fCK = PCLK2 (max 72 MHz)
- USART2/3/4/5: fCK = PCLK1 (max 36 MHz)

### Common Baud Rate Examples (with PCLK=72 MHz for USART1)

| Baud Rate | USART_BRR Value | Error |
|-----------|-----------------|-------|
| 2.4 kbps  | 1875            | 0%    |
| 9.6 kbps  | 468.75          | 0%    |
| 19.2 kbps | 234.375         | 0%    |
| 57.6 kbps | 78.125          | 0%    |
| 115.2 kbps| 39.0625         | 0%    |
| 230.4 kbps| 19.5            | 0.16% |
| 460.8 kbps| 9.75            | 0.16% |
| 921.6 kbps| 4.875           | 0.16% |
| 4.5 Mbps  | 1               | 0%    |

### Receiver Tolerance

| Condition              | M=0 (8-bit) | M=1 (9-bit) |
|------------------------|-------------|-------------|
| DIV_Fraction=0, NF error | 3.75%      | 3.41%       |
| DIV_Fraction=0, NF don't care | 4.375% | 3.97%  |
| DIV_Fraction!=0, NF error | 3.33%     | 3.03%       |
| DIV_Fraction!=0, NF don't care | 3.88% | 3.53%    |

## 7. SPI Timing

From RM0008 Chapter 25:

**SPI Baud Rate (master mode):**

| BR[2:0] | Division Factor |
|---------|-----------------|
| 000     | fPCLK/2         |
| 001     | fPCLK/4         |
| 010     | fPCLK/8         |
| 011     | fPCLK/16        |
| 100     | fPCLK/32        |
| 101     | fPCLK/64        |
| 110     | fPCLK/128       |
| 111     | fPCLK/256       |

**Max SPI frequencies:**
- SPI1 (APB2): PCLK2 max 72 MHz -> SPI master max **36 MHz** (PCLK2/2)
- SPI2/3 (APB1): PCLK1 max 36 MHz -> SPI master max **18 MHz** (PCLK1/2)

## 8. Timer Timing

From RM0008 Chapters 14-17:

**Timer clock:**
- TIM1, TIM8, TIM9-11: Clocked by PCLK2 (max 72 MHz)
- TIM2-7, TIM12-14: Clocked by PCLK1 (max 36 MHz)
- If APB prescaler = 1, timer clock = PCLKx
- If APB prescaler > 1, timer clock = **2 * PCLKx**

**Output frequency:** **f_TIM = CK_CNT / (ARR + 1)** where CK_CNT = timer_clock / (PSC + 1)

**PWM resolution:** **resolution = log2(ARR + 1)** bits

**Minimum PWM frequency with 16-bit timer:** f_TIM / 65536 (with PSC=0, ARR=65535)
**Maximum PWM frequency with 16-bit timer:** f_TIM (with PSC=0, ARR=0 -- but ARR=0 gives 50% duty only)

## 9. I2C Timing

From RM0008 Chapter 26:

**I2C Clock Frequencies from I2C_CCR:**

| Mode | Max SCL | CCR Formula           | Min CCR |
|------|---------|-----------------------|---------|
| Standard | 100 kHz | T_SCL = 2 * CCR * TPCLK1 | CCR > 4 |
| Fast (DUTY=0) | 400 kHz | T_SCL = 2 * CCR * TPCLK1 | CCR > 1 |
| Fast (DUTY=1) | 400 kHz | T_SCL = CCR * 25/9 * TPCLK1 | CCR > 1 |

**I2C_TRISE:** Must be programmed per the max rise time (1000 ns standard mode, 300 ns fast mode):
- TRISE = (t_max_rise / TPCLK1) + 1

## 10. Power Mode Timings

From RM0008 Chapter 5:

| Transition              | Latency                              |
|-------------------------|--------------------------------------|
| Sleep entry            | WFI/WFE instruction, no delay       |
| Sleep wakeup           | None (interrupt latency only)        |
| Stop entry             | Instant (delayed if Flash/APB access ongoing) |
| Stop wakeup            | HSI RC startup + regulator wakeup    |
| Standby entry          | WFI/WFE + SLEEPDEEP + PDDS           |
| Standby wakeup         | Full reset phase                     |
