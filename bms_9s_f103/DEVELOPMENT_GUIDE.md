# BMS 9串 开发流程指南

> **目标**: 使用 CubeMX + CMake + VSCode，基于 STM32F103C8T6 + BQ76940，从零构建 9串 BMS 固件。
> **参考项目**: `datasheet/BQ76940发货资料 20220817/3.程序/1.BQ76940（程序加CAN20210915）改温度探头/BMS_s940/`
> **原则**: 引脚配置和控制逻辑与参考项目完全一致。
> **用法**: 按顺序执行以下步骤。每一步都标注了参考文件、要创建的文件、关键要点和验证方法。

---

## 总流程

```
准备阶段（一次性）
├─ Step 1: 安装工具链
├─ Step 2: CubeMX 创建项目 + 引脚/外设/时钟配置
├─ Step 3: 获取 HAL/CMSIS 库文件
├─ Step 4: 编写 CMakeLists.txt + 链接脚本
├─ Step 5: 创建目录结构 + VSCode 配置
└─ Step 6: 验证空项目能编译通过

编码阶段（按依赖顺序，每步写完即可编译验证）
├─ Step 7:  Core 层 — main.h, stm32f1xx_hal_conf.h, stm32f1xx_it.c
├─ Step 8:  systick + led（最基础模块，验证烧录和LED闪烁）
├─ Step 9:  io_ctrl（GPIO 控制引脚）
├─ Step 10: i2c_sw（软件 I2C + CRC8）
├─ Step 11: usart_drv（串口驱动，后续调试全靠它）
├─ Step 12: bq76940 驱动（分6个子步，核心模块）
├─ Step 13: can_drv（CAN 通信）
├─ Step 14: timer + wdg（定时器 + 看门狗）
├─ Step 15: App 层 — protection, data_report, bms_app
├─ Step 16: main.c 整合所有模块
└─ [可选] FreeRTOS 集成 → 参见 附录D

验证阶段
├─ Step 17: 编译 + 烧录
└─ Step 18: 14 项功能验证清单
```

---

## 准备阶段

### Step 1: 安装工具链

