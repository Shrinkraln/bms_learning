# stm32f103 Register Map

> **PURPOSE:** Complete register map for all STM32F103 peripherals. Each peripheral's registers are documented with offset, name, access type, reset value, and bit-field descriptions.

## Reading This File

Each register is documented as a table. Bit positions use 0-indexed numbering (bit 0 = LSB). Access types: R = read-only, W = write-only, R/W = read/write, W1C = write-1-to-clear, VOL = volatile (may change between reads).

---

## Memory Map (Peripheral Base Addresses)

From RM0008 Chapter 3, Table 3:

| Bus  | Peripheral    | Base Address  | Size      | RM Chapter |
|------|---------------|---------------|-----------|------------|
| AHB  | FSMC          | 0xA000 0000   | 4 KB      | Ch 21      |
| AHB  | CRC           | 0x4002 3000   | 1 KB      | Ch 4       |
| AHB  | Flash IF      | 0x4002 2000   | 1 KB      | Ch 3       |
| AHB  | RCC           | 0x4002 1000   | 1 KB      | Ch 7/8     |
| AHB  | DMA2          | 0x4002 0400   | 1 KB      | Ch 13      |
| AHB  | DMA1          | 0x4002 0000   | 1 KB      | Ch 13      |
| AHB  | SDIO          | 0x4001 8000   | 1 KB      | Ch 22      |
| APB2 | TIM11         | 0x4001 5400   | 1 KB      | Ch 16      |
| APB2 | TIM10         | 0x4001 5000   | 1 KB      | Ch 16      |
| APB2 | TIM9          | 0x4001 4C00   | 1 KB      | Ch 16      |
| APB2 | ADC3          | 0x4001 3C00   | 1 KB      | Ch 11      |
| APB2 | USART1        | 0x4001 3800   | 1 KB      | Ch 27      |
| APB2 | TIM8          | 0x4001 3400   | 1 KB      | Ch 14      |
| APB2 | SPI1          | 0x4001 3000   | 1 KB      | Ch 25      |
| APB2 | TIM1          | 0x4001 2C00   | 1 KB      | Ch 14      |
| APB2 | ADC2          | 0x4001 2800   | 1 KB      | Ch 11      |
| APB2 | ADC1          | 0x4001 2400   | 1 KB      | Ch 11      |
| APB2 | GPIOG         | 0x4001 2000   | 1 KB      | Ch 9       |
| APB2 | GPIOF         | 0x4001 1C00   | 1 KB      | Ch 9       |
| APB2 | GPIOE         | 0x4001 1800   | 1 KB      | Ch 9       |
| APB2 | GPIOD         | 0x4001 1400   | 1 KB      | Ch 9       |
| APB2 | GPIOC         | 0x4001 1000   | 1 KB      | Ch 9       |
| APB2 | GPIOB         | 0x4001 0C00   | 1 KB      | Ch 9       |
| APB2 | GPIOA         | 0x4001 0800   | 1 KB      | Ch 9       |
| APB2 | EXTI          | 0x4001 0400   | 1 KB      | Ch 10      |
| APB2 | AFIO          | 0x4001 0000   | 1 KB      | Ch 9       |
| APB1 | DAC           | 0x4000 7400   | 1 KB      | Ch 12      |
| APB1 | PWR           | 0x4000 7000   | 1 KB      | Ch 5       |
| APB1 | BKP           | 0x4000 6C00   | 1 KB      | Ch 6       |
| APB1 | bxCAN1        | 0x4000 6400   | 1 KB      | Ch 24      |
| APB1 | bxCAN2        | 0x4000 6800   | 1 KB      | Ch 24      |
| APB1 | USB FS        | 0x4000 5C00   | 1 KB      | Ch 23      |
| APB1 | I2C2          | 0x4000 5800   | 1 KB      | Ch 26      |
| APB1 | I2C1          | 0x4000 5400   | 1 KB      | Ch 26      |
| APB1 | UART5         | 0x4000 5000   | 1 KB      | Ch 27      |
| APB1 | UART4         | 0x4000 4C00   | 1 KB      | Ch 27      |
| APB1 | USART3        | 0x4000 4800   | 1 KB      | Ch 27      |
| APB1 | USART2        | 0x4000 4400   | 1 KB      | Ch 27      |
| APB1 | SPI3/I2S      | 0x4000 3C00   | 1 KB      | Ch 25      |
| APB1 | SPI2/I2S      | 0x4000 3800   | 1 KB      | Ch 25      |
| APB1 | IWDG          | 0x4000 3000   | 1 KB      | Ch 19      |
| APB1 | WWDG          | 0x4000 2C00   | 1 KB      | Ch 20      |
| APB1 | RTC           | 0x4000 2800   | 1 KB      | Ch 18      |
| APB1 | TIM14         | 0x4000 2000   | 1 KB      | Ch 16      |
| APB1 | TIM13         | 0x4000 1C00   | 1 KB      | Ch 16      |
| APB1 | TIM12         | 0x4000 1800   | 1 KB      | Ch 16      |
| APB1 | TIM7          | 0x4000 1400   | 1 KB      | Ch 17      |
| APB1 | TIM6          | 0x4000 1000   | 1 KB      | Ch 17      |
| APB1 | TIM5          | 0x4000 0C00   | 1 KB      | Ch 15      |
| APB1 | TIM4          | 0x4000 0800   | 1 KB      | Ch 15      |
| APB1 | TIM3          | 0x4000 0400   | 1 KB      | Ch 15      |
| APB1 | TIM2          | 0x4000 0000   | 1 KB      | Ch 15      |

---

# 1. Reset and Clock Control (RCC)

> Base: **0x4002 1000** | RM0008 Chapter 7

## RCC Register Summary

| Offset | Register       | Description                      |
|--------|----------------|----------------------------------|
| 0x00   | RCC_CR         | Clock control register           |
| 0x04   | RCC_CFGR       | Clock configuration register     |
| 0x08   | RCC_CIR        | Clock interrupt register         |
| 0x0C   | RCC_APB2RSTR   | APB2 peripheral reset register   |
| 0x10   | RCC_APB1RSTR   | APB1 peripheral reset register   |
| 0x14   | RCC_AHBENR     | AHB peripheral clock enable      |
| 0x18   | RCC_APB2ENR    | APB2 peripheral clock enable     |
| 0x1C   | RCC_APB1ENR    | APB1 peripheral clock enable     |
| 0x20   | RCC_BDCR       | Backup domain control register   |
| 0x24   | RCC_CSR        | Control/status register          |

