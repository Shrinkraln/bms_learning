# BMS 9串 开发指导框架

> **目标**: 使用 CubeMX + CMake + VSCode 工具链，基于 STM32F103C8T6 + BQ76940，实现 9串 BMS 固件。
> **参考项目**: `datasheet/BQ76940发货资料 20220817/3.程序/1.BQ76940（程序加CAN20210915）改温度探头/BMS_s940/`
> **原则**: 引脚配置和控制逻辑与参考项目完全一致。

---

## 目录

1. [开发环境搭建](#1-开发环境搭建)
2. [CubeMX 项目配置](#2-cubemx-项目配置)
3. [CMake 工程搭建](#3-cmake-工程搭建)
4. [代码架构与分层设计](#4-代码架构与分层设计)
5. [BSP 驱动层编写流程](#5-bsp-驱动层编写流程)
6. [BQ76940 驱动编写流程](#6-bq76940-驱动编写流程)
7. [应用层编写流程](#7-应用层编写流程)
8. [VSCode 调试配置](#8-vscode-调试配置)
9. [编译、烧录、验证](#9-编译烧录验证)
10. [StdPeriph → HAL 转换对照表](#10-stdperiph--hal-转换对照表)
11. [参考项目关键代码摘录](#11-参考项目关键代码摘录)

---

## 1. 开发环境搭建

### 1.1 必备工具

| 工具 | 版本要求 | 下载/安装方式 | 用途 |
|------|---------|-------------|------|
| **STM32CubeMX** | ≥6.0 | [ST官网](https://www.st.com/en/development-tools/stm32cubemx.html) | 引脚配置、时钟树、外设初始化代码生成 |
| **ARM GCC Toolchain** | 10.3+ | [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain) | 交叉编译 |
| **CMake** | ≥3.16 | `winget install CMake` 或 [cmake.org](https://cmake.org) | 构建系统 |
| **VSCode** | latest | [code.visualstudio.com](https://code.visualstudio.com) | 代码编辑与调试 |
| **STM32CubeF1** | v1.8.x | [GitHub](https://github.com/STMicroelectronics/STM32CubeF1) 或 CubeMX 自动下载 | HAL库 + CMSIS |
| **OpenOCD** | ≥0.11 | `winget install openocd` 或 [openocd.org](https://openocd.org) | 调试与烧录 |
| **J-Link / ST-Link** | - | 调试器硬件 | 代码下载与调试 |

### 1.2 VSCode 必备插件

```json
{
  "recommendations": [
    "ms-vscode.cpptools",           // C/C++ IntelliSense
    "ms-vscode.cmake-tools",        // CMake 集成
    "marus25.cortex-debug",         // ARM Cortex 调试
    "dan-c-underwood.arm",          // ARM 汇编语法高亮
    "twxs.cmake"                    // CMake 语法高亮
  ]
}
```

### 1.3 获取 STM32CubeF1 HAL 库

**方法A（推荐）**: CubeMX 自动管理 — 创建项目时 CubeMX 会自动下载对应固件包。

**方法B**: 手动下载:
```bash
# 从 GitHub 克隆
git clone https://github.com/STMicroelectronics/STM32CubeF1.git --branch v1.8.6 --depth 1

# 或者用项目内脚本
bash scripts/setup_cmsis_hal.sh
```

**方法C**: CubeMX 安装后，固件包位于:
```
C:/Users/<用户名>/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.x/
```

### 1.4 环境变量配置（建议）

```powershell
# 添加到系统 PATH:
# C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\bin
# C:\Program Files\CMake\bin
# C:\Program Files\OpenOCD\bin
```

---

## 2. CubeMX 项目配置

### 2.1 创建项目

```
1. 打开 STM32CubeMX
2. File → New Project → 搜索 STM32F103C8T6 → 选中 → Start Project
3. Project Manager 标签:
   - Project Name: bms_9s_f103
   - Project Location: D:\coding_codes\bms_learning\
   - Toolchain / IDE: 选择 "Makefile"（后续用CMake覆盖）
   - 勾选 "Generate peripheral initialization as a pair of .c/.h files"
```

### 2.2 引脚配置表（≡ 参考项目完全一致）

这是本指南最关键的配置表。**必须逐项核对**。

#### GPIOA 引脚配置

| 引脚 | CubeMX 配置 | 功能 | 用户标签 | 说明 |
|------|-----------|------|---------|------|
| **PA0** | GPIO_Output | QB 电源控制 | MCU_KZ_QB_POWER | 初始电平: Low |
| **PA1** | GPIO_Output | D 电源控制 | MCU_D_POWER | 初始电平: Low |
| **PA2** | USART2_TX | HMI 串口屏 TX | USART2_TX | 9600bps |
| **PA3** | USART2_RX | HMI 串口屏 RX | USART2_RX | 9600bps |
| PA4 | (未用) | - | - | - |
| PA5 | (未用) | - | - | - |
| PA6 | (未用) | - | - | - |
| PA7 | (未用) | - | - | - |
| **PA8** | GPIO_Output | BQ76940 唤醒 | MCU_WAKE_BQ | 初始电平: Low |
| **PA9** | USART1_TX | QT上位机 TX | USART1_TX | 115200bps |
| **PA10** | USART1_RX | QT上位机 RX | USART1_RX | 115200bps |
| **PA11** | CAN1_RX | CAN 总线 RX | CAN1_RX | 注意: 输入模式 |
| **PA12** | CAN1_TX | CAN 总线 TX | CAN1_TX | **注意**: 与 MCU_KZ_P2 冲突，CAN优先 |
| **PA13** | SYS_SWDIO | SWD 调试数据 | SWDIO | JTAG 禁用后为 SWD |
| **PA14** | SYS_SWCLK | SWD 调试时钟 | SWCLK | JTAG 禁用后为 SWD |
| PA15 | GPIO_Output | (JTAG禁用后可用) | - | 备用 |

#### GPIOB 引脚配置

| 引脚 | CubeMX 配置 | 功能 | 用户标签 | 说明 |
|------|-----------|------|---------|------|
| PB0 | GPIO_Output | LED2 指示灯 | LED2 | - |
| **PB1** | GPIO_Output | 唤醒状态 | STA_WAKE | 初始电平: Low |
| **PB2** | GPIO_Output | BQ76940 Alert 控制 | MCU_BQ1_ART | 初始电平: Low |
| **PB3** | GPIO_Output | 通用 IO | PB3 | **需要先关闭JTAG**才能用作GPIO |
| **PB4** | GPIO_Output | 通用 IO | PB4 | **需要先关闭JTAG**才能用作GPIO |
| **PB5** | GPIO_Output | 通用 IO | PB5 | - |
| PB6 | I2C1_SCL | (硬件I2C，本项目不用) | - | 参考项目使用软件I2C |
| PB7 | I2C1_SDA | (硬件I2C，本项目不用) | - | 参考项目使用软件I2C |
| **PB8** | GPIO_Output | **软件I2C SCL** → BQ76940 | I2C1_SW_SCL | 开漏输出模式 |
| **PB9** | GPIO_Output | **软件I2C SDA** → BQ76940 | I2C1_SW_SDA | 开漏输出模式 |
| PB10 | (未用/USART3_TX) | - | - | 可选扩展 |
| PB11 | (未用/USART3_RX) | - | - | 可选扩展 |
| **PB12** | GPIO_Output | 输入状态指示 | MCU_IN_STA1 | 初始电平: High |
| PB13 | SPI2_SCK | W25Qxx Flash SCK | SPI2_SCK | Flash 存储（可选） |
| PB14 | SPI2_MISO | W25Qxx Flash MISO | SPI2_MISO | Flash 存储（可选） |
| PB15 | SPI2_MOSI | W25Qxx Flash MOSI | SPI2_MOSI | Flash 存储（可选） |

#### GPIOC 引脚配置

| 引脚 | CubeMX 配置 | 功能 | 用户标签 | 说明 |
|------|-----------|------|---------|------|
| PC13 | GPIO_Output | LED1 指示灯 | LED1 | 注意: 此引脚驱动能力有限 |

> **关键注意事项**:
> 1. **PA12 冲突**: 参考项目 IO_CTRL.h 中 PA12 定义为 MCU_KZ_P2, 但 can.c 中 PA12 被配置为 CAN_TX。由于 CAN_Mode_Init() 在 IO_CTRL_Config() 之后调用，**CAN_TX 会覆盖 P2 配置**。如果你需要同时使用 P2 功能和 CAN，需将 P2 移至其他空闲引脚。
> 2. **JTAG 禁用**: PB3/PB4/PA15 在默认情况下被 JTAG 占用，需要调用 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 将其释放为普通 GPIO（代码中已包含）。
> 3. **软件 I2C 必须用开漏模式**: PB8/PB9 必须配置为 `GPIO_MODE_OUTPUT_OD`，因为 I2C 总线需要线与特性。

### 2.3 外设配置

#### USART1 — QT上位机通信
```
模式: Asynchronous
波特率: 115200
数据位: 8
停止位: 1
校验: None
硬件流控: None
中断: RXNE (USART1 global interrupt) — 使能
```

#### USART2 — HMI 串口屏
```
模式: Asynchronous
波特率: 9600
数据位: 8
停止位: 1
校验: None
硬件流控: None
```

#### CAN1 — CAN 总线通信
```
模式: Normal (生产用) / LoopBack (调试用)
预分频器 (Prescaler): 4
时间段1 (BS1): 9 tq
时间段2 (BS2): 8 tq
同步跳转宽度 (SJW): 1 tq
结果: 波特率 = 36MHz / ((9+8+1) × 4) = 500 Kbps
中断: CAN1 RX0 interrupt — 可选使能
```

#### TIM2 — 100ms 周期定时器
```
时钟源: Internal Clock
预分频器: 7199  → CNT_CLK = 72MHz / 7200 = 10KHz
自动重载: 999   → 更新周期 = 10KHz / 1000 = 10Hz → 100ms
中断: TIM2 global interrupt — 使能
```

> 用途：① USART1 接收超时检测 ② 周期性数据轮询的节拍

#### IWDG — 独立看门狗
```
预分频器: 256
重载值: 1250
超时时间: ≈ 4秒  (基于 LSI 40KHz)
```

> **必须在主循环中定期喂狗 (`HAL_IWDG_Refresh`)，否则 MCU 会复位。**

#### SPI2 — W25Qxx Flash（可选）
```
模式: Full-Duplex Master
NSS: Software (用GPIO控制片选)
数据大小: 8 bit
时钟极性 CPOL: Low
时钟相位 CPHA: 1 Edge
预分频器: 8  (SPI时钟 = 36MHz / 8 = 4.5MHz)
```

### 2.4 时钟树配置

```
外部晶振 HSE: 8 MHz
        ↓
PLL 倍频: ×9 = 72 MHz
        ↓
SYSCLK: 72 MHz ──→ AHB (HCLK): 72 MHz ──→ APB2 (PCLK2): 72 MHz (GPIO, USART1, SPI1, AFIO, ADC)
        │                                  └─── APB1 (PCLK1): 36 MHz (USART2/3, CAN, TIM2-7, IWDG, SPI2)
        ↓
APB1 定时器时钟: 36MHz ×2 = 72MHz (TIM2-7)
```

### 2.5 NVIC 中断配置

| 中断源 | 抢占优先级 | 响应优先级 | 说明 |
|--------|----------|----------|------|
| SysTick | 0 | 0 | 系统滴答 (HAL 延时) |
| USART1 | 1 | 0 | 上位机指令接收 |
| TIM2 | 2 | 0 | 100ms 定时 + 串口超时检测 |
| CAN1_RX0 | 3 | 0 | CAN 消息接收 |

NVIC 优先级分组: **2 位抢占 / 2 位响应** (`NVIC_PRIORITYGROUP_2`)

### 2.6 生成代码

```
Project Manager → Code Generator:
  ☑ Copy only the necessary library files
  ☑ Generate peripheral initialization as a pair of .c/.h files
  ☑ Set all free pins as analog (降低功耗)

点击 "GENERATE CODE" → 生成 Makefile 项目
```

> CubeMX生成的 Makefile 项目只是起点。我们会用 CMake 覆盖构建系统。

---

## 3. CMake 工程搭建

### 3.1 目录结构

```
bms_9s_f103/
├── CMakeLists.txt              # 顶层 CMake 构建脚本
├── STM32F103C8Tx_FLASH.ld     # GCC 链接脚本
├── DEVELOPMENT_GUIDE.md        # 本指导文档
├── README.md                   # 项目说明
│
├── .vscode/                    # VSCode 配置
│   ├── c_cpp_properties.json   # IntelliSense 配置
│   ├── launch.json             # 调试启动配置
│   └── tasks.json              # 构建任务
│
├── Core/                       # 内核相关（CubeMX生成）
│   ├── Inc/
│   │   ├── main.h              # 主头文件 + 引脚宏定义
│   │   ├── stm32f1xx_hal_conf.h # HAL 模块配置
│   │   └── stm32f1xx_it.h     # 中断处理声明
│   └── Src/
│       ├── main.c              # 入口：初始化 → 主循环
│       ├── stm32f1xx_it.c      # 中断服务函数 (SysTick, USART1, TIM2)
│       ├── stm32f1xx_hal_msp.c # HAL 底层外设初始化
│       └── system_stm32f1xx.c  # 系统初始化（时钟配置）
│
├── Drivers/                    # CMSIS + HAL（由脚本/手动下载）
│   ├── CMSIS/
│   │   ├── Include/            # core_cm3.h 等
│   │   └── Device/ST/STM32F1xx/
│   │       ├── Include/        # stm32f1xx.h, system_stm32f1xx.h
│   │       └── Source/Templates/gcc/
│   │           └── startup_stm32f103xb.s  # 启动文件
│   └── STM32F1xx_HAL_Driver/
│       ├── Inc/                # HAL 头文件
│       └── Src/                # HAL 实现
│
├── BSP/                        # 板级支持包（从参考项目移植）
│   ├── Inc/
│   │   ├── io_ctrl.h           # GPIO 控制宏 + 初始化声明
│   │   ├── i2c_sw.h            # 软件 I2C 驱动（BQ76940 专用）
│   │   ├── bq76940.h           # BQ76940 寄存器定义 + API
│   │   ├── can_drv.h           # CAN 驱动
│   │   ├── usart_drv.h         # 串口驱动（USART1 + USART2）
│   │   ├── systick.h           # 微秒/毫秒延时
│   │   ├── led.h               # LED 指示灯
│   │   ├── timer.h             # TIM2 定时器
│   │   └── wdg.h               # IWDG 看门狗
│   └── Src/
│       ├── io_ctrl.c           # [待实现] GPIO 初始化
│       ├── i2c_sw.c            # [待实现] 软件 I2C + CRC8
│       ├── bq76940.c           # [待实现] BQ76940 驱动核心
│       ├── can_drv.c           # [待实现] CAN 收发
│       ├── usart_drv.c         # [待实现] 串口驱动
│       ├── systick.c           # [待实现] 延时函数
│       ├── led.c               # [待实现] LED 控制
│       ├── timer.c             # [待实现] TIM2 初始化
│       └── wdg.c               # [待实现] IWDG 操作
│
├── App/                        # 应用层
│   ├── Inc/
│   │   ├── bms_app.h           # BMS 数据采集调度
│   │   ├── protection.h        # 保护逻辑判断
│   │   └── data_report.h       # 数据上报 (UART/CAN/HMI)
│   └── Src/
│       ├── bms_app.c           # [待实现] 主数据采集流程
│       ├── protection.c        # [待实现] OV/UV/OC/OT 判断
│       └── data_report.c       # [待实现] 数据打包发送
│
├── scripts/
│   └── setup_cmsis_hal.sh      # CMSIS/HAL 自动下载脚本（Windows 用户用 Git Bash 运行）
│
└── build/                      # 构建输出目录 (gitignore)
```

### 3.2 CMakeLists.txt 模板

项目已提供完整的 `CMakeLists.txt`，关键配置说明：

```cmake
# ===== 工具链 =====
set(TOOLCHAIN_PREFIX arm-none-eabi-)   # GCC ARM 工具链前缀

# ===== MCU 编译标志 =====
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb")  # Cortex-M3, Thumb指令集
add_compile_definitions(STM32F103xB)       # 芯片型号宏（C8T6 = 中等密度 B）

# ===== 源文件组织 =====
# CMSIS_CORE_SRCS    → system_stm32f1xx.c
# STARTUP_ASM        → startup_stm32f103xb.s
# HAL_SRCS           → 按需包含的 HAL 模块（见下）
# CORE_SRCS          → main.c, stm32f1xx_it.c, stm32f1xx_hal_msp.c
# BSP_SRCS           → BSP 层所有 .c 文件
# BQ76940_SRCS       → bq76940.c
# APP_SRCS           → 应用层所有 .c 文件

# ===== 链接脚本 =====
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${CMAKE_SOURCE_DIR}/STM32F103C8Tx_FLASH.ld
)

# ===== 生成 hex/bin =====
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ...   # 生成 .hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ...  # 生成 .bin
)
```

### 3.3 按需包含 HAL 模块

你只需要编译实际使用的 HAL 源文件。本项目需要的最小集合：

```
必须:
  stm32f1xx_hal.c              # HAL 核心
  stm32f1xx_hal_cortex.c       # NVIC, SysTick
  stm32f1xx_hal_rcc.c          # 时钟
  stm32f1xx_hal_gpio.c         # GPIO
  stm32f1xx_hal_flash.c        # Flash (参数存储)

按需:
  stm32f1xx_hal_dma.c          # 如果 USART 使用 DMA
  stm32f1xx_hal_uart.c         # USART1 + USART2
  stm32f1xx_hal_can.c          # CAN
  stm32f1xx_hal_tim.c          # TIM2
  stm32f1xx_hal_spi.c          # SPI (W25Qxx Flash)
  stm32f1xx_hal_iwdg.c         # IWDG 看门狗
  stm32f1xx_hal_pwr.c          # 电源管理
```

### 3.4 构建流程

```bash
# 1. 确保 CMSIS/HAL 已下载
bash scripts/setup_cmsis_hal.sh

# 2. 配置并构建
mkdir build && cd build
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# 3. 产物
# build/BMS_9S_F103.elf   → ELF 可执行文件
# build/BMS_9S_F103.hex   → HEX 烧录文件
# build/BMS_9S_F103.bin   → BIN 烧录文件
# build/BMS_9S_F103.map   → 内存映射文件
```

---

## 4. 代码架构与分层设计

### 4.1 分层架构图

```
┌─────────────────────────────────────────────────────┐
│                   main.c (入口+主循环)                │
│  • 初始化各模块                                       │
│  • while(1) { 喂狗; 接收指令; 采集数据; 保护判断; }    │
└────────┬─────────────────────────────┬────────────────┘
         │                             │
    ┌────▼────┐                  ┌─────▼──────┐
    │ App 层  │                  │ App 层      │
    │ bms_app │                  │ protection  │
    │ (调度)   │                  │ (保护判断)   │
    └────┬────┘                  └─────┬──────┘
         │                             │
    ┌────▼─────────────────────────────▼────┐
    │            App 层: data_report         │
    │  数据打包 → USART1 / USART2 / CAN      │
    └────────────────────┬───────────────────┘
                         │
    ┌────────────────────▼───────────────────┐
    │           BSP 层: bq76940              │
    │  • 电压采集  • 温度/电流  • SOC        │
    │  • 保护配置  • FET控制   • 均衡        │
    └────────────────────┬───────────────────┘
                         │
    ┌────────────────────▼───────────────────┐
    │         BSP 层: 硬件抽象驱动             │
    │  i2c_sw │ can_drv │ usart_drv │ timer  │
    │  systick │ io_ctrl │ led       │ wdg   │
    └────────────────────┬───────────────────┘
                         │
    ┌────────────────────▼───────────────────┐
    │   HAL 库 + CMSIS (ST 官方提供)          │
    │  GPIO / UART / CAN / TIM / IWDG / SPI  │
    └─────────────────────────────────────────┘
```

### 4.2 数据流图

```
                    ┌──────────────┐
                    │   BQ76940    │
                    │   (AFE芯片)   │
                    └──────┬───────┘
                           │ I2C (软件, PB8/PB9)
                    ┌──────▼───────┐
                    │   i2c_sw.c   │
                    │ + CRC8 校验   │
                    └──────┬───────┘
                           │
              ┌────────────▼────────────┐
              │      bq76940.c          │
              │  • Get_Battery1..15()   │
              │  • Get_BQ_Current()     │
              │  • Get_BQ1_2_Temp()     │
              │  • Get_SOC()            │
              │  • Get_Update_ALL_Data()│
              └──┬──────────┬───────────┘
                 │          │
          ┌──────▼──┐ ┌────▼──────┐
          │ 保护判断 │ │ 数据上报   │
          │protection│ │data_report│
          │ .c       │ │ .c        │
          └──────┬──┘ └─┬───┬───┬─┘
                 │      │   │   │
          FET控制│  USART1  │ USART2
          (CHG/   │  (QT)    │ (HMI)
          DSG)    │          │
                  │      CAN (CAN网络)
```

### 4.3 模块依赖关系（编译顺序无关，但初始化顺序有要求）

```
初始化顺序（main.c 中必须按此顺序调用）:
  1. HAL_Init()              # HAL 内核
  2. SystemClock_Config()    # 时钟树
  3. SYSTICK_Init()          # 延时函数（使用 SysTick）
  4. LED_GPIO_Config()       # LED（调试用，尽早亮）
  5. IO_CTRL_Config()        # GPIO 控制引脚
  6. USART1_Init(115200)     # QT上位机通信
  7. USART2_Init(9600)       # HMI串口屏
  8. I2C_SW_Init()           # 软件 I2C（PB8/PB9）
  9. CAN_Init()              # CAN 总线
  10. BQ76940_Config()       # BQ76940 初始配置（需要通过 I2C）
  11. TIM2_Config()          # 100ms 定时器
  12. IWDG_Config()          # 看门狗（最后使能）
```

---

## 5. BSP 驱动层编写流程

### 5.1 编写规范

每个 BSP 驱动模块遵循统一的开发流程：

```
1. 读取参考项目对应的 .c/.h 文件
2. 在 Inc/ 创建对应的 .h 头文件:
   - 包含 HAL 头 (#include "stm32f1xx_hal.h")
   - 定义宏（引脚、常量、参数）
   - 声明函数原型
3. 在 Src/ 创建对应的 .c 实现文件:
   - 包含自己的头文件
   - 用 HAL API 重写 StdPeriph 代码
   - 保持函数签名与原项目一致
4. 实现完成后立即编译验证
```

### 5.2 io_ctrl.c — GPIO 控制

**参考文件**: `BMS_s940/BSP/IO_CTRL.h/c`

**代码模板**（已提供完整头文件）:

```c
// io_ctrl.c - 编写步骤:

#include "io_ctrl.h"

void IO_CTRL_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // ===== 第1步: 使能 GPIO 时钟 =====
    __HAL_RCC_GPIOA_CLK_ENABLE();  // 替代 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // ===== 第2步: 逐个配置引脚（参考 io_ctrl.h 中的宏定义）=====
    // 每个引脚:
    //   GPIO_InitStruct.Pin   = <引脚宏>;
    //   GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  // 推挽输出
    //   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // 50MHz
    //   HAL_GPIO_Init(<PORT>, &GPIO_InitStruct);

    // 示例: PA0 → MCU_KZ_QB_POWER
    GPIO_InitStruct.Pin   = MCU_KZ_QB_POWER_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // ... 按引脚表逐个配置所有 PA0, PA1, PA8, PA12,
    //     PB12, PB1, PB2, PB3, PB4, PB5 ...

    // ===== 第3步: 关闭 JTAG，释放 PB3/PB4/PA15 =====
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();  // 替代 GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)

    // ===== 第4步: 设置默认电平 =====
    MCU_IN_STA1_ONOFF(1);  // 默认高
    MCU_KZ_P2_ONOFF(0);    // 默认低
    PB3_ONOFF(0);
    PB4_ONOFF(0);
    PB5_ONOFF(0);
}
```

### 5.3 i2c_sw.c — 软件 I2C（BQ76940 专用，含 CRC）

**参考文件**: `BMS_s940/BSP/i2c1.h/c`, `BMS_s940/BSP/BQ76930.c` (CRC8函数)

**为什么用软件I2C而不是硬件I2C？**
- BQ76940 的 I2C 写操作需要在最后一个字节后附加 CRC8 校验码
- 标准的 STM32 硬件 I2C 模块无法在 stop 前自动插入 CRC
- 软件模拟 I2C 可以自由控制时序

**编写步骤**:

```c
// i2c_sw.c - 分5个子模块实现:

// ===== 模块1: GPIO 初始化 =====
void I2C_SW_Init(void)
{
    // PB8(SCL), PB9(SDA) 配置为开漏输出
    // Mode = GPIO_MODE_OUTPUT_OD
    // 初始电平: 两个引脚都拉高 (I2C 空闲状态)
}

// ===== 模块2: I2C 基本时序 =====
// 需要实现以下静态函数:
//   static void i2c_delay(void)     // ~5us 延时（调整此函数可调 I2C 速度）
//   static void i2c_start(void)     // 起始信号: SDA↓→SCL↓
//   static void i2c_stop(void)      // 停止信号: SCL↑→SDA↑
//   static void i2c_send_byte(uint8_t byte)  // 发送8位 + 等待ACK
//   static uint8_t i2c_read_byte(uint8_t ack) // 读取8位 + 发送ACK/NACK

// ===== 模块3: 标准读写（不带CRC，用于读操作）=====
uint8_t I2C_SW_ReadByte(uint16_t reg_addr)
{
    // 1. Start
    // 2. 发送设备地址 (0x08 << 1) = 0x10 (写模式)
    // 3. 发送寄存器地址
    // 4. Restart
    // 5. 发送设备地址 (0x08 << 1 | 0x01) = 0x11 (读模式)
    // 6. 读取1字节 + NACK
    // 7. Stop
    // 8. 返回读取值
}

void I2C_SW_WriteByte(uint16_t reg_addr, uint8_t data)
{
    // 1. Start
    // 2. 发送设备地址 0x10
    // 3. 发送寄存器地址
    // 4. 发送数据
    // 5. Stop
    // 注意: 此函数不带CRC，仅用于不要求CRC的寄存器
}

// ===== 模块4: CRC8 写（BQ76940 关键功能）=====
void I2C_SW_WriteByteCRC(uint16_t reg_addr, uint8_t data)
{
    // BQ76940 的 I2C CRC 写协议:
    //   帧格式: [设备地址(W)] [寄存器地址] [数据] [CRC8]
    //
    // 实现步骤:
    //   1. 构建设备地址: 0x08 << 1 = 0x10 (7位地址左移1位)
    //   2. 构建CRC计算缓冲区: {0x10, reg_addr, data} (3字节)
    //   3. 计算CRC8: CRC8(buffer, 3, 0x07)
    //   4. Start → 发送0x10 → 发送reg_addr → 发送data → 发送CRC → Stop
    //   5. 延时10ms (BQ76940 要求写操作间隔≥10ms)
}

// ===== 模块5: CRC8 算法 =====
uint8_t CRC8(uint8_t *ptr, uint8_t len, uint8_t key)
{
    // CRC-8 多项式: x^8 + x^2 + x + 1 (key=0x07)
    // 初始值: 0x00
    //
    // 伪代码:
    //   crc = 0
    //   for each byte in data:
    //       for bit in 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01:
    //           if (crc & 0x80): crc = (crc << 1) ^ key
    //           else:            crc = crc << 1
    //           if (byte & bit): crc ^= key
    //   return crc

    // 完整实现见参考文件 BMS_s940/BSP/BQ76930.c 第1112-1134行
}
```

**关键时序参数**:
| 参数 | 值 | 说明 |
|------|---|------|
| I2C 时钟频率 | ~100KHz (标准模式) | 通过 i2c_delay() 调整 |
| 写操作间隔 | ≥10ms | BQ76940 要求，由延时保证 |
| 起始信号保持时间 | ≥4.7μs | SDA↓ → SCL↓ |

### 5.4 usart_drv.c — 串口驱动

**参考文件**: `BMS_s940/BSP/usart.c`, `usart2.c`

**编写分为两部分**:

#### 第一部分: USART1 — QT上位机 (115200bps)

```c
// 初始化: 使用 HAL_UART_Init()
//   - Instance = USART1
//   - BaudRate = 115200
//   - WordLength = UART_WORDLENGTH_8B
//   - StopBits = UART_STOPBITS_1
//   - Parity = UART_PARITY_NONE

// 发送: UartSend(char *str)
//   → HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 1000);

// 接收: 使用中断方式
//   → HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
//   → 在 USART1_IRQHandler 中调用 HAL_UART_IRQHandler
//   → 在 HAL_UART_RxCpltCallback 中将收到的字节存入环形缓冲区

// USART1 数据帧格式 (参考项目的协议):
//   - 帧头: 0x01 0x02 0x55 → 开始发送 BMS 数据
//   - 帧头: 0x01 0x03 0x55 → 停止发送 BMS 数据
//   - 帧头: 0x01 0x04 0x55 → 只打开 DSG FET
//   - 帧头: 0x01 0x05 0x55 → 只关闭 DSG FET
//   - 帧头: 0x01 0x06 0x55 → 只打开 CHG FET
//   - 帧头: 0x01 0x07 0x55 → 只关闭 CHG FET
//   - 帧头: 0x01 0x12 0x55 + 2B → 写入电池参数到Flash
//   - 帧头: 0x01 0x13 0x55 + 2B → 写入温度上限到Flash
//   - 帧头: 0x01 0x14 0x55 → 读取Flash中的参数
```

#### 第二部分: USART2 — HMI 串口屏 (9600bps)

```c
// 初始化: 类似 USART1，但波特率为 9600

// HMI 通信协议 (参考项目使用淘晶驰/维控等串口屏):
//   DCV16(x, y, 'text', font, value, unit)  →  显示数值
//   CLR(layer)                              →  清屏
//   DIR(1)                                  →  设置显示方向
//
// 示例:
//   sprintf(buf, "DCV16(0,0,'%s%d%s',3);\r\n", "第一节电压:", voltage, "mV");
//   USART2_SendStr(buf);
```

### 5.5 can_drv.c — CAN 驱动

**参考文件**: `BMS_s940/BSP/can.c`

```c
// 初始化: 使用 HAL_CAN_Init()
//   - Instance = CAN1
//   - Prescaler = 4
//   - SJW = CAN_SJW_1TQ
//   - BS1 = CAN_BS1_9TQ
//   - BS2 = CAN_BS2_8TQ
//   - Mode = CAN_MODE_NORMAL

// 发送 (扩展帧):
uint32_t Can_Send_Msg(uint8_t *msg, uint8_t len, uint32_t appid)
{
    // 1. 配置 TxHeader:
    //    - ExtId = appid           (扩展ID)
    //    - IDE = CAN_ID_EXT         (扩展帧)
    //    - RTR = CAN_RTR_DATA       (数据帧)
    //    - DLC = len                (数据长度)
    // 2. HAL_CAN_AddTxMessage(&hcan1, &TxHeader, msg, &TxMailbox)
    // 3. 等待发送完成或超时
}

// 接收:
uint8_t Can_Receive_Msg(uint8_t *buf)
{
    // 1. HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) 检查是否有数据
    // 2. HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, buf)
    // 3. 返回 DLC
}

// CAN 消息分配表（7帧，每帧8字节）:
//   ID 0x0001: [0xAA] [0x01] [Cell1_H] [Cell1_L] [Cell2_H] [Cell2_L] [Cell3_H] [Cell3_L]
//   ID 0x0002: [0xAA] [0x02] [Cell4_H] [Cell4_L] [Cell5_H] [Cell5_L] [Cell6_H] [Cell6_L]
//   ID 0x0003: [0xAA] [0x03] [Cell7_H] [Cell7_L] [Cell8_H] [Cell8_L] [Cell9_H] [Cell9_L]
//   ID 0x0004: [0xAA] [0x04] ...
//   ID 0x0005: [0xAA] [0x05] ...
//   ID 0x0006: [0xAA] [0x06] [TotalVolt_H] [TotalVolt_L] [SOC_H] [SOC_L] [Curr_H] [Curr_L]
//   ID 0x0007: [0xAA] [0x07] [Temp_H] [Temp_L] [DSG_STA] [CHG_STA] [0x00] [0x00]
```

### 5.6 systick.c, led.c, timer.c, wdg.c — 辅助驱动

这四个模块相对简单，每个按如下流程编写：

#### systick.c
```
参考: BMS_s940/BSP/systick.c
核心: 用 SysTick 实现微秒/毫秒延时
HAL API:
  - SysTick_Config(SystemCoreClock / 1000) → 1ms 中断
  - 全局变量递减计数器实现 delay_ms()
  - 微秒延时用循环查询 SysTick->VAL
```

#### led.c
```
参考: BMS_s940/BSP/led.c
功能: 5个 LED 的 GPIO 控制
注意: PC13 是反逻辑 (低电平点亮)
```

#### timer.c
```
参考: BMS_s940/BSP/timer.c
功能: TIM2 配置为 100ms 周期定时器
HAL API:
  - HAL_TIM_Base_Init(&htim2)
  - HAL_TIM_Base_Start_IT(&htim2)  → 使能更新中断
  - TIM2_IRQHandler → HAL_TIM_IRQHandler → HAL_TIM_PeriodElapsedCallback
```

#### wdg.c
```
参考: BMS_s940/BSP/wdg.c
功能: IWDG 看门狗初始化 + 喂狗
HAL API:
  - HAL_IWDG_Init(&hiwdg) → 配置 4s 超时
  - HAL_IWDG_Refresh(&hiwdg) → 喂狗 (主循环中调用)
```

---

## 6. BQ76940 驱动编写流程

这是整个项目的**核心模块**，约 1100+ 行代码。建议分步实现、逐步验证。

### 6.1 编写顺序

```
第1步: 头文件 bq76940.h (已完成 → 直接参考)
第2步: CRC8 函数
第3步: ADC 校准 (Get_offset)
第4步: 单个电池电压读取 (Get_Battery1 作为模板)
第5步: 其余8个电池电压读取
第6步: 总电压 + SOC 估算
第7步: 电流 + 温度读取
第8步: BQ76940 初始化配置 (BQ_1_Config)
第9步: 保护阈值设置 (OV_UV_1_PROTECT)
第10步: FET 控制 (Open/Close CHG/DSG)
第11步: 数据打包 (Get_Update_Data + Update_val)
```

### 6.2 关键算法详解

#### 6.2.1 电压计算

```c
// ===== 步骤1: 从 BQ76940 读取 ADC 校准值 =====
void Get_offset(void)
{
    uint8_t gain[2];

    // 读取 ADC 增益寄存器
    gain[0] = I2C_SW_ReadByte(BQ_ADCGAIN1);  // 0x50
    gain[1] = I2C_SW_ReadByte(BQ_ADCGAIN2);  // 0x59

    // 计算 ADC_GAIN (12uV 步进)
    ADC_GAIN = ((gain[0] & 0x0C) << 1) + ((gain[1] & 0xE0) >> 5);

    // 读取 ADC 偏移 (45mV 步进)
    ADC_offset = I2C_SW_ReadByte(BQ_ADCOFFSET);  // 0x51

    // 最终增益值 (uV/LSB)
    GAIN = 365 + ADC_GAIN;  // 典型值 ~377uV/LSB
}

// ===== 步骤2: 读取并转换电池电压 =====
// 以 Cell 1 为例:
void Get_Battery1(void)
{
    uint16_t adc_raw;
    int16_t  voltage;

    // 读取 14-bit ADC 值 (两个连续寄存器)
    uint8_t hi = I2C_SW_ReadByte(BQ_VC1_HI);  // 0x0C
    uint8_t lo = I2C_SW_ReadByte(BQ_VC1_LO);  // 0x0D

    adc_raw = ((uint16_t)hi << 8) | lo;

    // 电压计算公式 (参考 BQ76940 datasheet):
    //   V_cell(mV) = (ADC_raw × GAIN) / 1000 + ADC_offset
    //   其中 GAIN 单位是 uV/LSB, /1000 转为 mV
    voltage = (int16_t)(((int32_t)adc_raw * GAIN) / 1000) + ADC_offset;

    Batteryval[0] = voltage;  // 保存到全局数组

    // 打包到 CAN 和 UART 缓冲区
    can_buf1[2] = HIGH_BYTE(voltage);  // 高8位
    can_buf1[3] = LOW_BYTE(voltage);   // 低8位
}

// ===== 9节电池的寄存器映射 =====
//   Cell 1  → 0x0C/0x0D → Batteryval[0]
//   Cell 2  → 0x0E/0x0F → Batteryval[1]
//   Cell 5  → 0x14/0x15 → Batteryval[4]  ← 注意索引不连续!
//   Cell 6  → 0x16/0x17 → Batteryval[5]
//   Cell 7  → 0x18/0x19 → Batteryval[6]
//   Cell 10 → 0x1E/0x1F → Batteryval[9]
//   Cell 11 → 0x20/0x21 → Batteryval[10]
//   Cell 12 → 0x22/0x23 → Batteryval[11]
//   Cell 15 → 0x28/0x29 → Batteryval[14]
//
//   总电压  → 全部9节求和 → Batteryval[15]
//   SOC     → 查表法      → Batteryval[16]
//   电流    → 库仑计数器   → Batteryval[17]
//   温度    → TS1 NTC计算  → Batteryval[18]
```

> **重要**: 9节电池的物理连接在 BQ76940 的 VC1/2/5/6/7/10/11/12/15 通道上（共15个可能的通道，你只连接了其中9个）。因此需要在代码中**明确指定要读取哪些电池**，不要循环遍历全部15个通道。

#### 6.2.2 电流计算

```c
void Get_BQ_Current(void)
{
    uint16_t cc_raw;
    float    current;

    uint8_t hi = I2C_SW_ReadByte(BQ_CC_HI);  // 0x32
    uint8_t lo = I2C_SW_ReadByte(BQ_CC_LO);  // 0x33

    cc_raw = ((uint16_t)hi << 8) | lo;

    // 电流计算 (参考项目公式，4mΩ 采样电阻 / 8倍增益):
    //   正向电流 (放电): current = cc_raw × 2.11  (单位: mA)
    //   反向电流 (充电): current = (0xFFFF - cc_raw) × 2.11
    //
    // 判断方向:
    if (cc_raw <= 0x7D00) {
        current = cc_raw * 2.11f;            // 放电 → 正值
    } else {
        current = (0xFFFF - cc_raw) * 2.11f; // 充电 → 负值
    }

    Batteryval[17] = (int16_t)current;
}
```

#### 6.2.3 温度计算

```c
void Get_BQ1_2_Temp(void)
{
    // 使用 TS1 通道 (CFETOFF 引脚上的 103AT NTC 热敏电阻)
    // NTC 参数: R25 = 10KΩ, B = 3380K

    uint16_t ts_raw = (I2C_SW_ReadByte(BQ_TS1_HI) << 8)
                     | I2C_SW_ReadByte(BQ_TS1_LO);

    // 步骤1: 将 ADC 值转为 NTC 电阻值
    //   R_ntc = (10000 × (ts_raw × 382 / 1000)) / (3300 - (ts_raw × 382 / 1000))
    //   其中 382 = GAIN (uV/LSB), 3300 = 参考电压 (mV)

    float Rt = (10000.0f * (ts_raw * 382.0f / 1000.0f))
             / (3300.0f - (ts_raw * 382.0f / 1000.0f));

    // 步骤2: B参数公式转温度
    //   T(K) = 1 / (1/T2 + ln(Rt/Rp) / B)
    //   T(°C) = T(K) - 273.15
    //
    //   其中: T2 = 273.15 + 25 = 298.15K (25°C)
    //         Rp = 10000Ω (25°C时的标称电阻)
    //         B  = 3380K

    float T_K = 1.0f / (1.0f / 298.15f + logf(Rt / 10000.0f) / 3380.0f);
    float Temp = T_K - 273.15f + 0.5f;  // +0.5 四舍五入

    Batteryval[18] = (int16_t)Temp;
}
```

#### 6.2.4 SOC 估算（电压查表法）

```c
void Get_SOC(void)
{
    // 基于总电压 / 9节 的平均电压查表
    // 此方法精度有限（±10%），适用于无库仑计的简化方案
    //
    // 电压-SOC 对应表 (典型三元锂电池):
    //   >4100mV/cell → 100%
    //   4050-4100    → 90%
    //   4000-4050    → 80%
    //   ...
    //   3100-3200    → 5%
    //   <3100        → 0%

    int total_mv = Batteryval[15];  // 总电压
    int cell_count = 9;

    if (total_mv > 4100 * cell_count)       SOC = 100;
    else if (total_mv > 4050 * cell_count)   SOC = 90;
    else if (total_mv > 4000 * cell_count)   SOC = 80;
    // ... 参考 findings.md 中的完整表格 ...
    else if (total_mv > 3100 * cell_count)   SOC = 5;
    else                                      SOC = 0;

    Batteryval[16] = SOC;
}
```

> **改进建议**: SOC 估算可以使用库仑计数 + 电压融合的扩展卡尔曼滤波方法，但需要更复杂的算法和校准数据。当前简化的查表法适用于原型验证阶段。

#### 6.2.5 BQ76940 初始化配置序列

```c
void BQ_1_Config(void)
{
    // 寄存器初始化表 (共12个寄存器):
    // 数组格式: {寄存器地址, 配置值}
    //
    // BQ769_INITAdd[12] = {
    //    SYS_STAT,  CELLBAL1, CELLBAL2, CELLBAL3,
    //    SYS_CTRL1, SYS_CTRL2, PROTECT1, PROTECT2,
    //    PROTECT3,  OV_TRIP,  UV_TRIP,  CC_CFG
    // };
    //
    // BQ769_INITdata[12] = {
    //    0xFF,  // SYS_STAT  → 清除所有状态位
    //    0x00,  // CELLBAL1 → 关闭均衡1
    //    0x00,  // CELLBAL2 → 关闭均衡2
    //    0x00,  // CELLBAL3 → 关闭均衡3
    //    0x18,  // SYS_CTRL1→ ADC使能, 退出SHIP模式
    //           //   bit[4] ADC_EN = 1
    //           //   bit[3] SHUT = 0 (不在shutdown)
    //    0x43,  // SYS_CTRL2→ DSG=1(放电MOS开), CHG=1(充电MOS开)
    //    0x00,  // PROTECT1→ 暂不配置(稍后由OCD_SCD_PROTECT设置)
    //    0x00,  // PROTECT2→ 同上
    //    0x00,  // PROTECT3
    //    0x00,  // OV_TRIP → 稍后由OV_UV_1_PROTECT计算设置
    //    0x00,  // UV_TRIP → 同上
    //    0x19   // CC_CFG  → 库仑计配置
    //         //   bit[4] CC_EN = 1
    //         //   bit[3:0] gain = 1 (8x)
    // };

    // 写入每个寄存器 (使用CRC写入):
    for (int i = 0; i < 12; i++) {
        HAL_Delay(10);  // 写操作间隔 ≥10ms
        I2C_SW_WriteByteCRC(BQ769_INITAdd[i], BQ769_INITdata[i]);
    }
}

// 完整初始化流程:
void BQ76940_Config(void)
{
    WAKE_ALL_DEVICE();       // 1. 唤醒: PA8脉冲
    BQ_1_Config();           // 2. 写12个配置寄存器
    Get_offset();            // 3. 读取ADC校准值
    OV_UV_1_PROTECT();       // 4. 配置过压/欠压保护阈值
    OCD_SCD_PROTECT();       // 5. 配置短路/过流保护
    I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);  // 6. 清除所有告警
}
```

#### 6.2.6 FET 控制

```c
// SYS_CTRL2 寄存器 (地址 0x05):
//   bit[6] DSG_ON: 1=放电MOS导通
//   bit[5] CHG_ON: 1=充电MOS导通
//
//   0x43 = 0b0100_0011 → DSG=1, CHG=1 (双开)
//   0x40 = 0b0100_0000 → DSG=0, CHG=0 (双关)
//   0x42 = 0b0100_0010 → DSG=1, CHG=0 (只开放电)
//   0x41 = 0b0100_0001 → DSG=0, CHG=1 (只开充电)

void Open_DSG_CHG(void) {
    I2C_SW_WriteByteCRC(BQ_SYS_CTRL2, 0x43);  // 双开
}
void Close_DSG_CHG(void) {
    I2C_SW_WriteByteCRC(BQ_SYS_CTRL2, 0x40);  // 双关
}
// 同理: Only_Open_CHG, Only_Close_CHG, Only_Open_DSG, Only_Close_DSG
```

---

## 7. 应用层编写流程

### 7.1 main.c — 入口与主循环

```c
// ===== main.c 编写模板 =====

// 头文件包含 (按需):
#include "main.h"
#include "led.h"         // LED 指示灯
#include "wdg.h"          // IWDG 看门狗
#include "systick.h"      // 延时
#include "usart_drv.h"    // 串口驱动
#include "i2c_sw.h"       // 软件 I2C
#include "bq76940.h"      // BQ76940 驱动
#include "io_ctrl.h"      // GPIO 控制
#include "timer.h"        // TIM2
#include "can_drv.h"      // CAN

// ===== 全局变量 =====
int     Batteryval[50] = {0};
uint8_t BMS_DATA_FLAG   = 0;
uint8_t OV_FLAG         = 0;
uint8_t UV_FLAG         = 0;
uint8_t OC_FLAG         = 0;

// ===== Flash 参数存储地址 (用于保存配置) =====
#define FLASH_BATTERY_ADDR  0x08007800U  // 电池参数存储地址
#define FLASH_TEMP_ADDR     0x08007C00U  // 温度上限存储地址

// ===== 主函数 =====
int main(void)
{
    // ---------- 第1部分: 系统初始化 ----------
    HAL_Init();
    SystemClock_Config();              // 配置时钟 (HSE → PLL → 72MHz)
    SYSTICK_Init();                    // SysTick 1ms
    LED_GPIO_Config();                 // LED 初始化
    IO_CTRL_Config();                  // GPIO 控制引脚
    USART1_Init(115200);               // QT上位机通信
    USART2_Init(9600);                 // HMI 串口屏
    I2C_SW_Init();                     // 软件 I2C (PB8/PB9 → BQ76940)
    CAN_Init();                        // CAN 总线 (500Kbps)

    // ---------- 第2部分: BQ76940 初始化 ----------
    BQ76940_Config();                  // 初始化 BQ76940 + 保护配置
    LED4_ONOFF(1);                     // 亮灯指示初始化完成

    // ---------- 第3部分: 定时器 + 看门狗 ----------
    TIM2_Config(TIM2_PERIOD, TIM2_PRESCALER);  // 100ms 定时器
    IWDG_Config(IWDG_PRESCALER_256, 1250);     // 4s 看门狗

    // ---------- 第4部分: 发送 HMI 初始化命令 ----------
    UartSend("MODE_CFG(1);DIR(1);\r\n");  // 配置串口屏
    HAL_Delay(1000);
    UartSend("CLR(61);\r\n");             // 清屏

    // ---------- 第5部分: 主循环 ----------
    while (1)
    {
        IWDG_Feed();                     // 喂狗
        LEDXToggle(1);                   // 翻转 LED1 (心跳指示)

        // ---- 5a. 接收 CAN 消息 ----
        uint8_t canbuf[8];
        if (Can_Receive_Msg(canbuf)) {
            // 处理接收到的 CAN 命令
        }

        // ---- 5b. 接收并处理串口指令 ----
        RECEIVE_DATA_DEAL();             // 解析 USART1 命令

        // ---- 5c. 采集 BMS 数据 ----
        if (BMS_DATA_FLAG) {
            Get_Update_Data();           // 读取所有电池数据 + 发送
        }

        // ---- 5d. 软件保护判断 ----
        Check_Protection();              // OV/UV/OC/OT 判断 + FET 控制
    }
}

// ===== 保护判断函数 =====
void Check_Protection(void)
{
    // ===== 过压保护 (软件层，>4200mV) =====
    if (any_cell_voltage > 4200) {
        Only_Close_CHG();                // 关闭充电 FET
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
        OV_FLAG = 1;
    }
    if (OV_FLAG && all_cells_below(4100)) {
        Only_Open_CHG();                 // 恢复充电
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
        OV_FLAG = 0;
    }

    // ===== 欠压保护 (软件层，<2800mV) =====
    if (any_cell_voltage < 2800) {
        Only_Close_DSG();                // 关闭放电 FET
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
        UV_FLAG = 1;
    }
    if (UV_FLAG && all_cells_above(2800)) {
        Only_Open_DSG();                 // 恢复放电
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
        UV_FLAG = 0;
    }

    // ===== 过流保护 (>2000mA) =====
    if (Batteryval[17] > 2000) {
        Close_DSG_CHG();
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
        OC_FLAG = 1;
    }
    if (OC_FLAG && Batteryval[17] < 2000) {
        Open_DSG_CHG();
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
        OC_FLAG = 0;
    }

    // ===== 过温保护 (温度阈值从 Flash 读取) =====
    int temp_limit = Read_Flash(FLASH_TEMP_ADDR);
    if (temp_limit == 0xFFFF) temp_limit = 55;  // Flash 默认值
    if (Batteryval[18] > temp_limit) {
        Close_DSG_CHG();
        I2C_SW_WriteByteCRC(BQ_SYS_STAT, 0xFF);
    }
}
```

### 7.2 protection.c — 保护逻辑模块

```c
// 将保护判断从 main.c 中抽离为独立模块
// 函数接口:
//   void Protection_Check(void);
//   uint8_t Protection_GetOVFlag(void);
//   uint8_t Protection_GetUVFlag(void);
//   void Protection_ClearAlarm(void);

// 保护判断的核心是遍历 Batteryval 数组中的9个电压值
// 检查索引: [0], [1], [4], [5], [6], [9], [10], [11], [14]
```

### 7.3 data_report.c — 数据上报模块

```c
// 将数据打包发送从 bq76940.c 中抽离
// 函数接口:
//   void DataReport_SendAll(void);
//     → 通过 USART1 (QT上位机) 发送原始数据帧
//     → 通过 USART2 (HMI串口屏) 发送显示指令
//     → 通过 CAN 发送7帧数据

// CAN 数据帧组织:
//   can_buf1[8] = {0xAA, 0x01, Cell1_H, Cell1_L, Cell2_H, Cell2_L, Cell3_H, Cell3_L} → ID 0x0001
//   can_buf2[8] = {0xAA, 0x02, Cell4_H, Cell4_L, Cell5_H, Cell5_L, Cell6_H, Cell6_L} → ID 0x0002
//   ... 以此类推共7帧
```

### 7.4 bms_app.c — 数据采集调度

```c
// 统一的数据采集调度接口:
//   void BMS_DataAcquire(void);

// 内部调用 bq76940.c 中的函数按顺序采集:
//   1. Get_Battery1-15() (9节中有数据的)
//   2. Get_Update_ALL_Data()  → 计算总电压
//   3. Get_SOC()              → SOC 估算
//   4. Get_BQ1_2_Temp()       → 温度
//   5. Get_BQ_Current()       → 电流
//   6. BMS_STA()              → FET 状态
```

### 7.5 Flash 参数存储

```c
// STM32F103C8T6 的 Flash 特点:
//   - 页大小: 1KB
//   - 总容量: 64KB
//   - 代码区之后的页可用于参数存储
//
// 存储地址规划:
//   0x08007800 → 电池参数 (2 bytes)
//   0x08007C00 → 温度上限 (2 bytes)
//
// 注意: 写入前必须擦除整页!

void Write_Flash(uint32_t addr, uint16_t data)
{
    HAL_FLASH_Unlock();
    // 擦除页 (需要先确定 addr 所在的页)
    FLASH_EraseInitTypeDef EraseInit = {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .PageAddress = addr,
        .NbPages     = 1
    };
    uint32_t PageError;
    HAL_FLASHEx_Erase(&EraseInit, &PageError);
    // 编程
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data);
    HAL_FLASH_Lock();
}

uint16_t Read_Flash(uint32_t addr)
{
    return *(volatile uint16_t*)addr;
}
```

---

## 8. VSCode 调试配置

### 8.1 .vscode/c_cpp_properties.json

```json
{
    "configurations": [{
        "name": "STM32",
        "includePath": [
            "${workspaceFolder}/Core/Inc",
            "${workspaceFolder}/BSP/Inc",
            "${workspaceFolder}/App/Inc",
            "${workspaceFolder}/Drivers/CMSIS/Include",
            "${workspaceFolder}/Drivers/CMSIS/Device/ST/STM32F1xx/Include",
            "${workspaceFolder}/Drivers/STM32F1xx_HAL_Driver/Inc"
        ],
        "defines": [
            "STM32F103xB",
            "USE_HAL_DRIVER"
        ],
        "compilerPath": "arm-none-eabi-gcc",
        "cStandard": "c11",
        "intelliSenseMode": "gcc-arm"
    }],
    "version": 4
}
```

### 8.2 .vscode/launch.json (Cortex-Debug + OpenOCD)

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Cortex Debug (OpenOCD)",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/BMS_9S_F103.elf",
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "openocd",
            "configFiles": [
                "interface/stlink.cfg",
                "target/stm32f1x.cfg"
            ],
            "svdFile": "STM32F103.svd",
            "runToEntryPoint": "main",
            "preLaunchTask": "CMake: Build"
        },
        {
            "name": "Cortex Debug (J-Link)",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/BMS_9S_F103.elf",
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "jlink",
            "device": "STM32F103C8",
            "interface": "swd",
            "runToEntryPoint": "main",
            "preLaunchTask": "CMake: Build"
        }
    ]
}
```

### 8.3 .vscode/tasks.json

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "CMake: Configure",
            "type": "shell",
            "command": "cmake -B build -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=Debug",
            "problemMatcher": []
        },
        {
            "label": "CMake: Build",
            "type": "shell",
            "command": "cmake --build build -j",
            "group": { "kind": "build", "isDefault": true },
            "problemMatcher": "$gcc"
        },
        {
            "label": "Flash (OpenOCD)",
            "type": "shell",
            "command": "openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c \"program build/BMS_9S_F103.elf verify reset exit\"",
            "problemMatcher": []
        }
    ]
}
```

---

## 9. 编译、烧录、验证

### 9.1 编译命令

```bash
# 在项目根目录 bms_9s_f103/ 下:

# 配置
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build -j

# 或使用 make:
cd build && make -j$(nproc)
```

### 9.2 烧录命令

```bash
# 方法A: OpenOCD + ST-Link
openocd -f interface/stlink.cfg \
        -f target/stm32f1x.cfg \
        -c "program build/BMS_9S_F103.elf verify reset exit"

# 方法B: OpenOCD + J-Link
openocd -f interface/jlink.cfg \
        -f target/stm32f1x.cfg \
        -c "program build/BMS_9S_F103.elf verify reset exit"

# 方法C: STM32CubeProgrammer (GUI 工具)
STM32_Programmer_CLI -c port=SWD -w build/BMS_9S_F103.hex -v -rst
```

### 9.3 功能验证清单

完成烧录后，按以下清单逐项验证：

| 序号 | 验证项 | 预期结果 | 调试方法 |
|------|--------|---------|---------|
| 1 | LED 心跳 | LED1 以 ~1Hz 闪烁 | 观察 LED1 |
| 2 | I2C 通信 | 能读到 BQ76940 SYS_STAT 寄存器 (非 0xFF 非 0x00) | 串口打印寄存器值 |
| 3 | 电池电压 | 9节电压值在合理范围 (2500-4200mV) | 串口打印 Batteryval[0-14] |
| 4 | 总电压 | 等于9节电压之和 (偏差 < 50mV) | 万用表对比 |
| 5 | 温度 | 显示当前环境温度 (±3°C) | 温度计对比 |
| 6 | 电流 | 空载 ≈ 0mA, 带载 ≈ 实际值 | 万用表/电流探头对比 |
| 7 | SOC | 随电压变化 (如 80% 对应 ~4.0V/cell) | 串口打印 SOC 值 |
| 8 | CAN 发送 | PCAN/CAN分析仪收到7帧数据 | CAN 分析仪监听 |
| 9 | USART1 指令 | 发送 01 02 55 → 开始收到数据 | 串口助手测试 |
| 10 | HMI 显示 | 串口屏显示电压/电流/SOC | 观察串口屏 |
| 11 | 过压保护 | 模拟 >4.2V → CHG FET 关闭 | 可调电源测试 |
| 12 | 欠压保护 | 模拟 <2.8V → DSG FET 关闭 | 可调电源测试 |
| 13 | 看门狗 | 不喂狗 → 4s 后 MCU 自动复位 | 注释掉 IWDG_Feed() |
| 14 | Flash 存储 | 写入温度阈值后复位 → 读取值一致 | 通过 USART1 指令测试 |

### 9.4 调试技巧

```c
// 1. 在关键位置插入串口打印进行调试:
printf("BQ76940 init OK, GAIN=%d, offset=%d\r\n", GAIN, ADC_offset);
printf("Cell1=%dmV Cell2=%dmV ... Total=%dmV\r\n", ...);

// 2. 用 LED 指示代码执行位置:
LED2_ONOFF(1);   // 进入某函数
// ... 函数体 ...
LED2_ONOFF(0);   // 退出某函数

// 3. 检查 I2C 通信是否正常:
uint8_t sys_stat = I2C_SW_ReadByte(BQ_SYS_STAT);
printf("SYS_STAT = 0x%02X (should not be 0xFF)\r\n", sys_stat);

// 4. 检查单个电池是否正确读取:
// 用稳压电源接在某个电池通道上，对比读取值与设定值
```

---

## 10. StdPeriph → HAL 转换对照表

| 功能 | StdPeriph API | HAL API | 说明 |
|------|-------------|---------|------|
| GPIO 时钟使能 | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)` | `__HAL_RCC_GPIOA_CLK_ENABLE()` | |
| GPIO 初始化 | `GPIO_Init(GPIOA, &GPIO_InitStructure)` | `HAL_GPIO_Init(GPIOA, &GPIO_InitStruct)` | 结构体字段名略有不同 |
| GPIO 输出 | `GPIO_WriteBit(GPIOA, GPIO_Pin_0, Bit_SET)` | `HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET)` | |
| GPIO 读取 | `GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)` | `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)` | |
| JTAG 重映射 | `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` | `__HAL_AFIO_REMAP_SWJ_NOJTAG()` | 需先 `__HAL_RCC_AFIO_CLK_ENABLE()` |
| USART 初始化 | `USART_Init(USART1, &USART_InitStructure)` | `HAL_UART_Init(&huart1)` | 配置在 handle 中 |
| USART 发送字节 | `USART_SendData(USART1, data)` | `HAL_UART_Transmit(&huart1, &data, 1, timeout)` | |
| USART 接收 | `USART_ReceiveData(USART1)` | `HAL_UART_Receive(&huart1, &data, 1, timeout)` | 或使用中断模式 |
| TIM 初始化 | `TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure)` | `HAL_TIM_Base_Init(&htim2)` | |
| TIM 中断使能 | `TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE)` | `HAL_TIM_Base_Start_IT(&htim2)` | |
| CAN 初始化 | `CAN_Init(CAN1, &CAN_InitStructure)` | `HAL_CAN_Init(&hcan1)` | |
| CAN 发送 | `CAN_Transmit(CAN1, &TxMessage)` | `HAL_CAN_AddTxMessage(&hcan1, &TxHeader, data, &TxMailbox)` | |
| CAN 接收 | `CAN_Receive(CAN1, CAN_FIFO0, &RxMessage)` | `HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxHeader, data)` | |
| IWDG 初始化 | `IWDG_WriteAccessCmd()` + `IWDG_SetPrescaler()` + `IWDG_SetReload()` | `HAL_IWDG_Init(&hiwdg)` | |
| IWDG 喂狗 | `IWDG_ReloadCounter()` | `HAL_IWDG_Refresh(&hiwdg)` | |
| Flash 解锁 | `FLASH_Unlock()` | `HAL_FLASH_Unlock()` | |
| Flash 擦页 | `FLASH_ErasePage(addr)` | `HAL_FLASHEx_Erase(&EraseInit, &PageError)` | HAL 需要结构体 |
| Flash 编程 | `FLASH_ProgramWord(addr, data)` | `HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data)` | STM32F1 为半字编程 |
| Flash 上锁 | `FLASH_Lock()` | `HAL_FLASH_Lock()` | |
| 延时 ms | `delay_ms(n)` (自定义) | `HAL_Delay(n)` | 都基于 SysTick |
| 中断优先级组 | `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2)` | `HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2)` | |
| 系统复位 | `NVIC_SystemReset()` | `HAL_NVIC_SystemReset()` | 相同 |

---

## 11. 参考项目关键代码摘录

> 以下摘录自参考项目 `BMS_s940/BSP/BQ76930.c`，帮助你理解核心逻辑。
> 完整代码请查看原始文件。

### 11.1 电压读取通用模式

参考项目中所有电池电压读取遵循同一个模式。以 Cell 1 为模板:

```c
// 原始代码 (StdPeriph):
void Get_Battery1(void)
{
    unsigned int readbattbuf[2];
    unsigned int battery1val[2];
    short batteryval1;

    readbattbuf[1] = IIC1_read_one_byte(0x0c);  // VC1 High Byte
    readbattbuf[0] = IIC1_read_one_byte(0x0d);  // VC1 Low Byte

    batteryval1 = readbattbuf[1];
    batteryval1 = (batteryval1 << 8) | readbattbuf[0];
    batteryval1 = ((batteryval1 * GAIN) / 1000) + ADC_offset;

    Batteryval[0] = batteryval1;  // 存到全局数组

    // 打包到 UART 和 CAN 发送缓冲区
    battery1val[1] = (char)(batteryval1 >> 8);
    battery1val[0] = (char)(batteryval1 & 0x00FF);

    shang[2]   = battery1val[1];   // 串口帧1
    shang[3]   = battery1val[0];
    shang1[2]  = battery1val[1];   // 串口帧2 (HMI)
    shang1[3]  = battery1val[0];
    can_buf1[2] = battery1val[1];  // CAN 帧1
    can_buf1[3] = battery1val[0];
}
```

### 11.2 9节电池采集函数调用序列

```c
// 参考项目的 Get_Update_Data() 函数(第1033-1056行):
void Get_Update_Data(void)
{
    Get_Battery1();      // VC1  → Batteryval[0]
    Get_Battery2();      // VC2  → Batteryval[1]
    //Get_Battery3();    // 注释掉的 = 硬件未连接
    //Get_Battery4();    // 注释掉的 = 硬件未连接
    Get_Battery5();      // VC5  → Batteryval[4]
    Get_Battery6();      // VC6  → Batteryval[5]
    Get_Battery7();      // VC7  → Batteryval[6]
    //Get_Battery8();    // 未连接
    //Get_Battery9();    // 未连接
    Get_Battery10();     // VC10 → Batteryval[9]
    Get_Battery11();     // VC11 → Batteryval[10]
    Get_Battery12();     // VC12 → Batteryval[11]
    //Get_Battery13();   // 未连接
    //Get_Battery14();   // 未连接
    Get_Battery15();     // VC15 → Batteryval[14]

    Get_Update_ALL_Data();  // 计算总电压 → Batteryval[15]
    Get_SOC();              // SOC → Batteryval[16]
    Get_BQ1_2_Temp();       // 温度 → Batteryval[18]
    Get_BQ_Current();       // 电流 → Batteryval[17]
    BMS_STA();              // FET 状态
    Update_val();            // 发送数据到 UART/CAN/HMI
}
```

### 11.3 参考项目总电压计算

```c
// 注意: 只对9节有连接的电池求和
void Get_Update_ALL_Data(void)
{
    int Sum_val = Batteryval[0]  + Batteryval[1] +
                  Batteryval[4]  + Batteryval[5] +
                  Batteryval[6]  + Batteryval[9] +
                  Batteryval[10] + Batteryval[11] +
                  Batteryval[14];

    Batteryval[15] = Sum_val;
}
```

---

## 实施建议

### 迭代开发路径

```
第1轮 (最小原型):      确认 I2C 能跟 BQ76940 通信
  → 只写 i2c_sw.c + bq76940.c 的 Get_offset() + Get_Battery1()
  → 串口打印电压值，验证读数正确

第2轮 (数据采集):      完成所有数据读取
  → 补全9个电池、温度、电流、SOC
  → 补全数据上报 (USART1 + USART2 + CAN)

第3轮 (保护逻辑):      加入保护判断
  → protection.c
  → 测试过压/欠压/过流/过温的 FET 响应

第4轮 (打磨):          Flash 存储、看门狗、异常处理
  → 完善错误处理
  → 长时间运行稳定性测试
```

### 注意事项

1. **PA12 冲突**: 如果 PCB 上同时使用 CAN 和 P2 控制，需要硬件改版或引脚重映射
2. **I2C 写间隔**: BQ76940 要求写操作间隔 ≥10ms，**不可忽略**
3. **Flash 擦写寿命**: STM32F103 的 Flash 擦写次数约 1 万次，不要在循环中频繁擦写
4. **看门狗超时**: I2C 通信如果阻塞过久（例如 BQ76940 无响应）可能导致看门狗复位
5. **温度计算精度**: NTC 公式使用 `log()` 函数，需要链接数学库 (`-lm`)，建议在 CMakeLists.txt 中添加 `target_link_libraries(... m)`

---

> **文档版本**: v1.0
> **最后更新**: 2026-07-15
> **与参考项目的对应关系**: 本指南的引脚表、寄存器定义、保护参数均来自 `BMS_s940` 参考项目
