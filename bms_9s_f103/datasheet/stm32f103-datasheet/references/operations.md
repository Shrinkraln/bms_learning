# stm32f103 Operation Sequences & State Machines

> **PURPOSE:** Canonical sequences for system initialization, boot, Flash programming, low-power entry/exit, and peripheral configuration.

---

## 1. Boot Configuration

From RM0008 Section 3.4:

### Boot Mode Selection

| BOOT1 | BOOT0 | Boot Mode       | Aliasing                              |
|-------|-------|-----------------|---------------------------------------|
| x     | 0     | Main Flash      | Flash at 0x0800 0000 aliased to 0x0   |
| 0     | 1     | System Memory   | Bootloader at 0x1FFF F000 aliased to 0x0 |
| 1     | 1     | Embedded SRAM   | SRAM at 0x2000 0000 (vector table must be relocated) |

- BOOT pins latched on 4th rising edge of SYSCLK after reset
- Pins resampled when exiting Standby mode
- Boot from SRAM requires vector table relocation in code

### Boot Sequence (from Reset)

```
Step 1: BOOT pins sampled on 4th SYSCLK rising edge
Step 2: CPU fetches top-of-stack from address 0x0000 0000
Step 3: CPU starts code execution at address 0x0000 0004
```

---

## 2. System Initialization Sequence

This is the canonical startup sequence for STM32F103:

```
Step 1: Configure Flash memory
  - Set FLASH_ACR (0x4002 2000) LATENCY bits based on target SYSCLK
  - Enable PRFTBE if AHB prescaler != 1

Step 2: Configure HSI (default after reset, 8 MHz)
  - Wait for HSIRDY in RCC_CR

Step 3: Configure HSE (optional, for higher frequencies)
  - Set HSEON in RCC_CR bit 16
  - Wait for HSERDY in RCC_CR bit 17

Step 4: Configure PLL (optional)
  - Set PLLSRC to HSE or HSI/2 in RCC_CFGR bit 16
  - Set PLLMUL in RCC_CFGR bits 21:18 (output must not exceed 72 MHz)
  - Set PLLXTPRE in RCC_CFGR bit 17 if HSE/2 needed
  - Enable PLL: set PLLON in RCC_CR bit 24
  - Wait for PLLRDY in RCC_CR bit 25

Step 5: Configure AHB, APB1, APB2 prescalers
  - Set HPRE in RCC_CFGR bits 7:4 (AHB prescaler)
  - Set PPRE1 in RCC_CFGR bits 10:8 (APB1, max 36 MHz)
  - Set PPRE2 in RCC_CFGR bits 13:11 (APB2)

Step 6: Switch system clock
  - Set SW in RCC_CFGR bits 1:0 (00=HSI, 01=HSE, 10=PLL)
  - Wait for SWS to reflect the selected source (RCC_CFGR bits 3:2)

Step 7: Configure peripheral clocks (optional)
  - Set clock enable bits in RCC_AHBENR, RCC_APB1ENR, RCC_APB2ENR
  - Note: Reset state disables all peripheral clocks except Flash and SRAM

Step 8: Update FLASH_ACR latency if SYSCLK changed
  - Verify LATENCY bits match the final SYSCLK
```

### Example: 72 MHz System Clock from 8 MHz HSE

```
RCC_CR   |= 0x00010000;              // HSEON
while (!(RCC_CR & 0x00020000));      // Wait for HSERDY
FLASH_ACR = 0x00000012;              // 2 WS, Prefetch enable
RCC_CFGR  = 0x001D0400;              // PLLSRC=HSE, PLLMUL=x9, APB1=/2, APB2=/1, AHB=/1
RCC_CR   |= 0x01000000;              // PLLON
while (!(RCC_CR & 0x02000000));      // Wait for PLLRDY
RCC_CFGR |= 0x00000002;              // SW=PLL
while ((RCC_CFGR & 0x0000000C) != 0x00000008); // Wait for SWS=PLL
```

---

## 3. Flash Programming / Erase

From RM0008 Chapter 3 and PM0075:

### Flash Programming (16-bit half-word)