## RCC_CR (Offset 0x00, Reset: 0x0000 XX83)

| Bits | Field      | Access | Description                                       |
|------|------------|--------|---------------------------------------------------|
| 25   | PLLRDY     | R      | PLL lock flag (1=locked)                          |
| 24   | PLLON      | R/W    | PLL enable (0=OFF, 1=ON)                          |
| 19   | CSSON      | R/W    | Clock security system enable                      |
| 18   | HSEBYP     | R/W    | HSE oscillator bypass                             |
| 17   | HSERDY     | R      | HSE oscillator ready flag                         |
| 16   | HSEON      | R/W    | HSE oscillator enable                             |
| 15:8 | HSICAL[7:0]| R      | HSI calibration (auto-initialized)                |
| 7:3  | HSITRIM[4:0]| R/W   | HSI trimming (default 16, ~40 kHz step)           |
| 1    | HSIRDY     | R      | HSI 8 MHz RC ready flag                           |
| 0    | HSION      | R/W    | HSI oscillator enable                             |

## RCC_CFGR (Offset 0x04, Reset: 0x0000 0000)

| Bits | Field        | Access | Description                                            |
|------|--------------|--------|--------------------------------------------------------|
| 26:24| MCO[2:0]     | R/W    | Microcontroller clock output                            |
| 22   | USBPRE       | R/W    | USB prescaler (0=PLL/1.5, 1=PLL/1)                     |
| 21:18| PLLMUL[3:0]  | R/W    | PLL multiplier (x2..x16; max output 72 MHz)            |
| 17   | PLLXTPRE     | R/W    | HSE divider for PLL (0=/1, 1=/2)                       |
| 16   | PLLSRC       | R/W    | PLL source (0=HSI/2, 1=HSE)                            |
| 15:14| ADCPRE[1:0]  | R/W    | ADC prescaler (00=PCLK2/2, 01=/4, 10=/6, 11=/8)        |
| 13:11| PPRE2[2:0]   | R/W    | APB2 prescaler (0xx=/1, 100=/2, 101=/4, 110=/8, 111=/16)|
| 10:8 | PPRE1[2:0]   | R/W    | APB1 prescaler (max 36 MHz; same encoding as PPRE2)    |
| 7:4  | HPRE[3:0]    | R/W    | AHB prescaler (0xxx=/1, 1000=/2, 1001=/4, .., 1111=/512)|
| 3:2  | SWS[1:0]     | R      | System clock switch status (00=HSI, 01=HSE, 10=PLL)    |
| 1:0  | SW[1:0]      | R/W    | System clock switch (00=HSI, 01=HSE, 10=PLL)           |

## RCC_APB2ENR (Offset 0x18, Reset: 0x0000 0000)

| Bit | Field      | Description                |
|-----|------------|----------------------------|
| 21  | TIM11EN    | TIM11 clock enable         |
| 20  | TIM10EN    | TIM10 clock enable         |
| 19  | TIM9EN     | TIM9 clock enable          |
| 15  | ADC3EN     | ADC3 clock enable          |
| 14  | USART1EN   | USART1 clock enable        |
| 13  | TIM8EN     | TIM8 clock enable          |
| 12  | SPI1EN     | SPI1 clock enable          |
| 11  | TIM1EN     | TIM1 clock enable          |
| 10  | ADC2EN     | ADC2 clock enable          |
| 9   | ADC1EN     | ADC1 clock enable          |
| 8   | IOPGEN     | GPIOG clock enable         |
| 7   | IOPFEN     | GPIOF clock enable         |
| 6   | IOPEEN     | GPIOE clock enable         |
| 5   | IOPDEN     | GPIOD clock enable         |
| 4   | IOPCEN     | GPIOC clock enable         |
| 3   | IOPBEN     | GPIOB clock enable         |
| 2   | IOPAEN     | GPIOA clock enable         |
| 0   | AFIOEN     | Alternate function IO enable|

## RCC_APB1ENR (Offset 0x1C, Reset: 0x0000 0000)

| Bit | Field      | Description                |
|-----|------------|----------------------------|
| 29  | DACEN      | DAC clock enable           |
| 28  | PWREN      | PWR clock enable           |
| 27  | BKPEN      | BKP clock enable           |
| 25  | CANEN      | CAN clock enable           |
| 23  | USBEN      | USB clock enable           |
| 22  | I2C2EN     | I2C2 clock enable          |
| 21  | I2C1EN     | I2C1 clock enable          |
| 20  | UART5EN    | UART5 clock enable         |
| 19  | UART4EN    | UART4 clock enable         |
| 18  | USART3EN   | USART3 clock enable        |
| 17  | USART2EN   | USART2 clock enable        |
| 15  | SPI3EN     | SPI3 clock enable          |
| 14  | SPI2EN     | SPI2 clock enable          |
| 11  | WWDGEN     | WWDG clock enable          |
| 8   | TIM14EN    | TIM14 clock enable         |
| 7   | TIM13EN    | TIM13 clock enable         |
| 6   | TIM12EN    | TIM12 clock enable         |
| 5   | TIM7EN     | TIM7 clock enable          |
| 4   | TIM6EN     | TIM6 clock enable          |
| 3   | TIM5EN     | TIM5 clock enable          |
| 2   | TIM4EN     | TIM4 clock enable          |
| 1   | TIM3EN     | TIM3 clock enable          |
| 0   | TIM2EN     | TIM2 clock enable          |

## RCC_BDCR (Offset 0x20, Reset: 0x0000 0000)

| Bits | Field      | Access | Description                                    |
|------|------------|--------|------------------------------------------------|
| 16   | BDRST      | R/W    | Backup domain software reset                   |
| 15   | RTCEN      | R/W    | RTC clock enable                               |
| 9:8  | RTCSEL[1:0]| R/W   | RTC source (00=no clk, 01=LSE, 10=LSI, 11=HSE/128) |
| 2    | LSEBYP     | R/W    | LSE oscillator bypass                          |
| 1    | LSERDY     | R      | LSE ready flag                                 |
| 0    | LSEON      | R/W    | LSE oscillator enable                          |

---

# 2. General-Purpose I/O (GPIO)

> Base: **GPIOA = 0x4001 0800**, GPIOB = 0x4001 0C00, GPIOC = 0x4001 1000, GPIOD = 0x4001 1400, GPIOE = 0x4001 1800, GPIOF = 0x4001 1C00, GPIOG = 0x4001 2000 | RM0008 Chapter 9