| 工具 | 版本 | 安装方式 | 验证命令 |
|------|------|---------|---------|
| ARM GCC | 10.3+ | [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain) | `arm-none-eabi-gcc --version` |
| CMake | ≥3.16 | `winget install CMake` | `cmake --version` |
| STM32CubeMX | ≥6.0 | [ST官网](https://www.st.com/stm32cubemx) | 启动 CubeMX GUI |
| VSCode | latest | [code.visualstudio.com](https://code.visualstudio.com) | `code --version` |
| OpenOCD | ≥0.11 | `winget install openocd` | `openocd --version` |

**VSCode 插件**（`.vscode/extensions.json`）:
```json
{"recommendations": ["ms-vscode.cpptools","ms-vscode.cmake-tools","marus25.cortex-debug","dan-c-underwood.arm","twxs.cmake"]}
```

---

### Step 2: CubeMX 创建项目

```
1. CubeMX → File → New Project → 搜索 STM32F103C8T6 → Start Project
2. Project Manager:
   - Project Name: bms_9s_f103
   - Project Location: D:\coding_codes\bms_learning\
   - Toolchain: Makefile
   - ☑ Generate peripheral initialization as a pair of .c/.h files
```

#### 2.1 引脚配置（逐项核对，≡ 参考项目）

**GPIOA:**
| 引脚 | 功能 | 标签 | CubeMX 设置 |
|------|------|------|-----------|
| PA0 | QB 电源控制 | MCU_KZ_QB_POWER | GPIO_Output, 初始 Low |
| PA1 | D 电源控制 | MCU_D_POWER | GPIO_Output, 初始 Low |
| PA2 | HMI 串口屏 TX | USART2_TX | USART2_TX |
| PA3 | HMI 串口屏 RX | USART2_RX | USART2_RX |
| PA8 | BQ76940 唤醒 | MCU_WAKE_BQ | GPIO_Output, 初始 Low |
| PA9 | QT上位机 TX | USART1_TX | USART1_TX |
| PA10 | QT上位机 RX | USART1_RX | USART1_RX |
| PA11 | CAN RX | CAN1_RX | CAN1_RX |
| PA12 | CAN TX (⚠️ 见注) | CAN1_TX | CAN1_TX |
| PA13 | SWD 数据 | SWDIO | SYS_SWDIO |
| PA14 | SWD 时钟 | SWCLK | SYS_SWCLK |
| PA15 | 备用 (JTAG禁用后) | - | GPIO_Output |

**GPIOB:**
| 引脚 | 功能 | 标签 | CubeMX 设置 |
|------|------|------|-----------|
| PB0 | LED2 | LED2 | GPIO_Output |
| PB1 | 唤醒状态 | STA_WAKE | GPIO_Output, 初始 Low |
| PB2 | BQ76940 Alert | MCU_BQ1_ART | GPIO_Output, 初始 Low |
| PB3 | 通用 IO | PB3 | GPIO_Output (⚠️ 需关JTAG) |
| PB4 | 通用 IO | PB4 | GPIO_Output (⚠️ 需关JTAG) |
| PB5 | 通用 IO | PB5 | GPIO_Output |
| PB8 | 软件 I2C SCL | I2C1_SW_SCL | GPIO_Output, **开漏** |
| PB9 | 软件 I2C SDA | I2C1_SW_SDA | GPIO_Output, **开漏** |
| PB12 | 状态指示 | MCU_IN_STA1 | GPIO_Output, 初始 High |
| PB13 | SPI SCK | SPI2_SCK | SPI2_SCK (可选) |
| PB14 | SPI MISO | SPI2_MISO | SPI2_MISO (可选) |
| PB15 | SPI MOSI | SPI2_MOSI | SPI2_MOSI (可选) |

**GPIOC:**
| 引脚 | 功能 | 标签 | CubeMX 设置 |
|------|------|------|-----------|
| PC13 | LED1 | LED1 | GPIO_Output |

> ⚠️ **PA12 冲突**: 参考项目中 PA12 同时被 IO_CTRL.h (MCU_KZ_P2) 和 can.c (CAN_TX) 配置。CAN_TX 后初始化、覆盖前者。如需同时用 CAN 和 P2，将 P2 改到 PA15。

#### 2.2 外设参数

| 外设 | 关键参数 |
|------|---------|
| **USART1** | 115200, 8N1, RXNE 中断使能 |
| **USART2** | 9600, 8N1 |
| **CAN1** | Prescaler=4, BS1=9, BS2=8, SJW=1 → 500Kbps |
| **TIM2** | Prescaler=7199, Period=999 → 100ms, 中断使能 |
| **IWDG** | Prescaler=256, Reload=1250 → ~4s |
| **SPI2** (可选) | Master, CPOL=Low, CPHA=1Edge, Prescaler=8 |

#### 2.3 时钟树

```
HSE(8MHz) → PLL×9 → SYSCLK=72MHz
  ├─ AHB=72MHz
  ├─ APB2=72MHz (GPIO, USART1, AFIO)
  └─ APB1=36MHz (USART2, CAN, TIM2, IWDG, SPI2)
      └─ TIM2 clk = 36×2 = 72MHz
```

#### 2.4 NVIC 优先级 (分组=2bit抢占/2bit响应)

| 中断 | 抢占 | 响应 |
|------|------|------|
| SysTick | 0 | 0 |
| USART1 | 1 | 0 |
| TIM2 | 2 | 0 |
| CAN1_RX0 | 3 | 0 |

**生成代码**: `GENERATE CODE` → 得到 Makefile 项目骨架。

---

### Step 3: 获取 HAL/CMSIS 库

CubeMX 生成的 `Drivers/` 目录已包含 HAL 和 CMSIS 文件。验证：

```bash
ls Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c      # 必须存在
ls Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s  # 启动文件
```

如果缺失，CubeMX 会自动在线下载。也可以在 CubeMX 中 `Help → Manage embedded software packages` 手动安装 `STM32Cube_FW_F1`。

---

### Step 4: 编写 CMakeLists.txt

**要创建的文件**: `CMakeLists.txt`

**关键要点**:
- 工具链前缀 `arm-none-eabi-`
- CPU 标志 `-mcpu=cortex-m3 -mthumb`
- 宏定义 `STM32F103xB`, `USE_HAL_DRIVER`
- 源文件按 CMSIS / HAL / Core / BSP / App 分组组织
- 链接脚本: `STM32F103C8Tx_FLASH.ld`
- 编译后生成 `.hex` 和 `.bin`

**CMakeLists.txt 结构模板**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(BMS_9S_F103 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 工具链
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

# MCU 标志
set(COMMON_FLAGS "-mcpu=cortex-m3 -mthumb -specs=nano.specs -specs=nosys.specs")
set(CMAKE_C_FLAGS "${COMMON_FLAGS} -Wall -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -flto")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m3 -mthumb -x assembler-with-cpp")

# 宏定义
add_compile_definitions(STM32F103xB USE_HAL_DRIVER HSE_VALUE=8000000)

# 头文件路径
include_directories(
    Core/Inc BSP/Inc App/Inc
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32F1xx/Include
    Drivers/STM32F1xx_HAL_Driver/Inc
    Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
)

# 源文件（随编码进度逐步添加）
set(SOURCES
    # Startup
    Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s
    Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
    # HAL (按需)
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_iwdg.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
    # Core
    Core/Src/main.c
    Core/Src/stm32f1xx_it.c
    Core/Src/stm32f1xx_hal_msp.c
    # BSP（逐步添加）
    BSP/Src/systick.c BSP/Src/led.c BSP/Src/io_ctrl.c
    BSP/Src/i2c_sw.c BSP/Src/usart_drv.c BSP/Src/bq76940.c
    BSP/Src/can_drv.c BSP/Src/timer.c BSP/Src/wdg.c
    # App（逐步添加）
    App/Src/protection.c App/Src/data_report.c App/Src/bms_app.c
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${CMAKE_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld
    ${COMMON_FLAGS} -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)

# 生成 hex/bin
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
)
```

> **注意**: CMakeLists.txt 中的 SOURCES 列表随编码进度逐步取消注释。一开始只保留 CMSIS + HAL + Core 即可验证空项目编译。

---

### Step 5: 创建目录结构 + VSCode 配置

```bash
mkdir -p Core/Inc Core/Src BSP/Inc BSP/Src App/Inc App/Src .vscode build
```

**创建 `.vscode/c_cpp_properties.json`**（IntelliSense 配置）:
```json
{
    "configurations": [{
        "name": "STM32",
        "includePath": [
            "${workspaceFolder}/Core/Inc","${workspaceFolder}/BSP/Inc",
            "${workspaceFolder}/App/Inc","${workspaceFolder}/Drivers/CMSIS/Include",
            "${workspaceFolder}/Drivers/CMSIS/Device/ST/STM32F1xx/Include",
            "${workspaceFolder}/Drivers/STM32F1xx_HAL_Driver/Inc"
        ],
        "defines": ["STM32F103xB","USE_HAL_DRIVER"],
        "compilerPath": "arm-none-eabi-gcc",
        "cStandard": "c11",
        "intelliSenseMode": "gcc-arm"
    }],
    "version": 4
}
```

**创建 `.vscode/launch.json`**（调试配置 — Cortex-Debug 插件）:
```json
{
    "version": "0.2.0",
    "configurations": [{
        "name": "Cortex Debug (OpenOCD)",
        "cwd": "${workspaceFolder}",
        "executable": "${workspaceFolder}/build/BMS_9S_F103.elf",
        "request": "launch",
        "type": "cortex-debug",
        "servertype": "openocd",
        "configFiles": ["interface/stlink.cfg","target/stm32f1x.cfg"],
        "runToEntryPoint": "main",
        "preLaunchTask": "CMake: Build"
    }]
}
```

**创建 `.vscode/tasks.json`**（构建任务）:
```json
{
    "version": "2.0.0",
    "tasks": [{
        "label": "CMake: Build",
        "type": "shell",
        "command": "cmake --build build -j",
        "group": {"kind": "build", "isDefault": true},
        "problemMatcher": "$gcc"
    }]
}
```

**创建 `STM32F103C8Tx_FLASH.ld`**（链接脚本，从 CubeMX 生成的 STM32F103C8Tx_FLASH.ld 复制，或参考标准 STM32F103C8 链接脚本，FLASH=64K, RAM=20K）。

---

### Step 6: 验证空项目编译

```bash
mkdir build && cd build
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
make -j
```

预期结果: 编译成功，生成 `build/BMS_9S_F103.elf`、`.hex`、`.bin`。

> 如果编译失败 → 检查 CMakeLists.txt 中 HAL 源文件路径是否正确，链接脚本是否存在。

---

## 编码阶段

> 每一步都可以独立编译验证。建议**写完一个模块就编译一次**，而不是全部写完后一起编译。

---

### Step 7: Core 层

**参考文件**: CubeMX 生成的 `Core/Inc/main.h`, `Core/Src/main.c`, `Core/Src/stm32f1xx_it.c`

**要创建的文件**:

| 文件 | 内容 |
|------|------|
| `Core/Inc/main.h` | ① 包含 `stm32f1xx_hal.h` ② 所有引脚宏定义 (PA0=MCU_KZ_QB_POWER, PA8=MCU_WAKE_BQ 等) ③ BMS 参数宏 (OVP=4400, UVP=2400 等) ④ `extern` 全局变量声明 |
| `Core/Inc/stm32f1xx_hal_conf.h` | HAL 模块开关: 使能 GPIO/UART/CAN/TIM/IWDG/SPI/FLASH，禁用不需要的模块 |
| `Core/Inc/stm32f1xx_it.h` | 声明 SysTick_Handler, USART1_IRQHandler, TIM2_IRQHandler |
| `Core/Src/stm32f1xx_it.c` | 实现 SysTick 中断(全局计数器递减)、USART1 中断(字节接收)、TIM2 中断(100ms 标志位) |
| `Core/Src/stm32f1xx_hal_msp.c` | HAL_MspInit() — 目前可为空 |

**main.h 引脚宏模板**（完整列表参见 [附录A](#附录a-完整引脚宏定义)）:
```c
// GPIOA
#define MCU_KZ_QB_POWER_PIN    GPIO_PIN_0
#define MCU_KZ_QB_POWER_PORT   GPIOA
#define MCU_D_POWER_PIN        GPIO_PIN_1
#define MCU_D_POWER_PORT       GPIOA
#define MCU_WAKE_BQ_PIN        GPIO_PIN_8
#define MCU_WAKE_BQ_PORT       GPIOA
// ... 按引脚表补全

// GPIOB
#define MCU_IN_STA1_PIN        GPIO_PIN_12
#define MCU_IN_STA1_PORT       GPIOB
#define STA_WAKE_PIN           GPIO_PIN_1
#define STA_WAKE_PORT          GPIOB
#define MCU_BQ1_ART_PIN        GPIO_PIN_2
#define MCU_BQ1_ART_PORT       GPIOB
#define I2C1_SW_SCL_PIN        GPIO_PIN_8
#define I2C1_SW_SCL_PORT       GPIOB
#define I2C1_SW_SDA_PIN        GPIO_PIN_9
#define I2C1_SW_SDA_PORT       GPIOB
// ... 按引脚表补全

// BMS 参数
#define BMS_CELL_COUNT          9
#define BMS_OVP_THRESHOLD       4400
#define BMS_UVP_THRESHOLD       2400
#define BMS_SW_OV_THRESHOLD     4200
#define BMS_SW_UV_THRESHOLD     2800
#define BMS_OC_THRESHOLD        2000
```

**验证**: 编译通过（目前 main.c 可以是一个空的 `while(1){}`）。

---

### Step 8: systick + led（最基础模块）

**参考文件**: `BMS_s940/BSP/systick.c/h`, `BMS_s940/BSP/led.c/h`

**要创建的文件**:

| 文件 | 关键内容 |
|------|---------|
| `BSP/Inc/systick.h` | 声明 `SYSTICK_Init()`, `delay_ms()`, `delay_us()` |
| `BSP/Src/systick.c` | SysTick 配置为 1ms 中断；全局变量 `uwTick` 在 SysTick_Handler 中递减；delay_ms 用 HAL_Delay 或自实现 |
| `BSP/Inc/led.h` | 定义 LED1(PC13)、LED2(PB0)、LED3(PB1)、LED4(PB12)、LED5(PA8) 引脚宏 + `LED_ONOFF` 宏 + `LED_GPIO_Config()` 声明 |
| `BSP/Src/led.c` | 用 `HAL_GPIO_WritePin` 实现 5 个 LED 控制；PC13 注意反逻辑 (低电平亮) |

**CMakeLists.txt 更新**: 在 SOURCES 中添加 `BSP/Src/systick.c BSP/Src/led.c`

**验证**:
```c
// 在 main.c 中:
LED_GPIO_Config();
while (1) {
    LED1_ONOFF(1); delay_ms(500);
    LED1_ONOFF(0); delay_ms(500);  // LED1 以 1Hz 闪烁 → 硬件正常
}
```
**烧录验证**: `openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/BMS_9S_F103.elf verify reset exit"` → 观察 LED1 是否闪烁。

---

### Step 9: io_ctrl（GPIO 控制）

**参考文件**: `BMS_s940/BSP/IO_CTRL.h/c`

**要创建的文件**:

| 文件 | 关键内容 |
|------|---------|
| `BSP/Inc/io_ctrl.h` | 所有控制引脚的 `_ONOFF(x)` 宏 + `IO_CTRL_Config()` 声明 |
| `BSP/Src/io_ctrl.c` | `IO_CTRL_Config()` — 初始化 PA0/PA1/PA8/PA12, PB12/PB1/PB2/PB3/PB4/PB5 为推挽输出，**释放 JTAG** (`__HAL_AFIO_REMAP_SWJ_NOJTAG`)，设置默认电平 |

**io_ctrl.h 宏模板**:
```c
#define MCU_KZ_QB_POWER_ONOFF(x)  HAL_GPIO_WritePin(MCU_KZ_QB_POWER_PORT, MCU_KZ_QB_POWER_PIN, (GPIO_PinState)(x))
#define MCU_D_POWER_ONOFF(x)      HAL_GPIO_WritePin(MCU_D_POWER_PORT, MCU_D_POWER_PIN, (GPIO_PinState)(x))
#define MCU_WAKE_BQ_ONOFF(x)      HAL_GPIO_WritePin(MCU_WAKE_BQ_PORT, MCU_WAKE_BQ_PIN, (GPIO_PinState)(x))
#define MCU_IN_STA1_ONOFF(x)      HAL_GPIO_WritePin(MCU_IN_STA1_PORT, MCU_IN_STA1_PIN, (GPIO_PinState)(x))
// ... 其余引脚
```

**io_ctrl.c 关键代码**:
```c
void IO_CTRL_Config(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    // 逐个引脚 HAL_GPIO_Init ...
    // PA0: gpio.Pin = GPIO_PIN_0; HAL_GPIO_Init(GPIOA, &gpio);
    // PA1: gpio.Pin = GPIO_PIN_1; HAL_GPIO_Init(GPIOA, &gpio);

    // 释放 JTAG → SWD 模式
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    // 设置默认电平
    MCU_IN_STA1_ONOFF(1);
    MCU_KZ_P2_ONOFF(0);
    PB3_ONOFF(0); PB4_ONOFF(0); PB5_ONOFF(0);
}
```

**验证**: 编译通过。

---

### Step 10: i2c_sw（软件 I2C + CRC8）

**参考文件**: `BMS_s940/BSP/i2c1.h/c` (软件 I2C), `BMS_s940/BSP/BQ76930.c` 的 CRC8 函数 (第 1112-1134 行)

**为什么用软件 I2C？** BQ76940 的写操作需要在数据后附加 CRC8 字节，标准 STM32 硬件 I2C 无法在 stop 前自动插入 CRC。软件模拟可以自由控制。

**要创建的文件**:

| 文件 | 关键内容 |
|------|---------|
| `BSP/Inc/i2c_sw.h` | SCL/SDA 宏、BQ76940 地址 `0x08`、函数声明: `I2C_SW_Init`, `I2C_SW_ReadByte`, `I2C_SW_WriteByteCRC`, `CRC8` |
| `BSP/Src/i2c_sw.c` | 分 5 层实现 |

**i2c_sw.c 分 5 层实现**:

```
第1层: GPIO 初始化 — PB8(SCL)/PB9(SDA) 配置为开漏输出，初始拉高
第2层: I2C 基本时序 — i2c_delay(~5μs), i2c_start, i2c_stop, 
        i2c_send_byte, i2c_read_byte
第3层: 标准读写 — I2C_SW_ReadByte (读寄存器), I2C_SW_WriteByte (不带CRC的写)
第4层: CRC8 写 — I2C_SW_WriteByteCRC:
        ① 构建设备地址 = 0x08 << 1 = 0x10
        ② 构建 CRC 计算缓冲区 {0x10, reg_addr, data}
        ③ CRC8(buf, 3, 0x07)
        ④ I2C 发送: Start→addr→reg→data→crc→Stop
        ⑤ delay_ms(10) ← BQ76940 要求写间隔 ≥ 10ms
第5层: CRC8 算法 — 多项式 x^8+x^2+x+1, key=0x07, 初始值 0x00
```

**CRC8 实现**（直接复制参考项目代码，唯一的关键算法不能出错）:
```c
uint8_t CRC8(uint8_t *ptr, uint8_t len, uint8_t key)
{
    uint8_t i, crc = 0;
    while (len-- != 0) {
        for (i = 0x80; i != 0; i /= 2) {
            if ((crc & 0x80) != 0) { crc *= 2; crc ^= key; }
            else crc *= 2;
            if ((*ptr & i) != 0) crc ^= key;
        }
        ptr++;
    }
    return crc;
}
```

**I2C 时序参数**:
| 参数 | 值 | 说明 |
|------|---|------|
| 时钟频率 | ~100KHz | 由 i2c_delay() 控制 |
| 写间隔 | ≥10ms | BQ76940 要求，用 delay_ms(10) 保证 |
| SDA↓→SCL↓ | ≥4.7μs | 起始信号保持时间 |

**验证**: 暂时无法完全验证（需要 BQ76940 硬件连接），但可以编译通过。下一步写 bq76940 后一起验证 I2C 通信。

---

### Step 11: usart_drv（串口驱动）

**参考文件**: `BMS_s940/BSP/usart.c/h`, `BMS_s940/BSP/usart2.c/h`

**要创建的文件**:

| 文件 | 关键内容 |
|------|---------|
| `BSP/Inc/usart_drv.h` | 缓冲区大小常量、ASCII/DEC/HEX 枚举、USART1/2 初始化与收发函数声明 |
| `BSP/Src/usart_drv.c` | USART1(115200, 中断接收) + USART2(9600, 阻塞发送 HMI 指令) |

**USART1 实现要点**:
```c
// 初始化: HAL_UART_Init(&huart1)
// huart1.Instance = USART1, BaudRate = 115200, 
// WordLength = UART_WORDLENGTH_8B, StopBits = UART_STOPBITS_1

// 发送: HAL_UART_Transmit 或 printf 重定向
// 接收: 中断模式 — HAL_UART_Receive_IT
//       在 HAL_UART_RxCpltCallback 中存入缓冲区

// 接收超时检测: 在 TIM2 中断(100ms)中判断数据是否停止流入
//   详见参考项目 stm32f10x_it.c 中 TIM2_IRQHandler 的逻辑
```

**USART2 实现要点**:
```c
// 初始化: 类似 USART1, 波特率 9600
// 仅用于发送 HMI 指令，不需要中断接收
// 封装 USART2_Printf(buf, len, ASCII_CODE)
```

**USART1 指令协议**（与参考项目 QT上位机一致）:
```
帧格式: [0x01] [命令] [0x55] [+可选参数]
  01 02 55 → 开始发送 BMS 数据
  01 03 55 → 停止发送
  01 04 55 → 只开 DSG FET
  01 05 55 → 只关 DSG FET
  01 06 55 → 只开 CHG FET
  01 07 55 → 只关 CHG FET
  01 12 55 + 2B → 写入电池参数至 Flash
  01 13 55 + 2B → 写入温度上限至 Flash
  01 14 55 → 读取 Flash 参数
```

**验证**: 用串口助手连接 USART1(115200)，在 main 中调用 `printf("BMS Init OK\r\n")` 确认收到输出。

---

### Step 12: bq76940 驱动（核心模块）

**参考文件**: `BMS_s940/BSP/BQ76930.c/h`（注意文件名误导，实际是 BQ76940 驱动）

这是整个项目最核心的模块。按以下子步骤顺序编写:

```
12.1 头文件 bq76940.h
12.2 CRC8 + ADC 校准 (Get_offset)
12.3 单个电池电压读取 (以 Get_Battery1 为模板)
12.4 其余 8 节电池电压读取
12.5 总电压 + SOC + 电流 + 温度
12.6 BQ76940 初始化 + 保护配置 + FET 控制
12.7 数据打包 (Update_val)
```

#### 12.1 头文件 bq76940.h

**关键内容**:
- 所有寄存器地址宏 (SYS_STAT=0x00, VC1_HI=0x0C, ... 参见 [附录B](#附录b-bq76940-完整寄存器表))
- 保护参数宏 (OVP_THRESHOLD=4400, UVP_THRESHOLD=2400, ...)
- SCD/OCD 延迟和阈值枚举
- 所有函数声明
- LOW_BYTE/HIGH_BYTE 辅助宏

#### 12.2-12.4 电压采集

**核心算法 — 电压计算公式**:
```
① 读 ADCGAIN1(0x50), ADCGAIN2(0x59), ADCOFFSET(0x51)
② ADC_GAIN = ((gain1 & 0x0C) << 1) + ((gain2 & 0xE0) >> 5)
③ GAIN = 365 + ADC_GAIN  (单位: μV/LSB, 典型值 ~377)
④ V_cell(mV) = (ADC_raw × GAIN) / 1000 + ADC_offset
```

**9 节电池读取函数与寄存器对应**:
| 函数 | 寄存器 | 存入 Batteryval[] |
|------|--------|-------------------|
| Get_Battery1() | 0x0C/0x0D | [0] |
| Get_Battery2() | 0x0E/0x0F | [1] |
| Get_Battery5() | 0x14/0x15 | [4] ← 索引不连续! |
| Get_Battery6() | 0x16/0x17 | [5] |
| Get_Battery7() | 0x18/0x19 | [6] |
| Get_Battery10() | 0x1E/0x1F | [9] |
| Get_Battery11() | 0x20/0x21 | [10] |
| Get_Battery12() | 0x22/0x23 | [11] |
| Get_Battery15() | 0x28/0x29 | [14] |

> ⚠️ **不要用 for 循环遍历**: 只有 9 个通道接了电池，未连接的通道读数无意义。

**每个 Get_BatteryX() 的模式** (以 Cell1 为模板):
```c
void Get_Battery1(void)
{
    uint16_t raw = ((uint16_t)I2C_SW_ReadByte(0x0C) << 8) | I2C_SW_ReadByte(0x0D);
    int16_t mv = (int16_t)(((int32_t)raw * GAIN) / 1000) + ADC_offset;
    Batteryval[0] = mv;
    // 打包到 CAN/UART 发送缓冲区
    can_buf1[2] = HIGH_BYTE(mv);
    can_buf1[3] = LOW_BYTE(mv);
}
```

#### 12.5 总电压 / SOC / 电流 / 温度

**总电压** (只求和 9 节有连接的电池):
```c
Batteryval[15] = Batteryval[0] + Batteryval[1]
               + Batteryval[4] + Batteryval[5] + Batteryval[6]
               + Batteryval[9] + Batteryval[10] + Batteryval[11]
               + Batteryval[14];
```

**SOC** — 简化查表法（基于总电压的 1/9 平均）:
```c
if (total_mv > 4100*9) SOC = 100;
else if (total_mv > 4050*9) SOC = 90;
// ... 详见参考项目 BQ76930.c Get_SOC() 第 584-621 行
else if (total_mv > 3100*9) SOC = 5;
else SOC = 0;
```

**电流** — 库仑计数器 (CC 寄存器 0x32/0x33):
```c
uint16_t raw = (I2C_SW_ReadByte(0x32) << 8) | I2C_SW_ReadByte(0x33);
// 4mΩ 采样电阻, 8x 增益 → 系数 2.11
if (raw <= 0x7D00) current = raw * 2.11;             // 放电
else               current = (0xFFFF - raw) * 2.11;  // 充电
Batteryval[17] = (int16_t)current;
```

**温度** — TS1 通道 (103AT NTC, R25=10KΩ, B=3380K):
```c
// ① ADC 值 → 电阻值
Rt = 10000 * (ts_raw * 382 / 1000) / (3300 - ts_raw * 382 / 1000);
// ② B 值公式 → 温度
T_K = 1 / (1/298.15 + ln(Rt/10000) / 3380);
Temp = T_K - 273.15 + 0.5;  // +0.5 四舍五入
Batteryval[18] = (int16_t)Temp;
```
> 注意: 需要链接数学库 `-lm`，在 CMakeLists.txt 添加 `target_link_libraries(${CMAKE_PROJECT_NAME} m)`

#### 12.6 初始化 / 保护配置 / FET 控制

**BQ76940 初始化序列**:
```c
void BQ76940_Config(void)
{
    WAKE_ALL_DEVICE();   // PA8 输出高→延时 100ms→低 (脉冲唤醒)
    BQ_1_Config();       // 写入 12 个配置寄存器 (CRC模式)
    Get_offset();        // 读取 ADC 校准值
    OV_UV_1_PROTECT();   // 计算并写入 OV/UV 阈值
    OCD_SCD_PROTECT();   // 配置 SCD/OCD 参数
    I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);  // 清除告警
}
```

**12 个初始化寄存器**:
```
地址: SYS_STAT CELLBAL1 CELLBAL2 CELLBAL3 SYS_CTRL1 SYS_CTRL2 PROTECT1 PROTECT2 PROTECT3 OV_TRIP UV_TRIP CC_CFG
数据:   0xFF      0x00      0x00      0x00      0x18      0x43      0x00      0x00      0x00    0x00    0x00   0x19
```

**OV/UV 阈值计算** (在 OV_UV_1_PROTECT 中):
```c
float t = 0.377;  // GAIN in mV/LSB
OVTrip_Val = (uint8_t)((((uint16_t)((OVPThreshold - ADC_offset)/t + 0.5)) >> 4) & 0xFF);
UVTrip_Val = (uint8_t)((((uint16_t)((UVPThreshold - ADC_offset)/t + 0.5)) >> 4) & 0xFF);
I2C_SW_WriteByteCRC(OV_TRIP, OVTrip_Val);
I2C_SW_WriteByteCRC(UV_TRIP, UVTrip_Val);
```

**FET 控制** (SYS_CTRL2 寄存器, 地址 0x05):
```
0x43 → DSG=1, CHG=1  // Open_DSG_CHG()
0x40 → DSG=0, CHG=0  // Close_DSG_CHG()
0x42 → DSG=1, CHG=0  // Only_Open_DSG()
0x41 → DSG=0, CHG=1  // Only_Open_CHG()
```

**验证**: 连接 BQ76940 硬件，调用 `Get_offset()` 后打印 GAIN 和 ADC_offset 值（应有合理的非零值），调用 `Get_Battery1()` 后打印电压（应在 2500-4200mV 范围）。

---

### Step 13: can_drv（CAN 驱动）

**参考文件**: `BMS_s940/BSP/can.c/h`

**要创建的文件**:

| 文件 | 关键内容 |
|------|---------|
| `BSP/Inc/can_drv.h` | CAN 初始化声明、Can_Send_Msg/Can_Receive_Msg 声明 |
| `BSP/Src/can_drv.c` | HAL_CAN 初始化和收发 |

**CAN 配置** (PA11=RX, PA12=TX, 500Kbps, 扩展帧):
```c
uint8_t CAN_Init(void)
{
    CAN_FilterTypeDef sFilterConfig;
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 4;       // 36MHz / 4 = 9MHz
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_9TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_8TQ;  // 9MHz / (9+8+1) = 500Kbps
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    HAL_CAN_Init(&hcan1);

    // 滤波器: 接收所有扩展帧
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0; sFilterConfig.FilterIdLow = 0;
    sFilterConfig.FilterMaskIdHigh = 0; sFilterConfig.FilterMaskIdLow = 0;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

    HAL_CAN_Start(&hcan1);
    return 0;
}
```

**发送** (扩展帧):
```c
uint32_t Can_Send_Msg(uint8_t *msg, uint8_t len, uint32_t ext_id)
{
    CAN_TxHeaderTypeDef TxHeader = {
        .ExtId = ext_id, .IDE = CAN_ID_EXT,
        .RTR = CAN_RTR_DATA, .DLC = len
    };
    uint32_t TxMailbox;
    return HAL_CAN_AddTxMessage(&hcan1, &TxHeader, msg, &TxMailbox);
}
```

**CAN 数据帧分配** (7帧, 每帧 8 字节):
```
ID 0x0001: [AA][01][C1_H][C1_L][C2_H][C2_L][C3_H][C3_L]
ID 0x0002: [AA][02][C4_H][C4_L][C5_H][C5_L][C6_H][C6_L]
ID 0x0003: [AA][03][C7_H][C7_L][C8_H][C8_L][C9_H][C9_L]
ID 0x0004: [AA][04]...
ID 0x0005: [AA][05]...
ID 0x0006: [AA][06][TotV_H][TotV_L][SOC_H][SOC_L][Curr_H][Curr_L]
ID 0x0007: [AA][07][Temp_H][Temp_L][DSG][CHG][00][00]
```

**验证**: 连接 CAN 分析仪(如 PCAN-USB)，调用 `Can_Send_Msg(test_data, 8, 0x001)` 确认收到数据。

---

### Step 14: timer + wdg

**参考文件**: `BMS_s940/BSP/timer.c/h`, `BMS_s940/BSP/wdg.c/h`

**timer.c** — TIM2 100ms 周期定时:
```c
void TIM2_Config(uint16_t period, uint16_t prescaler)
{
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = prescaler;       // 7199
    htim2.Init.Period = period;             // 999
    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start_IT(&htim2);          // 使能更新中断
}
// TIM2_IRQHandler → HAL_TIM_IRQHandler → HAL_TIM_PeriodElapsedCallback
// 在 Callback 中: ① USART1 接收超时判断 ② 全局 tick 计数
```

**wdg.c** — IWDG ~4s:
```c
void IWDG_Config(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250;
    HAL_IWDG_Init(&hiwdg);
}
void IWDG_Feed(void) { HAL_IWDG_Refresh(&hiwdg); }
```

**验证**: 编译通过。

---

### Step 15: App 层

**要创建的文件**:

| 文件 | 职责 |
|------|------|
| `App/Inc/protection.h` | `Protection_Check()` — OV/UV/OC/OT 判断 |
| `App/Src/protection.c` | 实现 4 种保护判断逻辑 |
| `App/Inc/data_report.h` | `DataReport_SendAll()` — 数据打包发送 |
| `App/Src/data_report.c` | USART1 + USART2 + CAN 三种上报 |
| `App/Inc/bms_app.h` | `BMS_DataAcquire()` — 采集调度 |
| `App/Src/bms_app.c` | 按顺序调用 bq76940 采集函数 |

**protection.c 核心逻辑**:
```c
void Protection_Check(void)
{
    // 9 节电池电压在 Batteryval[] 的索引:
    // [0], [1], [4], [5], [6], [9], [10], [11], [14]

    // 过压: 任一节 > 4200mV → Close_CHG; 全部 < 4100mV → Open_CHG
    // 欠压: 任一节 < 2800mV → Close_DSG; 全部 > 2900mV → Open_DSG
    // 过流: Batteryval[17] > 2000mA → Close_Both; < 2000 → Open_Both
    // 过温: Batteryval[18] > Flash中存储的温度阈值 → Close_Both

    // 每次修改 FET 状态后调用:
    // I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);  // 清除 BQ76940 状态
}
```

**data_report.c 结构**:
```c
void DataReport_SendAll(void)
{
    // 通道1: USART1 → QT上位机 (原始字节帧)
    //   帧头 AA 01 / AA 02 / AA 03 / AA 04，每帧含电压数据

    // 通道2: USART2 → HMI 串口屏 (ASCII 指令)
    //   sprintf(buf, "DCV16(0,0,'%s%d%s',3);\r\n", "第一节电压:", mv, "mV");
    //   USART2_Printf(buf, ...);

    // 通道3: CAN → 7 帧数据 (ID 0x0001-0x0007)
    //   Can_Send_Msg(can_buf1, 8, 0x0001);
    //   ... Can_Send_Msg(can_buf7, 8, 0x0007);
}
```

**bms_app.c 采集调度**:
```c
void BMS_DataAcquire(void)
{
    // 按参考项目 Get_Update_Data() 的顺序调用:
    Get_Battery1();
    Get_Battery2();
    Get_Battery5();
    Get_Battery6();
    Get_Battery7();
    Get_Battery10();
    Get_Battery11();
    Get_Battery12();
    Get_Battery15();
    Get_Update_ALL_Data();   // 总电压 → Batteryval[15]
    Get_SOC();               // SOC → Batteryval[16]
    Get_BQ1_2_Temp();        // 温度 → Batteryval[18]
    Get_BQ_Current();        // 电流 → Batteryval[17]
    BMS_STA();               // FET 状态
    DataReport_SendAll();    // 上报所有通道
}
```

**验证**: 编译通过。

---

### Step 16: main.c 整合

**这是最后一步** — 把所有模块串联起来。

```c
#include "main.h"
#include "systick.h"    // 延时
#include "led.h"        // LED
#include "io_ctrl.h"    // GPIO
#include "i2c_sw.h"     // I2C
#include "usart_drv.h"  // 串口
#include "bq76940.h"    // BQ76940
#include "can_drv.h"    // CAN
#include "timer.h"      // TIM2
#include "wdg.h"        // IWDG
#include "protection.h" // 保护逻辑
#include "bms_app.h"    // 采集调度

// 全局变量
int     Batteryval[50] = {0};
uint8_t BMS_DATA_FLAG   = 0;
uint8_t OV_FLAG = 0, UV_FLAG = 0, OC_FLAG = 0;

// Flash 参数存储地址
#define FLASH_BATTERY_ADDR  0x08007800
#define FLASH_TEMP_ADDR     0x08007C00

int main(void)
{
    // === 初始化阶段 (严格按此顺序) ===
    HAL_Init();
    SystemClock_Config();        // HSE → PLL → 72MHz
    SYSTICK_Init();              // 1ms SysTick
    LED_GPIO_Config();           // 5 个 LED
    IO_CTRL_Config();            // GPIO 控制引脚 + JTAG释放
    USART1_Init(115200);         // QT 上位机
    USART2_Init(9600);           // HMI 串口屏
    I2C_SW_Init();               // 软件 I2C → BQ76940
    CAN_Init();                  // CAN 500Kbps

    // === BQ76940 初始化 ===
    BQ76940_Config();            // 初始化+保护配置
    LED4_ONOFF(1);               // 初始化完成指示

    // === 定时器 + 看门狗 ===
    TIM2_Config(999, 7199);      // 100ms
    IWDG_Config();               // 4s 看门狗

    // === HMI 初始化 ===
    UartSend("MODE_CFG(1);DIR(1);\r\n");  // 串口屏配置
    HAL_Delay(1000);
    UartSend("CLR(61);\r\n");

    // === 主循环 ===
    while (1)
    {
        IWDG_Feed();             // 喂狗 (不能放在任何可能阻塞的代码之后)
        LEDXToggle(1);           // 心跳指示

        // ① 接收 CAN 命令
        uint8_t canbuf[8];
        if (Can_Receive_Msg(canbuf)) { /* 处理CAN命令 */ }

        // ② 接收并处理 USART1 指令
        RECEIVE_DATA_DEAL();     // 解析上位机指令

        // ③ 采集 BMS 数据 (当收到开始指令后)
        if (BMS_DATA_FLAG) {
            BMS_DataAcquire();   // 采集 + 上报
        }

        // ④ 保护判断
        Protection_Check();      // OV/UV/OC/OT
    }
}
```

**RECEIVE_DATA_DEAL 实现**（参考 main.c 第 61-140 行）:
```c
void RECEIVE_DATA_DEAL(void)
{
    if (Get_USART1_StopFlag() == USART1_STOP_TRUE) {
        // 检查指令帧头
        if (buf[0]==0x01 && buf[1]==0x02 && buf[2]==0x55)
            BMS_DATA_FLAG = 1;   // 开始发送
        if (buf[0]==0x01 && buf[1]==0x03 && buf[2]==0x55)
            BMS_DATA_FLAG = 0;   // 停止发送
        if (buf[0]==0x01 && buf[1]==0x04 && buf[2]==0x55)
            Only_Open_DSG();     // 只开放电
        // ... 其余指令按协议表实现
        Set_USART1_StopFlag(USART1_STOP_FALSE);
    }
}
```

**Flash 读写**（用于存储参数，STM32F103 为半字编程）:
```c
void Write_Flash(uint32_t addr, uint16_t data) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef e = {.TypeErase=FLASH_TYPEERASE_PAGES, .PageAddress=addr, .NbPages=1};
    uint32_t err;
    HAL_FLASHEx_Erase(&e, &err);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data);
    HAL_FLASH_Lock();
}
uint16_t Read_Flash(uint32_t addr) { return *(volatile uint16_t*)addr; }
```

**验证**: 全项目编译通过。

---

## 验证阶段

### Step 17: 编译 + 烧录

```bash
# 编译
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# 烧录 (ST-Link)
openocd -f interface/stlink.cfg \
        -f target/stm32f1x.cfg \
        -c "program build/BMS_9S_F103.elf verify reset exit"

# 烧录 (J-Link)
openocd -f interface/jlink.cfg \
        -f target/stm32f1x.cfg \
        -c "program build/BMS_9S_F103.elf verify reset exit"
```

### Step 18: 14 项功能验证

| # | 验证项 | 预期 | 方法 |
|---|--------|------|------|
| 1 | LED 心跳 | ~1Hz 闪烁 | 目视 |
| 2 | I2C 通信 | SYS_STAT ≠ 0xFF | 串口打印寄存器 |
| 3 | 9 节电压 | 2500-4200mV | 万用表对比 |
| 4 | 总电压 | 9 节和 ±50mV | 万用表对比 |
| 5 | 温度 | 环境温度 ±3°C | 温度计对比 |
| 6 | 电流 | 0±100mA(空载) | 万用表对比 |
| 7 | SOC | 随电压变化 | 串口打印 |
| 8 | CAN 发送 | 7 帧收到 | CAN 分析仪 |
| 9 | USART1 指令 | 01 02 55→收到数据 | 串口助手 |
| 10 | HMI 显示 | 屏显电压/电流/SOC | 目视串口屏 |
| 11 | 过压保护 | >4.2V→CHG 关 | 可调电源 |
| 12 | 欠压保护 | <2.8V→DSG 关 | 可调电源 |
| 13 | 看门狗 | 不喂狗→4s 复位 | 注释 IWDG_Feed() |
| 14 | Flash 存储 | 写值→复位→读值一致 | USART1 指令 |

---

## 迭代开发路径建议

```
第1轮 (最小验证): Step 1-8 + Step 10-12.2
  → 确认开发环境、LED 闪烁、I2C 能读到 BQ76940 寄存器值

第2轮 (数据采集): Step 12.3-12.5 + Step 11
  → 完成 9 节电压、温度、电流、SOC 采集
  → 通过串口打印所有数据

第3轮 (完整功能): Step 12.6-12.7 + Step 13-16
  → 保护逻辑、CAN 上报、HMI 显示、指令解析

第4轮 (打磨): 长时间运行测试、Flash 参数存储、异常处理优化
```

---

## 附录A: 完整引脚宏定义

```c
// === GPIOA ===
#define MCU_KZ_QB_POWER_PIN     GPIO_PIN_0   // PA0 - QB电源
#define MCU_KZ_QB_POWER_PORT    GPIOA
#define MCU_D_POWER_PIN         GPIO_PIN_1   // PA1 - D电源
#define MCU_D_POWER_PORT        GPIOA
#define MCU_WAKE_BQ_PIN         GPIO_PIN_8   // PA8 - BQ唤醒
#define MCU_WAKE_BQ_PORT        GPIOA
#define MCU_KZ_P2_PIN           GPIO_PIN_12  // PA12 - P2控制(⚠️与CAN_TX冲突)
#define MCU_KZ_P2_PORT          GPIOA

// === GPIOB ===
#define MCU_IN_STA1_PIN         GPIO_PIN_12  // PB12 - 状态指示
#define MCU_IN_STA1_PORT        GPIOB
#define STA_WAKE_PIN            GPIO_PIN_1   // PB1 - 唤醒状态
#define STA_WAKE_PORT           GPIOB
#define MCU_BQ1_ART_PIN         GPIO_PIN_2   // PB2 - BQ Alert
#define MCU_BQ1_ART_PORT        GPIOB
#define PB3_PIN                 GPIO_PIN_3   // PB3 (JTAG禁用后)
#define PB3_PORT                GPIOB
#define PB4_PIN                 GPIO_PIN_4   // PB4 (JTAG禁用后)
#define PB4_PORT                GPIOB
#define PB5_PIN                 GPIO_PIN_5   // PB5
#define PB5_PORT                GPIOB
#define I2C1_SW_SCL_PIN         GPIO_PIN_8   // PB8 - 软件I2C SCL
#define I2C1_SW_SCL_PORT        GPIOB
#define I2C1_SW_SDA_PIN         GPIO_PIN_9   // PB9 - 软件I2C SDA
#define I2C1_SW_SDA_PORT        GPIOB

// === LED 引脚 ===
#define LED1_PIN    GPIO_PIN_13
#define LED1_PORT   GPIOC
#define LED2_PIN    GPIO_PIN_0
#define LED2_PORT   GPIOB
#define LED3_PIN    GPIO_PIN_1
#define LED3_PORT   GPIOB
#define LED4_PIN    GPIO_PIN_12
#define LED4_PORT   GPIOB
#define LED5_PIN    GPIO_PIN_8
#define LED5_PORT   GPIOA
```

## 附录B: BQ76940 完整寄存器表

| 地址 | 名称 | 描述 |
|------|------|------|
| 0x00 | SYS_STAT | 系统状态 (UV/OV/SCD/OCD 标志) |
| 0x01-03 | CELLBAL1-3 | 电池均衡控制 |
| 0x04 | SYS_CTRL1 | ADC使能, SHIP模式控制 |
| 0x05 | SYS_CTRL2 | DSG/CHG FET 控制 |
| 0x06 | PROTECT1 | SCD 阈值和延迟 |
| 0x07 | PROTECT2 | OCD 阈值和延迟 |
| 0x08 | PROTECT3 | 保护3 |
| 0x09 | OV_TRIP | 过压阈值 |
| 0x0A | UV_TRIP | 欠压阈值 |
| 0x0B | CC_CFG | 库仑计配置 |
| 0x0C-0x29 | VC1-VC15 | 15路 14-bit ADC 电压 (只用9路) |
| 0x2A-0x2B | BAT | 总电池电压 |
| 0x2C-0x2F | TS1-TS2 | 温度传感器 |
| 0x32-0x33 | CC | 库仑计数器 (电流) |
| 0x50 | ADCGAIN1 | ADC 增益1 |
| 0x51 | ADCOFFSET | ADC 偏移 |
| 0x59 | ADCGAIN2 | ADC 增益2 |

## 附录C: StdPeriph → HAL 常用转换

| 功能 | StdPeriph | HAL |
|------|----------|-----|
| GPIO 时钟 | `RCC_APB2PeriphClockCmd(RCC_..., ENABLE)` | `__HAL_RCC_GPIOx_CLK_ENABLE()` |
| GPIO 写 | `GPIO_WriteBit(GPIOA, PIN, Bit_SET)` | `HAL_GPIO_WritePin(GPIOA, PIN, GPIO_PIN_SET)` |
| GPIO 读 | `GPIO_ReadInputDataBit(GPIOA, PIN)` | `HAL_GPIO_ReadPin(GPIOA, PIN)` |
| JTAG重映射 | `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` | `__HAL_AFIO_REMAP_SWJ_NOJTAG()` |
| USART 发送 | `USART_SendData(USART1, data)` | `HAL_UART_Transmit(&huart1, &data, 1, timeout)` |
| CAN 发送 | `CAN_Transmit(CAN1, &TxMsg)` | `HAL_CAN_AddTxMessage(&hcan1, &TxHeader, data, &mbox)` |
| TIM 启动 | `TIM_Cmd(TIM2, ENABLE)` | `HAL_TIM_Base_Start_IT(&htim2)` |
| IWDG 喂狗 | `IWDG_ReloadCounter()` | `HAL_IWDG_Refresh(&hiwdg)` |
| Flash 编程 | `FLASH_ProgramWord(addr, data)` | `HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data)` |
| 延时 ms | `delay_ms(n)` | `HAL_Delay(n)` |

## 附录D: FreeRTOS 集成配置

> FreeRTOS 是可选项。参考项目为裸机设计，加入 FreeRTOS 可更好地管理多任务（数据采集、保护判断、通信上报等并行执行）。
> 以下为 STM32F103C8T6 + FreeRTOS 的完整时钟和系统配置指南。

### D.1 CubeMX 启用 FreeRTOS

```
CubeMX → Pinout & Configuration → Middleware → FREERTOS
  Interface: CMSIS_V2 (推荐)
  勾选 "Enabled"
```

CubeMX 会自动:
- 添加 FreeRTOS 源码到 `Middlewares/Third_Party/FreeRTOS/`
- 生成 `FreeRTOSConfig.h`
- 将 SysTick 分配给 FreeRTOS 作为系统 tick
- 将 HAL 时基从 SysTick 切换到其他定时器（通常是 TIM1 或 TIM17）

### D.2 时钟配置变化

**裸机方案** (原设计):
```
SysTick → HAL_Delay() + 自定义 delay_ms()
TIM2    → 100ms 数据采集节拍
```

**FreeRTOS 方案**:
```
SysTick → FreeRTOS 系统 tick (1ms, 不可抢占)
TIM2    → 100ms 数据采集节拍 (保留)
TIM17   → HAL 时基 (替代 SysTick，供 HAL_Delay 等使用)
```

#### D.2.1 FreeRTOS Tick 时钟源选择

| 方案 | 时钟源 | 适用场景 |
|------|--------|---------|
| **SysTick** (默认) | 72MHz (AHB/8 或直接 AHB) | 通用，CubeMX 自动配置，省一个定时器 |
| TIMx | 72MHz (APB1/2 定时器时钟) | SysTick 被占用时的替代方案 |

**本项目推荐**: 使用 SysTick 作为 FreeRTOS tick，另选 TIM17 作为 HAL 时基（CubeMX 默认行为）。

#### D.2.2 CubeMX 中的具体配置

```
① Pinout & Configuration → System Core → SYS
   Timebase Source: TIM17 (代替 SysTick 给 HAL 用)

② Pinout & Configuration → Middleware → FREERTOS → Config Parameters:
   TICK_RATE_HZ: 1000         → 1ms 系统 tick
   CPU_CLOCK_HZ: 72000000     → SYSCLK 72MHz
   configTICK_RATE_HZ: 1000
```

**关键结果**:
- SysTick 10ms 中断改为 1ms (FreeRTOS tick)，中断优先级最低
- SysTick_Handler 中调用 `xPortSysTickHandler()` 或 FreeRTOS 自动接管
- TIM17 提供 HAL 1ms 时基 (`HAL_InitTick()` 默认调用 `HAL_TIM_Base_Start_IT`)
- TIM2 保持原有 100ms 配置不变

### D.3 NVIC 优先级重新分配

**FreeRTOS 的优先级规则**（关键！）:
> FreeRTOS 管理的中断优先级必须 ≤ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (默认 5，即抢占优先级 ≥ 5 的中断才能调用 FreeRTOS API)。

STM32F103 使用 4-bit 优先级，NVIC_PRIORITYGROUP_4 (全抢占):
```
configMAX_SYSCALL_INTERRUPT_PRIORITY = 5  (实际写入寄存器 = 5<<4 = 0x50)
→ 抢占优先级 ≥ 5 的中断可以调用 FreeRTOS API (如 xSemaphoreGiveFromISR)
→ 抢占优先级 0-4 的中断不能调用 FreeRTOS API (裸机级中断)
```

**更新后的中断优先级表** (分组 = 4bit 抢占 / 0bit 响应):

| 中断 | 抢占优先级 | 能否调 FreeRTOS API | 说明 |
|------|----------|-------------------|------|
| SysTick | 15 (最低) | ✅ | FreeRTOS tick，不可抢占任何用户中断 |
| USART1 | 5 | ✅ | 上位机指令接收，可用 xQueueSendFromISR |
| TIM2 | 6 | ✅ | 100ms 采集触发，可用 xSemaphoreGiveFromISR |
| CAN1_RX0 | 7 | ✅ | CAN 消息，可用 xQueueSendFromISR |
| PendSV | - | FreeRTOS 内部 | 任务切换，优先级最低 |
| SVC | - | FreeRTOS 内部 | 系统调用 |

> ⚠️ **中断优先级分组必须设为 NVIC_PRIORITYGROUP_4 (全抢占)** — CubeMX 开启 FreeRTOS 后会自动设置。原裸机方案用的是 NVIC_PRIORITYGROUP_2，FreeRTOS 下必须改为 4。

### D.4 FreeRTOSConfig.h 关键参数

```c
#define configCPU_CLOCK_HZ              (72000000)   // SYSCLK 72MHz
#define configTICK_RATE_HZ              ((TickType_t)1000)  // 1ms tick
#define configMAX_PRIORITIES            (7)          // 任务优先级 0-6
#define configMINIMAL_STACK_SIZE        ((uint16_t)128)
#define configTOTAL_HEAP_SIZE           ((size_t)8192)  // 8KB 堆 (STM32F103C8 有 20KB RAM)
#define configMAX_TASK_NAME_LEN         (16)
#define configUSE_PREEMPTION            1            // 抢占式调度
#define configUSE_TIME_SLICING          1            // 时间片轮转
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  5     // 关键: FreeRTOS API 中断阈值
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15    // 最低中断优先级
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
```

### D.5 CMakeLists.txt 变更

```
Middlewares/Third_Party/FreeRTOS/Source/
├── tasks.c, queue.c, list.c, timers.c       (核心)
├── event_groups.c, stream_buffer.c          (按需)
├── CMSIS_RTOS_V2/cmsis_os2.c               (CMSIS_V2 封装)
└── portable/GCC/ARM_CM3/port.c             (Cortex-M3 移植层)
```

需在 CMakeLists.txt 中添加:
```cmake
# 头文件路径
include_directories(
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
)

# 源文件
set(FREERTOS_SRCS
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
)
```

### D.6 任务设计建议

将裸机 main.c 的 `while(1)` 循环拆分为 FreeRTOS 任务:

| 任务 | 优先级 | 周期 | 职责 |
|------|--------|------|------|
| **DataAcqTask** | 3 (中) | 100ms | 触发 I2C 读取 BQ76940 → 存入全局数据 |
| **ProtectionTask** | 4 (高) | 50ms | 检查过压/欠压/过流/过温 → FET 控制 |
| **ReportTask** | 2 (低) | 200ms | USART1 + USART2 + CAN 数据上报 |
| **CmdTask** | 3 (中) | 事件驱动 | USART1 指令接收（信号量触发）|
| **CanRxTask** | 2 (低) | 事件驱动 | CAN 消息接收处理 |
| **IdleTask** | 0 | - | FreeRTOS 自动创建，喂 IWDG 也放在此 hook |

**任务间通信**:
```
DataAcqTask ──(全局 Batteryval[])──→ ProtectionTask
DataAcqTask ──(全局 Batteryval[])──→ ReportTask
CmdTask ──(BinarySemaphore)──→ DataAcqTask  (开始/停止采集)
TIM2 ISR ──(BinarySemaphore)──→ DataAcqTask  (100ms 触发)
USART1 ISR ──(Queue/字节流)──→ CmdTask       (指令数据)
```

### D.7 HAL 时基重配置

启用 FreeRTOS 后，`HAL_Init()` 默认调用 `HAL_InitTick(TICK_INT_PRIORITY)` 时使用的是 SysTick。由于 SysTick 已被 FreeRTOS 接管，需要改用 TIM17:

```c
// 在 main.c 的 HAL_Init() 之后:
HAL_Init();
// CubeMX 生成的 HAL_InitTick 会自动使用 TIM17 作为 HAL 时基
// 验证: 检查 stm32f1xx_hal_timebase_tim.c (CubeMX 自动生成)
SystemClock_Config();
```

CubeMX 生成 `stm32f1xx_hal_timebase_tim.c`:
```c
// CubeMX 自动生成，无需手动编写
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    // 使用 TIM17 提供 1ms HAL 时基
    htim17.Instance = TIM17;
    htim17.Init.Prescaler = (SystemCoreClock / 1000000U) - 1;  // 1MHz
    htim17.Init.Period = (1000000U / 1000U) - 1;               // 1ms
    HAL_TIM_Base_Init(&htim17);
    HAL_TIM_Base_Start_IT(&htim17);
    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, TickPriority, 0);
    return HAL_OK;
}
```

### D.8 与 Step 8 的冲突处理

原 Step 8 的 `systick.c` 依赖 SysTick 实现 `delay_ms()`。在 FreeRTOS 下:
- **删除** `systick.c` 中的 SysTick 配置代码和 `delay_ms()`
- **改用** `HAL_Delay()` (由 TIM17 驱动) 或 FreeRTOS 的 `vTaskDelay()` / `vTaskDelayUntil()`

```c
// 裸机: delay_ms(100);
// FreeRTOS: vTaskDelay(pdMS_TO_TICKS(100));

// 裸机: 阻塞等待
// FreeRTOS: 用信号量或队列等待 (释放 CPU 给其他任务)
```

### D.9 STM32F103C8T6 资源约束

| 资源 | 总量 | FreeRTOS 占用 | 剩余 (BMS任务) | 说明 |
|------|------|-------------|---------------|------|
| Flash | 64 KB | ~8 KB | ~56 KB | FreeRTOS + HAL 约 8KB |
| RAM | 20 KB | ~8 KB (堆) | ~12 KB | 5-6 个任务，每个 512B-1KB 栈 |
| 定时器 | 4 个 | TIM17 | TIM2 + 2 个可用 | TIM17=HAL时基 |
| 中断优先级 | 16 级 | 5-15 (FreeRTOS域) | 0-4 (裸机域) | 严格遵循优先级规则 |

> ⚠️ **RAM 紧张**: 20KB RAM 中 FreeRTOS 堆占 8KB，剩余 12KB 用于 5-6 个任务栈 + 数据缓冲区 + 系统栈。注意 `Batteryval[50]` (400B) + USART 缓冲区 (1KB) + CAN 缓冲区等开销。如果 OOM，可将 `configTOTAL_HEAP_SIZE` 减至 6KB。

### D.10 FreeRTOS 集成后的验证步骤

```
1. CubeMX 启用 FreeRTOS → 生成代码
2. 按 D.5 更新 CMakeLists.txt (添加 FreeRTOS 源文件)
3. 编译 → 应通过
4. 创建简单测试任务: LED 闪烁 (验证调度器运行)
5. 逐个将裸机功能迁移为任务 (按 D.6 的任务设计)
6. 测试中断中调用 FreeRTOS API (特别是 TIM2 ISR 中发信号量)
7. 长时间运行稳定性测试
```

---

> **文档版本**: v2.1
> **最后更新**: 2026-07-15
> **与 v2.0 的区别**: 新增附录D FreeRTOS 集成配置 (时钟/SysTick/NVIC/CMake/任务设计)。