```
Step 1: Check BSY bit in FLASH_SR (must be 0)
Step 2: Write key sequence to FLASH_KEYR (0x4002 2004)
  - Write 0x45670123
  - Write 0xCDEF89AB
Step 3: Set PG bit in FLASH_CR (0x4002 2010)
Step 4: Write half-word (16-bit) to target Flash address
Step 5: Wait for BSY bit in FLASH_SR to clear
Step 6: Check EOP flag in FLASH_SR
Step 7: Clear PG bit in FLASH_CR
```

### Flash Page Erase

```
Step 1: Check BSY bit in FLASH_SR (must be 0)
Step 2: Write key sequence to FLASH_KEYR
  - Write 0x45670123
  - Write 0xCDEF89AB
Step 3: Set PER bit in FLASH_CR
Step 4: Write page address to FLASH_AR (0x4002 2014)
Step 5: Set STRT bit in FLASH_CR
Step 6: Wait for BSY bit in FLASH_SR to clear
Step 7: Check EOP flag
Step 8: Clear PER bit in FLASH_CR
```

### Flash Mass Erase

```
Step 1: Check BSY bit in FLASH_SR (must be 0)
Step 2: Write key sequence to FLASH_KEYR
Step 3: Set MER bit in FLASH_CR
Step 4: Set STRT bit in FLASH_CR
Step 5: Wait for BSY bit to clear
Step 6: Clear MER bit in FLASH_CR
```

---

## 4. Peripheral Initialization Sequences

### GPIO Initialization

```
Step 1: Enable GPIO clock in RCC_APB2ENR
  - Set IOPxEN bit for the port (x = A..G)
Step 2: Configure pin mode in GPIOx_CRL or GPIOx_CRH
  - MODEy[1:0]: Input (00), Output 10MHz (01), Output 2MHz (10), Output 50MHz (11)
  - CNFy[1:0]: See configuration table
Step 3: For output, set initial level in GPIOx_BSRR or GPIOx_BRR
Step 4: For alternate functions, configure AFIO_MAPR if remapping needed
  - Enable AFIO clock: set AFIOEN in RCC_APB2ENR bit 0
```

### USART Initialization

```
Step 1: Enable USART clock in RCC_APB2ENR (USART1) or RCC_APB1ENR (USART2/3/4/5)
Step 2: Configure TX pin as alternate function push-pull output
Step 3: Configure RX pin as floating input or input with pull-up
Step 4: Reset USART: set USARTxRST in RCC_APB2RSTR or RCC_APB1RSTR, then clear it
Step 5: Set UE bit in USART_CR1 (enable USART)
Step 6: Configure word length (M bit in USART_CR1)
Step 7: Configure stop bits (STOP bits in USART_CR2)
Step 8: Set baud rate in USART_BRR
Step 9: Set TE bit in USART_CR1 (transmitter enable) - sends idle frame
Step 10: Set RE bit in USART_CR1 (receiver enable)
Step 11: Write data to USART_DR to transmit; read USART_DR to receive
```

### SPI Initialization (Master Mode)

```
Step 1: Enable SPI clock in RCC_APB2ENR (SPI1) or RCC_APB1ENR (SPI2/3)
Step 2: Configure SCK, MOSI pins as alternate function push-pull
Step 3: Configure MISO pin as floating input
Step 4: Configure NSS pin as appropriate (or use SSM/SSI for software NSS)
Step 5: Reset SPI: set SPInRST, then clear it
Step 6: Configure SPI_CR1 (all at once or before enabling):
  - CPOL, CPHA (clock polarity and phase)
  - BR[2:0] (baud rate divider)
  - MSTR = 1 (master mode)
  - DFF (8 or 16-bit data)
  - LSBFIRST (MSB or LSB first)
  - SSM=1, SSI=1 if using software NSS
Step 7: Set SPE bit in SPI_CR1 (enable SPI)
Step 8: Write data to SPI_DR to start transmission (master mode)
Step 9: Wait for TXE to write next data; wait for RXNE to read received data
```

### I2C Initialization