## GPIO Register Summary

| Offset | Register   | Reset Value  | Description                     |
|--------|------------|--------------|---------------------------------|
| 0x00   | GPIOx_CRL  | 0x4444 4444  | Port config register low (pins 0-7) |
| 0x04   | GPIOx_CRH  | 0x4444 4444  | Port config register high (pins 8-15) |
| 0x08   | GPIOx_IDR  | 0x0000 XXXX  | Port input data register        |
| 0x0C   | GPIOx_ODR  | 0x0000 0000  | Port output data register       |
| 0x10   | GPIOx_BSRR | 0x0000 0000  | Port bit set/reset register     |
| 0x14   | GPIOx_BRR  | 0x0000 0000  | Port bit reset register         |
| 0x18   | GPIOx_LCKR | 0x0000 0000  | Port configuration lock register|

## GPIOx_CRL/CRH -- Port Configuration (Reset: 0x4444 4444)

Each 4-bit field configures one pin:

| MODE[1:0] | Description                        |
|-----------|------------------------------------|
| 00        | Input mode (reset state)           |
| 01        | Output mode, max 10 MHz            |
| 10        | Output mode, max 2 MHz             |
| 11        | Output mode, max 50 MHz            |

**CNF[1:0] in Input mode (MODE=00):**
| CNF[1:0] | Description                  |
|----------|------------------------------|
| 00       | Analog mode                  |
| 01       | Floating input (reset state) |
| 10       | Input with pull-up/down      |
| 11       | Reserved                     |

**CNF[1:0] in Output mode (MODE>00):**
| CNF[1:0] | Description                       |
|----------|-----------------------------------|
| 00       | General-purpose output push-pull  |
| 01       | General-purpose output open-drain |
| 10       | Alternate function output push-pull |
| 11       | Alternate function output open-drain |

## GPIOx_BSRR (Offset 0x10)

| Bits | Field  | Description          |
|------|--------|----------------------|
| 31:16| BRy    | Reset bit y (y=0..15)|
| 15:0 | BSy    | Set bit y (y=0..15)  |

Note: If both BSx and BRx are set, BSx has priority.

## GPIOx_LCKR (Offset 0x18)

| Bit  | Field | Description                            |
|------|-------|----------------------------------------|
| 16   | LCKK  | Lock key (requires write sequence)     |
| 15:0 | LCKy  | Lock bit y (freezes CRL/CRH bits)      |

Lock sequence: Write 1 -> Write 0 -> Write 1 -> Read 0 -> Read 1

---

## AFIO Registers

> Base: **0x4001 0000** | RM0008 Chapter 9.4

| Offset | Register      | Description                              |
|--------|---------------|------------------------------------------|
| 0x00   | AFIO_EVCR     | Event control register                   |
| 0x04   | AFIO_MAPR     | AF remap and debug I/O configuration     |
| 0x08   | AFIO_EXTICR1  | External interrupt config register 1     |
| 0x0C   | AFIO_EXTICR2  | External interrupt config register 2     |
| 0x10   | AFIO_EXTICR3  | External interrupt config register 3     |
| 0x14   | AFIO_EXTICR4  | External interrupt config register 4     |
| 0x1C   | AFIO_MAPR2    | AF remap and debug I/O config register 2 |

AFIO_EXTICRx registers map EXTI lines 0-15 to specific GPIO ports.

---

# 3. Interrupts and Events (NVIC / EXTI)

> RM0008 Chapter 10

## NVIC -- Nested Vectored Interrupt Controller

The STM32F103 NVIC supports up to 68 maskable interrupt channels with 16 programmable priority levels (4 bits of preemption priority). The interrupt vector table is at address 0x0000 0000 (or 0x0800 0000 after boot).

### Key Interrupt Vectors (STM32F103 Medium-Density)

| Position | Peripheral   | Description                         |
|----------|--------------|-------------------------------------|
| 0        | WWDG         | Window watchdog interrupt           |
| 1        | PVD          | PVD through EXTI line               |
| 2        | TAMPER       | RTC tamper interrupt                |
| 3        | RTC          | RTC global interrupt                |
| 4        | FLASH        | Flash global interrupt              |
| 5        | RCC          | RCC global interrupt                |
| 6        | EXTI0        | EXTI line 0 interrupt               |
| 7        | EXTI1        | EXTI line 1 interrupt               |
| 8        | EXTI2        | EXTI line 2 interrupt               |
| 9        | EXTI3        | EXTI line 3 interrupt               |
| 10       | EXTI4        | EXTI line 4 interrupt               |
| 11       | DMA1_Channel1| DMA1 channel 1 interrupt            |
| 12       | DMA1_Channel2| DMA1 channel 2 interrupt            |
| 13       | DMA1_Channel3| DMA1 channel 3 interrupt            |
| 14       | DMA1_Channel4| DMA1 channel 4 interrupt            |
| 15       | DMA1_Channel5| DMA1 channel 5 interrupt            |
| 16       | DMA1_Channel6| DMA1 channel 6 interrupt            |
| 17       | DMA1_Channel7| DMA1 channel 7 interrupt            |
| 18       | ADC1_2       | ADC1 and ADC2 global interrupt      |
| 19       | USB_HP_CAN_TX| USB high priority / CAN TX          |
| 20       | USB_LP_CAN_RX0| USB low priority / CAN RX0         |
| 21       | CAN_RX1      | CAN RX1 interrupt                   |
| 22       | CAN_SCE      | CAN SCE interrupt                   |
| 23       | EXTI9_5      | EXTI lines 5-9 interrupt            |
| 24       | TIM1_BRK     | TIM1 break interrupt                |
| 25       | TIM1_UP      | TIM1 update interrupt               |
| 26       | TIM1_TRG_COM | TIM1 trigger and commutation        |
| 27       | TIM1_CC      | TIM1 capture compare                |
| 28       | TIM2         | TIM2 global interrupt               |
| 29       | TIM3         | TIM3 global interrupt               |
| 30       | TIM4         | TIM4 global interrupt               |
| 31       | I2C1_EV      | I2C1 event interrupt                |
| 32       | I2C1_ER      | I2C1 error interrupt                |
| 33       | I2C2_EV      | I2C2 event interrupt                |
| 34       | I2C2_ER      | I2C2 error interrupt                |
| 35       | SPI1         | SPI1 global interrupt               |
| 36       | SPI2         | SPI2 global interrupt               |
| 37       | USART1       | USART1 global interrupt             |
| 38       | USART2       | USART2 global interrupt             |
| 39       | USART3       | USART3 global interrupt             |
| 40       | EXTI15_10    | EXTI lines 10-15 interrupt          |
| 41       | RTCAlarm     | RTC alarm through EXTI line         |
| 42       | USBWakeup    | USB wakeup interrupt                |