```
Step 1: Enable I2C clock in RCC_APB1ENR
Step 2: Configure SCL and SDA pins as alternate function open-drain
Step 3: Reset I2C: set I2CxRST, then clear it
Step 4: Set FREQ[5:0] in I2C_CR2 to match PCLK1 frequency (MHz)
Step 5: Configure I2C_CCR:
  - F/S bit (standard or fast mode)
  - DUTY bit (fast mode duty cycle)
  - CCR[11:0] (clock control value for desired SCL frequency)
Step 6: Configure I2C_TRISE for max rise time
Step 7: Set PE bit in I2C_CR1 (enable I2C)
```

### ADC Initialization (Single Conversion)

```
Step 1: Enable ADC clock in RCC_APB2ENR (ADC1EN, ADC2EN, or ADC3EN)
Step 2: Configure ADC prescaler in RCC_CFGR (ADCPRE bits)
Step 3: Configure ADC channel as analog input (GPIO CNF=00, MODE=00)
Step 4: Reset ADC: set ADCxRST, then clear it
Step 5: Configure ADC_CR1:
  - SCAN bit (for multiple channels)
  - DISCEN (for discontinuous mode)
  - AWDCH (analog watchdog channel)
Step 6: Configure ADC_CR2:
  - CONT (0=single, 1=continuous)
  - ALIGN (0=right, 1=left)
  - EXTSEL (trigger source)
  - EXTTRIG (enable external trigger)
Step 7: Configure sample time in ADC_SMPR1/2
Step 8: Configure sequence in ADC_SQR1/2/3 (regular) or ADC_JSQR (injected)
Step 9: Set ADON bit in ADC_CR2 to power up ADC
Step 10: Wait for stabilization time (t_STAB)
Step 11: Set ADON bit again (or use SWSTART) to start conversion
Step 12: Wait for EOC bit in ADC_SR, read ADC_DR
```

### Timer Initialization (PWM Mode)

```
Step 1: Enable timer clock in RCC_APBxENR
Step 2: Configure PWM pin as alternate function push-pull
Step 3: Reset timer: set TIMxRST, then clear it
Step 4: Configure TIMx_ARR (auto-reload = period)
Step 5: Configure TIMx_PSC (prescaler for desired timer clock)
Step 6: Configure TIMx_CCMRx (output mode PWM1 or PWM2)
  - OCxM bits = 110 (PWM1) or 111 (PWM2)
  - OCxPE = 1 (preload enable)
Step 7: Configure TIMx_CCER (enable capture/compare output)
  - CCxE = 1
  - CCxP for polarity
Step 8: Configure TIMx_CR1:
  - ARPE = 1 (auto-reload preload)
  - CKD for clock division
Step 9: Set TIMx_CCRx (duty cycle)
Step 10: Set CEN bit in TIMx_CR1 (counter enable)
```

### DMA Initialization

```
Step 1: Enable DMA clock in RCC_AHBENR (DMA1EN or DMA2EN)
Step 2: Configure DMA_CCRx for the channel:
  - DIR (read from peripheral or memory)
  - CIRC (circular mode if needed)
  - PINC, MINC (increment modes)
  - PSIZE, MSIZE (data sizes)
  - PL[1:0] (priority)
  - TCIE, HTIE, TEIE (interrupt enables)
Step 3: Set DMA_CPARx (peripheral address)
Step 4: Set DMA_CMARx (memory address)
Step 5: Set DMA_CNDTRx (number of data items to transfer)
Step 6: Enable peripheral DMA request (in peripheral's control register)
Step 7: Set EN bit in DMA_CCRx (channel enable)
```

---

## 5. Low-Power Mode Entry/Exit Sequences

From RM0008 Chapter 5 (PWR) and Cortex-M3 System Control Register:

### Sleep Mode

```
Entry (Sleep-now):
  - Clear SLEEPDEEP in Cortex-M3 System Control Register
  - Clear SLEEPONEXIT
  - Execute WFI or WFE instruction

Entry (Sleep-on-exit):
  - Clear SLEEPDEEP
  - Set SLEEPONEXIT
  - Execute WFI instruction (will enter sleep after ISR exit)

Wakeup:
  - WFI: any interrupt from NVIC
  - WFE: any event
  - Wakeup latency: none
```

### Stop Mode

```
Entry:
  - Set SLEEPDEEP in Cortex-M3 System Control Register
  - Clear PDDS bit in PWR_CR (select Stop mode)
  - Configure LPDS bit in PWR_CR for regulator mode
  - Clear all EXTI Line pending bits (EXTI_PR)
  - Clear peripheral interrupt pending bits
  - Clear RTC Alarm flag
  - Execute WFI or WFE

Wakeup:
  - WFI: Any EXTI line configured in interrupt mode (NVIC enabled)
  - WFE: Any EXTI line configured in event mode
  - HSI RC selected as system clock after wakeup
  - Wakeup latency: HSI RC startup + regulator wakeup
```

### Standby Mode

```
Entry:
  - Set SLEEPDEEP in Cortex-M3 System Control Register
  - Set PDDS bit in PWR_CR (select Standby mode)
  - Clear WUF bit in PWR_CSR
  - Ensure no interrupt or event pending
  - Execute WFI or WFE

Wakeup:
  - WKUP pin rising edge
  - RTC alarm event rising edge
  - External reset on NRST pin
  - IWDG reset
  - Full reset sequence (boot pins resampled, program restarts)
```

### Auto-Wakeup from Stop/Standby using RTC

```
Step 1: Configure RTC clock source in RCC_BDCR (RTCSEL bits)
Step 2: Enable RTC clock in RCC_BDCR (RTCEN bit)
Step 3: Configure RTC prescaler and counter
Step 4: Configure RTC alarm value
Step 5: For Stop mode: configure EXTI Line 17 rising edge
Step 6: Enter Stop or Standby mode
Step 7: RTC alarm wakes device at programmed time
```

---

## 6. USB PLL Configuration Sequence

```
Step 1: Configure PLL for 72 MHz (if not already):
  - HSE (8 MHz) * PLLMUL (x9) = 72 MHz
Step 2: Set USBPRE bit in RCC_CFGR bit 22:
  - USBPRE=0: PLL/1.5 = 48 MHz (with PLL=72 MHz)
Step 3: Enable USB clock in RCC_APB1ENR bit 23 (USBEN)
```

**Alternative (48 MHz PLL):** PLLCLK = 48 MHz -> USBPRE=1 (no division)

---

## 7. Clock Source Switch Sequence

### Switching from HSI to PLL

```
Step 1: Configure PLL multiplication factor (PLLMUL in RCC_CFGR)
Step 2: Select PLL source (PLLSRC in RCC_CFGR)
Step 3: Enable PLL (PLLON in RCC_CR)
Step 4: Wait for PLLRDY
Step 5: Check Flash latency is appropriate for new frequency
Step 6: Set SW bits to 10 (PLL selected as system clock)
Step 7: Wait for SWS to read 10
```

### Switching from HSI to HSE

```
Step 1: Set HSEON in RCC_CR
Step 2: Wait for HSERDY
Step 3: Set SW bits to 01 (HSE selected)
Step 4: Wait for SWS to read 01
```

---

## 8. Reset Sources

From RM0008 Section 7.1:

| Reset Type     | Source                       | Effect on RCC_CSR     |
|----------------|------------------------------|------------------------|
| Power Reset    | POR/PDR pin                  | All reset flags set    |
| System Reset   | NRST pin, watchdog, software | SYSRESETREQ in NVIC   |
| Backup Domain  | BDRST bit, VDD/BAT fail      | Only backup domain     |

Reset flags in RCC_CSR (0x4002 1024):

| Bit | Flag       | Description                   |
|-----|------------|-------------------------------|
| 31  | LPWRRSTF   | Low-power management reset    |
| 30  | WWDGRSTF   | Window watchdog reset         |
| 29  | IWDGRSTF   | Independent watchdog reset    |
| 28  | SFTRSTF    | Software reset                |
| 27  | PORRSTF    | POR/PDR reset                 |
| 26  | PINRSTF    | NRST pin reset                |

Clear reset flags by setting RMVF bit (bit 24) in RCC_CSR.