## EXTI -- External Interrupt/Event Controller

> Base: **0x4001 0400** | RM0008 Chapter 10.3

| Offset | Register    | Reset Value  | Description                          |
|--------|-------------|--------------|--------------------------------------|
| 0x00   | EXTI_IMR    | 0x0000 0000  | Interrupt mask register              |
| 0x04   | EXTI_EMR    | 0x0000 0000  | Event mask register                  |
| 0x08   | EXTI_RTSR   | 0x0000 0000  | Rising trigger selection register    |
| 0x0C   | EXTI_FTSR   | 0x0000 0000  | Falling trigger selection register   |
| 0x10   | EXTI_SWIER  | 0x0000 0000  | Software interrupt event register    |
| 0x14   | EXTI_PR     | 0x0000 XXXX  | Pending register                     |

EXTI manages 20 event lines (0-19). Lines 0-15 are mapped to GPIO pins via AFIO_EXTICRx. Lines 16-19 are fixed to specific events (PVD, RTC alarm, USB, etc.).

---

# 4. Analog-to-Digital Converter (ADC)

> Base: **ADC1 = 0x4001 2400**, ADC2 = 0x4001 2800, ADC3 = 0x4001 3C00 | RM0008 Chapter 11

## ADC Register Summary

| Offset | Register    | Description                                |
|--------|-------------|--------------------------------------------|
| 0x00   | ADC_SR      | Status register                            |
| 0x04   | ADC_CR1     | Control register 1                         |
| 0x08   | ADC_CR2     | Control register 2                         |
| 0x0C   | ADC_SMPR1   | Sample time register 1 (channels 10-17)    |
| 0x10   | ADC_SMPR2   | Sample time register 2 (channels 0-9)      |
| 0x14   | ADC_JOFRx   | Injected channel data offset x (x=1..4)    |
| 0x24   | ADC_HTR     | Watchdog high threshold register           |
| 0x28   | ADC_LTR     | Watchdog low threshold register            |
| 0x2C   | ADC_SQR1    | Regular sequence register 1                |
| 0x30   | ADC_SQR2    | Regular sequence register 2                |
| 0x34   | ADC_SQR3    | Regular sequence register 3                |
| 0x38   | ADC_JSQR    | Injected sequence register                 |
| 0x3C   | ADC_JDRx    | Injected data register x (x=1..4)          |
| 0x4C   | ADC_DR      | Regular data register                      |

## ADC_SR (Offset 0x00, Reset: 0x0000 0000)

| Bit | Field  | Access | Description                      |
|-----|--------|--------|----------------------------------|
| 4   | STRT   | W1C    | Regular channel start flag       |
| 3   | JSTRT  | W1C    | Injected channel start flag      |
| 2   | JEOC   | W1C    | Injected channel end of conversion|
| 1   | EOC    | W1C    | End of conversion                |
| 0   | AWD    | W1C    | Analog watchdog flag             |

## ADC_CR1 (Offset 0x04, Reset: 0x0000 0000)

| Bits  | Field          | Description                                    |
|-------|----------------|------------------------------------------------|
| 23    | AWDEN          | Analog watchdog enable on regular channels     |
| 22    | JAWDEN         | Analog watchdog enable on injected channels    |
| 19:16 | DUALMOD[3:0]   | Dual mode selection                            |
| 15:13 | DISCNUM[2:0]   | Discontinuous mode channel count               |
| 12    | JDISCEN        | Discontinuous mode on injected channels        |
| 11    | DISCEN         | Discontinuous mode on regular channels         |
| 10    | JAUTO          | Automatic injected group conversion            |
| 9     | AWDSGL         | Watchdog on single channel in scan mode        |
| 8     | SCAN           | Scan mode enable                               |
| 7     | JEOCIE         | JEOC interrupt enable                          |
| 6     | AWDIE          | Analog watchdog interrupt enable               |
| 5     | EOCIE          | EOC interrupt enable                           |
| 4:0   | AWDCH[4:0]     | Analog watchdog channel select (0-17)          |

## ADC_CR2 (Offset 0x08, Reset: 0x0000 0000)

| Bits  | Field          | Description                                    |
|-------|----------------|------------------------------------------------|
| 23    | TSVREFE        | Temperature sensor and V_REFINT enable         |
| 22    | SWSTART        | Start conversion of regular channels           |
| 21    | JSWSTART       | Start conversion of injected channels          |
| 20    | EXTTRIG        | External trigger conversion mode (regular)     |
| 19:17 | EXTSEL[2:0]    | External event select for regular group        |
| 15    | JEXTTRIG       | External trigger conversion mode (injected)    |
| 14:12 | JEXTSEL[2:0]   | External event select for injected group       |
| 11    | ALIGN          | Data alignment (0=right, 1=left)               |
| 8     | DMA            | DMA mode enable                                |
| 3     | RSTCAL         | Reset calibration                              |
| 2     | CAL            | A/D calibration start                          |
| 1     | CONT           | Continuous conversion                          |
| 0     | ADON           | A/D converter ON/OFF (write 1 twice to start)  |

## ADC_SMPR1 (Offset 0x0C) and ADC_SMPR2 (Offset 0x10)

Each channel sample time selection (3 bits per channel):

| SMP[2:0] | Sample cycles |
|-----------|---------------|
| 000       | 1.5 cycles    |
| 001       | 7.5 cycles    |
| 010       | 13.5 cycles   |
| 011       | 28.5 cycles   |
| 100       | 41.5 cycles   |
| 101       | 55.5 cycles   |
| 110       | 71.5 cycles   |
| 111       | 239.5 cycles  |

Conversion time: **Tconv = Sampling time + 12.5 cycles**

---

# 5. Digital-to-Analog Converter (DAC)

> Base: **0x4000 7400** | RM0008 Chapter 12

| Offset | Register      | Description                              |
|--------|---------------|------------------------------------------|
| 0x00   | DAC_CR        | DAC control register                     |
| 0x04   | DAC_SWTRIGR   | DAC software trigger register            |
| 0x08   | DAC_DHR12R1   | DAC ch1 12-bit right-aligned data        |
| 0x0C   | DAC_DHR12L1   | DAC ch1 12-bit left-aligned data         |
| 0x10   | DAC_DHR8R1    | DAC ch1 8-bit right-aligned data         |
| 0x14   | DAC_DHR12R2   | DAC ch2 12-bit right-aligned data        |
| 0x18   | DAC_DHR12L2   | DAC ch2 12-bit left-aligned data         |
| 0x1C   | DAC_DHR8R2    | DAC ch2 8-bit right-aligned data         |
| 0x20   | DAC_DHR12RD   | Dual DAC 12-bit right-aligned data       |
| 0x24   | DAC_DHR12LD   | Dual DAC 12-bit left-aligned data        |
| 0x28   | DAC_DHR8RD    | Dual DAC 8-bit right-aligned data        |
| 0x2C   | DAC_DOR1      | DAC channel1 data output                 |
| 0x30   | DAC_DOR2      | DAC channel2 data output                 |

---

# 6. Direct Memory Access (DMA)

> Base: **DMA1 = 0x4002 0000**, DMA2 = 0x4002 0400 | RM0008 Chapter 13

## DMA Register Summary

| Offset | Register    | Description                              |
|--------|-------------|------------------------------------------|
| 0x00   | DMA_ISR     | DMA interrupt status register            |
| 0x04   | DMA_IFCR    | DMA interrupt flag clear register        |
| 0x08   | DMA_CCR1    | DMA channel 1 configuration register     |
| 0x0C   | DMA_CNDTR1  | DMA channel 1 number of data register    |
| 0x10   | DMA_CPAR1   | DMA channel 1 peripheral address reg     |
| 0x14   | DMA_CMAR1   | DMA channel 1 memory address register    |
| ...    | ...         | (same pattern for channels 2..7)         |

## DMA_CCRx -- Channel Configuration (Offset: 0x08 + 0x14*(ch-1))

| Bits | Field        | Description                                 |
|------|--------------|---------------------------------------------|
| 14   | MEM2MEM      | Memory-to-memory mode                       |
| 13:12| PL[1:0]      | Priority level (00=low, 01=med, 10=high, 11=very high) |
| 10   | MSIZE[1:0]   | Memory data size (00=8-bit, 01=16-bit, 10=32-bit) |
| 8    | PSIZE[1:0]   | Peripheral data size (same encoding)        |
| 7    | MINC         | Memory increment mode                       |
| 6    | PINC         | Peripheral increment mode                   |
| 5    | CIRC         | Circular mode enable                        |
| 4    | DIR          | Data transfer direction (0=read from peripheral, 1=read from memory) |
| 3    | TEIE         | Transfer error interrupt enable             |
| 2    | HTIE         | Half transfer interrupt enable              |
| 1    | TCIE         | Transfer complete interrupt enable          |
| 0    | EN           | Channel enable                              |

---

# 7. Advanced-Control Timers (TIM1, TIM8)

> Base: **TIM1 = 0x4001 2C00**, TIM8 = 0x4001 3400 | RM0008 Chapter 14

## TIM1/TIM8 Register Summary

| Offset | Register    | Description                              |
|--------|-------------|------------------------------------------|
| 0x00   | TIMx_CR1    | Control register 1                       |
| 0x04   | TIMx_CR2    | Control register 2                       |
| 0x08   | TIMx_SMCR   | Slave mode control register              |
| 0x0C   | TIMx_DIER   | DMA/Interrupt enable register            |
| 0x10   | TIMx_SR     | Status register                          |
| 0x14   | TIMx_EGR    | Event generation register                |
| 0x18   | TIMx_CCMR1  | Capture/compare mode register 1          |
| 0x1C   | TIMx_CCMR2  | Capture/compare mode register 2          |
| 0x20   | TIMx_CCER   | Capture/compare enable register          |
| 0x24   | TIMx_CNT    | Counter                                  |
| 0x28   | TIMx_PSC    | Prescaler                                |
| 0x2C   | TIMx_ARR    | Auto-reload register                     |
| 0x30   | TIMx_RCR    | Repetition counter register              |
| 0x34   | TIMx_CCR1   | Capture/compare register 1               |
| 0x38   | TIMx_CCR2   | Capture/compare register 2               |
| 0x3C   | TIMx_CCR3   | Capture/compare register 3               |
| 0x40   | TIMx_CCR4   | Capture/compare register 4               |
| 0x44   | TIMx_BDTR   | Break and dead-time register             |
| 0x48   | TIMx_DCR    | DMA control register                     |
| 0x4C   | TIMx_DMAR   | DMA address for full transfer            |

## TIMx_CR1 (Offset 0x00)

| Bits | Field  | Description                                           |
|------|--------|-------------------------------------------------------|
| 7    | CKD[1:0]| Clock division (00=tCK_INT, 01=2x, 10=4x)            |
| 5    | ARPE   | Auto-reload preload enable                            |
| 4    | OPM    | One-pulse mode                                        |
| 3    | URS    | Update request source                                 |
| 2    | UDIS   | Update disable                                        |
| 1    | CEN    | Counter enable                                        |
| 0    | DIR    | Direction (0=up, 1=down -- TIM1 only)                 |

---

# 8. General-Purpose Timers (TIM2-TIM5)

> Base: **TIM2 = 0x4000 0000**, TIM3 = 0x4000 0400, TIM4 = 0x4000 0800, TIM5 = 0x4000 0C00 | RM0008 Chapter 15

Same register structure as TIM1 minus RCR and BDTR:

| Offset | Register    | Description                              |
|--------|-------------|------------------------------------------|
| 0x00   | TIMx_CR1    | Control register 1                       |
| 0x04   | TIMx_CR2    | Control register 2                       |
| 0x08   | TIMx_SMCR   | Slave mode control register              |
| 0x0C   | TIMx_DIER   | DMA/Interrupt enable register            |
| 0x10   | TIMx_SR     | Status register                          |
| 0x14   | TIMx_EGR    | Event generation register                |
| 0x18   | TIMx_CCMR1  | Capture/compare mode register 1          |
| 0x1C   | TIMx_CCMR2  | Capture/compare mode register 2          |
| 0x20   | TIMx_CCER   | Capture/compare enable register          |
| 0x24   | TIMx_CNT    | Counter                                  |
| 0x28   | TIMx_PSC    | Prescaler                                |
| 0x2C   | TIMx_ARR    | Auto-reload register                     |
| 0x34   | TIMx_CCR1   | Capture/compare register 1               |
| 0x38   | TIMx_CCR2   | Capture/compare register 2               |
| 0x3C   | TIMx_CCR3   | Capture/compare register 3               |
| 0x40   | TIMx_CCR4   | Capture/compare register 4               |
| 0x48   | TIMx_DCR    | DMA control register                     |
| 0x4C   | TIMx_DMAR   | DMA address for full transfer            |

## TIMx_PSC -- Prescaler

All timers have a 16-bit prescaler (TIMx_PSC). The counter clock frequency is: **CK_CNT = CK_INT / (PSC[15:0] + 1)**

## Timer Update Frequency

**f_update = CK_CNT / (ARR + 1)** (for basic/general-purpose timers without repetition counter)

For advanced timers with repetition counter: **f_update = CK_CNT / ((ARR + 1) * (RCR + 1))**

---

# 9. Basic Timers (TIM6, TIM7)

> Base: **TIM6 = 0x4000 1000**, TIM7 = 0x4000 1400 | RM0008 Chapter 17

| Offset | Register    | Description                        |
|--------|-------------|------------------------------------|
| 0x00   | TIMx_CR1    | Control register 1                 |
| 0x04   | TIMx_CR2    | Control register 2                 |
| 0x0C   | TIMx_DIER   | DMA/Interrupt enable register      |
| 0x10   | TIMx_SR     | Status register                    |
| 0x14   | TIMx_EGR    | Event generation register          |
| 0x24   | TIMx_CNT    | Counter                            |
| 0x28   | TIMx_PSC    | Prescaler                          |
| 0x2C   | TIMx_ARR    | Auto-reload register               |

---

# 10. Universal Synchronous Asynchronous Receiver Transmitter (USART)

> Base: **USART1 = 0x4001 3800**, USART2 = 0x4000 4400, USART3 = 0x4000 4800, UART4 = 0x4000 4C00, UART5 = 0x4000 5000 | RM0008 Chapter 27

## USART Register Summary

| Offset | Register    | Description                              |
|--------|-------------|------------------------------------------|
| 0x00   | USART_SR    | Status register                          |
| 0x04   | USART_DR    | Data register                            |
| 0x08   | USART_BRR   | Baud rate register                       |
| 0x0C   | USART_CR1   | Control register 1                       |
| 0x10   | USART_CR2   | Control register 2                       |
| 0x14   | USART_CR3   | Control register 3                       |
| 0x18   | USART_GTPR  | Guard time and prescaler register        |

## USART_SR (Offset 0x00, Reset: 0x00C0)

| Bit | Field  | Access | Description                        |
|-----|--------|--------|------------------------------------|
| 9   | CTS    | W1C    | CTS flag                           |
| 8   | LBD    | W1C    | LIN break detection flag           |
| 7   | TXE    | R      | Transmit data register empty       |
| 6   | TC     | R/W1C  | Transmission complete              |
| 5   | RXNE   | R/W1C  | Read data register not empty       |
| 4   | IDLE   | R      | IDLE line detected                 |
| 3   | ORE    | R      | Overrun error                      |
| 2   | NE     | R      | Noise error flag                   |
| 1   | FE     | R      | Framing error                      |
| 0   | PE     | R      | Parity error                       |

## USART_DR (Offset 0x04)

| Bits | Field    | Access | Description                        |
|------|----------|--------|------------------------------------|
| 8:0  | DR[8:0]  | R/W    | Data value (9-bit with M=1)        |

## USART_BRR (Offset 0x08)

| Bits | Field            | Description                              |
|------|------------------|------------------------------------------|
| 15:4 | DIV_Mantissa[11:0]| Mantissa of USARTDIV                     |
| 3:0  | DIV_Fraction[3:0] | Fraction of USARTDIV                     |

Baud rate formula: **Baud = fCK / (16 * USARTDIV)**, where USARTDIV = DIV_Mantissa + DIV_Fraction/16

USART1 clock source: PCLK2 (max 72 MHz). USART2-5 clock source: PCLK1 (max 36 MHz).

## USART_CR1 (Offset 0x0C, Reset: 0x0000)

| Bit | Field    | Description                                       |
|-----|----------|---------------------------------------------------|
| 13  | UE       | USART enable                                      |
| 12  | M        | Word length (0=8-bit, 1=9-bit)                   |
| 11  | WAKE     | Wakeup method (0=idle line, 1=address mark)       |
| 10  | PCE      | Parity control enable                             |
| 9   | PS       | Parity selection (0=even, 1=odd)                  |
| 8   | PEIE     | PE interrupt enable                               |
| 7   | TXEIE    | TXE interrupt enable                              |
| 6   | TCIE     | Transmission complete interrupt enable            |
| 5   | RXNEIE   | RXNE interrupt enable                             |
| 4   | IDLEIE   | IDLE interrupt enable                             |
| 3   | TE       | Transmitter enable                                |
| 2   | RE       | Receiver enable                                   |
| 1   | RWU      | Receiver wakeup (mute mode)                       |
| 0   | SBK      | Send break                                        |

## USART_CR2 (Offset 0x10, Reset: 0x0000)

| Bit  | Field    | Description                                   |
|------|----------|-----------------------------------------------|
| 14   | LINEN    | LIN mode enable                               |
| 13:12| STOP[1:0]| Stop bits (00=1, 01=0.5, 10=2, 11=1.5)       |
| 11   | CLKEN    | Clock pin enable                              |
| 10   | CPOL     | Clock polarity                                |
| 9    | CPHA     | Clock phase                                   |
| 8    | LBCL     | Last bit clock pulse                          |
| 6    | LBDIE    | LIN break detection interrupt enable          |
| 5    | LBDL     | LIN break detection length                    |
| 3:0  | ADD[3:0] | Address of the USART node                     |

## USART_CR3 (Offset 0x14, Reset: 0x0000)

| Bit | Field  | Description                                    |
|-----|--------|------------------------------------------------|
| 10  | CTSIE  | CTS interrupt enable                           |
| 9   | CTSE   | CTS enable                                     |
| 8   | RTSE   | RTS enable                                     |
| 7   | DMAT   | DMA enable transmitter                         |
| 6   | DMAR   | DMA enable receiver                            |
| 5   | SCEN   | Smartcard mode enable                          |
| 4   | NACK   | Smartcard NACK enable                          |
| 3   | HDSEL  | Half-duplex selection                          |
| 2   | IRLP   | IrDA low-power                                 |
| 1   | IREN   | IrDA mode enable                               |
| 0   | EIE    | Error interrupt enable                         |

---

# 11. Serial Peripheral Interface (SPI)

> Base: **SPI1 = 0x4001 3000**, SPI2 = 0x4000 3800, SPI3 = 0x4000 3C00 | RM0008 Chapter 25

## SPI Register Summary

| Offset | Register    | Description                              |
|--------|-------------|------------------------------------------|
| 0x00   | SPI_CR1     | SPI control register 1                   |
| 0x04   | SPI_CR2     | SPI control register 2                   |
| 0x08   | SPI_SR      | SPI status register                      |
| 0x0C   | SPI_DR      | SPI data register                        |
| 0x10   | SPI_CRCPR   | SPI CRC polynomial register              |
| 0x14   | SPI_RXCRCR  | SPI RX CRC register                      |
| 0x18   | SPI_TXCRCR  | SPI TX CRC register                      |
| 0x1C   | SPI_I2SCFGR | I2S configuration register               |
| 0x20   | SPI_I2SPR   | I2S prescaler register                   |

## SPI_CR1 (Offset 0x00, Reset: 0x0000)

| Bits| Field      | Description                                          |
|-----|------------|------------------------------------------------------|
| 15  | BIDIMODE   | Bidirectional data mode enable                       |
| 14  | BIDIOE     | Output enable in bidirectional mode                  |
| 13  | CRCEN      | Hardware CRC calculation enable                      |
| 12  | CRCNEXT    | CRC transfer next                                    |
| 11  | DFF        | Data frame format (0=8-bit, 1=16-bit)               |
| 10  | RXONLY     | Receive only                                         |
| 9   | SSM        | Software slave management                            |
| 8   | SSI        | Internal slave select                                |
| 7   | LSBFIRST   | Frame format (0=MSB first, 1=LSB first)              |
| 6   | SPE        | SPI enable                                           |
| 5:3 | BR[2:0]    | Baud rate control (000=fPCLK/2, 001=/4, ..., 111=/256)|
| 2   | MSTR       | Master selection                                     |
| 1   | CPOL       | Clock polarity                                       |
| 0   | CPHA       | Clock phase                                          |

**Maximum SPI clock:** fPCLK/2 (PCLK1 max 36 MHz for SPI2/3, PCLK2 max 72 MHz for SPI1)

## SPI_CR2 (Offset 0x04, Reset: 0x0000)

| Bit| Field    | Description                           |
|----|----------|---------------------------------------|
| 7  | TXEIE    | Tx buffer empty interrupt enable      |
| 6  | RXNEIE   | RX buffer not empty interrupt enable  |
| 5  | ERRIE    | Error interrupt enable                |
| 2  | SSOE     | SS output enable                      |
| 1  | TXDMAEN  | Tx buffer DMA enable                  |
| 0  | RXDMAEN  | Rx buffer DMA enable                  |

## SPI_SR (Offset 0x08, Reset: 0x0002)

| Bit| Field   | Description                           |
|----|---------|---------------------------------------|
| 7  | BSY     | Busy flag                             |
| 6  | OVR     | Overrun flag                          |
| 5  | MODF    | Mode fault                            |
| 4  | CRCERR  | CRC error flag                        |
| 1  | TXE     | Transmit buffer empty                 |
| 0  | RXNE    | Receive buffer not empty              |

## SPI_DR (Offset 0x0C, Reset: 0x0000)

| Bits    | Field      | Description                   |
|---------|------------|-------------------------------|
| 15:0    | DR[15:0]   | Data register (8 or 16-bit)   |

---

# 12. Inter-Integrated Circuit (I2C)

> Base: **I2C1 = 0x4000 5400**, I2C2 = 0x4000 5800 | RM0008 Chapter 26

## I2C Register Summary

| Offset | Register    | Description                              |
|--------|-------------|------------------------------------------|
| 0x00   | I2C_CR1     | Control register 1                       |
| 0x04   | I2C_CR2     | Control register 2                       |
| 0x08   | I2C_OAR1    | Own address register 1                   |
| 0x0C   | I2C_OAR2    | Own address register 2                   |
| 0x10   | I2C_DR      | Data register                            |
| 0x14   | I2C_SR1     | Status register 1                        |
| 0x18   | I2C_SR2     | Status register 2                        |
| 0x1C   | I2C_CCR     | Clock control register                   |
| 0x20   | I2C_TRISE   | TRISE register                           |

## I2C_CR1 (Offset 0x00)

| Bit | Field  | Description                                   |
|-----|--------|-----------------------------------------------|
| 15  | SWRST  | Software reset                                |
| 10  | POS    | Acknowledge position                          |
| 9   | ACK    | Acknowledge enable                            |
| 8   | STOP   | Stop generation                               |
| 7   | START  | Start generation                              |
| 6   | NOSTRETCH| Clock stretching disable (SMBus)             |
| 5   | ENGC   | General call enable                           |
| 4   | ENPEC  | PEC enable                                    |
| 3   | ENARP  | ARP enable                                    |
| 1   | SMBUS  | SMBus mode                                    |
| 0   | PE     | Peripheral enable                             |

## I2C_CR2 (Offset 0x04)

| Bits | Field      | Description                              |
|------|------------|------------------------------------------|
| 5:0  | FREQ[5:0]  | Peripheral clock frequency (MHz)         |

## I2C_CCR (Offset 0x1C)

| Bits| Field    | Description                                   |
|-----|----------|-----------------------------------------------|
| 15  | F/S      | Fast/Standard mode (0=standard, 1=fast)       |
| 14  | DUTY     | Fast mode duty cycle (0=Tlow/Thigh=2, 1=16/9)|
| 11:0| CCR[11:0]| Clock control value                           |

Standard mode: **T_SCL = 2 * CCR * TPCLK1** (CCR must be > 4)
Fast mode DUTY=0: **T_SCL = 2 * CCR * TPCLK1** (CCR must be > 1)
Fast mode DUTY=1: **T_SCL = (CCR * 25/9) * TPCLK1**

---

# 13. Power Control (PWR)

> Base: **0x4000 7000** | RM0008 Chapter 5

| Offset | Register  | Reset Value | Description                            |
|--------|-----------|-------------|----------------------------------------|
| 0x00   | PWR_CR    | 0x0000 0000 | Power control register                 |
| 0x04   | PWR_CSR   | 0x0000 0000 | Power control/status register          |

## PWR_CR (Offset 0x00)

| Bit | Field  | Description                                          |
|-----|--------|------------------------------------------------------|
| 8   | DBP    | Disable backup domain write protection               |
| 7:5 | PLS    | PVD level selection                                  |
| 4   | PVDE   | PVD enable                                           |
| 3   | CSBF   | Clear standby flag                                   |
| 2   | CWUF   | Clear wakeup flag                                    |
| 1   | PDDS   | Power down deepsleep (0=Stop, 1=Standby)             |
| 0   | LPDS   | Low-power deepsleep (regulator in low-power in Stop) |

## PWR_CSR (Offset 0x04)

| Bit | Field | Description                                    |
|-----|-------|------------------------------------------------|
| 8   | EWUP  | Enable WKUP pin                                |
| 2   | PVDO  | PVD output                                     |
| 1   | SBF   | Standby flag                                   |
| 0   | WUF   | Wakeup flag                                    |

---

# 14. Other Peripherals (Summary)

### Real-Time Clock (RTC) -- Base: 0x4000 2800

| Offset | Register    | Description                        |
|--------|-------------|------------------------------------|
| 0x00   | RTC_CRH     | RTC control register high          |
| 0x04   | RTC_CRL     | RTC control register low           |
| 0x08   | RTC_PRLH    | RTC prescaler load high            |
| 0x0C   | RTC_PRLL    | RTC prescaler load low             |
| 0x10   | RTC_DIVH    | RTC prescaler divider high         |
| 0x14   | RTC_DIVL    | RTC prescaler divider low          |
| 0x18   | RTC_CNTH    | RTC counter high                   |
| 0x1C   | RTC_CNTL    | RTC counter low                    |
| 0x20   | RTC_ALRH    | RTC alarm high                     |
| 0x24   | RTC_ALRL    | RTC alarm low                      |

### Independent Watchdog (IWDG) -- Base: 0x4000 3000

| Offset | Register  | Description                        |
|--------|-----------|------------------------------------|
| 0x00   | IWDG_KR   | Key register (write 0xAAAA to reload, 0xCCCC to enable) |
| 0x04   | IWDG_PR   | Prescaler register                 |
| 0x08   | IWDG_RLR  | Reload register                    |
| 0x0C   | IWDG_SR   | Status register                    |

Timeout: **t_IWDG = (RLR * 4 * 2^PR) / 40 kHz** (with LSI ~40 kHz)

### Window Watchdog (WWDG) -- Base: 0x4000 2C00

| Offset | Register  | Description                        |
|--------|-----------|------------------------------------|
| 0x00   | WWDG_CR   | Control register (T[6:0] = 7-bit counter) |
| 0x04   | WWDG_CFR  | Configuration register (WDGTB prescaler, W[6:0] window) |
| 0x08   | WWDG_SR   | Status register (EWIF)             |

Timeout: **t_WWDG = (T[6:0] + 1) * 4096 * 2^WDGTB / PCLK1**

### CRC -- Base: 0x4002 3000

| Offset | Register  | Description                        |
|--------|-----------|------------------------------------|
| 0x00   | CRC_DR    | Data register (32-bit)             |
| 0x04   | CRC_IDR   | Independent data register (8-bit)  |
| 0x08   | CRC_CR    | Control register (bit 0 = RESET)   |

Polynomial: **0x4C11DB7** (CRC-32 Ethernet)

### bxCAN -- Base: CAN1 = 0x4000 6400, CAN2 = 0x4000 6800

| Offset | Register    | Description                        |
|--------|-------------|------------------------------------|
| 0x00   | CAN_MCR     | Master control register            |
| 0x04   | CAN_MSR     | Master status register             |
| 0x08   | CAN_TSR     | Transmit status register           |
| 0x0C   | CAN_RF0R    | Receive FIFO 0 register            |
| 0x10   | CAN_RF1R    | Receive FIFO 1 register            |
| 0x14   | CAN_IER     | Interrupt enable register          |
| 0x18   | CAN_ESR     | Error status register              |
| 0x1C   | CAN_BTR     | Bit timing register                |

### USB Device FS -- Base: 0x4000 5C00

Dedicated USB register set for packet buffer management, endpoint control, and status.

### Flash Memory Interface -- Base: 0x4002 2000

| Offset | Register    | Description                        |
|--------|-------------|------------------------------------|
| 0x00   | FLASH_ACR   | Flash access control register      |
| 0x04   | FLASH_KEYR  | Flash key register                 |
| 0x08   | FLASH_OPTKEYR| Flash option key register         |
| 0x0C   | FLASH_SR    | Flash status register              |
| 0x10   | FLASH_CR    | Flash control register             |
| 0x14   | FLASH_AR    | Flash address register             |
| 0x1C   | FLASH_OBR   | Flash option byte register         |
| 0x20   | FLASH_WRPR  | Flash write protection register    |

FLASH_ACR (Offset 0x00, Reset: 0x0000 0030):
| Bit | Field   | Description                                     |
|-----|---------|-------------------------------------------------|
| 5   | PRFTBS  | Prefetch buffer status (RO)                     |
| 4   | PRFTBE  | Prefetch buffer enable                          |
| 3   | HLFCYA  | Flash half cycle access enable                  |
| 2:0 | LATENCY | Wait states (000=0WS if SYSCLK<=24MHz, 001=1WS if 24<SYSCLK<=48MHz, 010=2WS if 48<SYSCLK<=72MHz) |

### Backup Registers (BKP) -- Base: 0x4000 6C00

| Offset     | Register    | Description                        |
|------------|-------------|------------------------------------|
| 0x04-0x28, 0x40-0xBC | BKP_DRx | Backup data registers (x=1..42, 16-bit each) |
| 0x2C       | BKP_RTCCR   | RTC clock calibration register     |
| 0x30       | BKP_CR      | Backup control register (TPE, TPAL)|
| 0x34       | BKP_CSR     | Backup control/status register     |

### SDIO -- Base: 0x4001 8000

Full SD/SDIO/MMC host controller interface with command, data, and clock control registers.

### FSMC -- Base: 0xA000 0000

Flexible static memory controller supporting NOR/NAND/PSRAM/SRAM with up to 4 chip selects and programmable timing.
