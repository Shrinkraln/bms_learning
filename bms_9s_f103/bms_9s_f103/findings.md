# 发现与决策记录

## 需求
基于 STM32F103 + FreeRTOS + BQ76940 的 9 串 BMS 固件，从底层向上开发完整驱动栈。

## 研究结果

### 项目架构
- **MCU:** STM32F103 (Cortex-M3, 72MHz)
- **RTOS:** FreeRTOS (CMSIS-RTOS V2 封装)
- **电池监视器:** BQ76940 (TI, 9-15 串电池监视器, I2C + CRC8)
- **构建系统:** CMake 3.22+ + Ninja + arm-none-eabi-gcc
- **HAL 层:** STM32CubeMX 生成的 STM32F1xx HAL Driver

### 最终项目结构
```
bms_9s_f103/
├── Core/           # 系统核心 (main, freertos, CubeMX 生成的 HAL 配置)
├── Drivers/        # CMSIS + STM32 HAL 驱动
├── Middlewares/    # FreeRTOS
├── BSP/            # 板级驱动 (已完成 8 个模块)
│   ├── Inc/        # 7 个头文件
│   └── Src/        # 7 个源文件
├── App/            # 应用层 (已完成 3 个模块)
│   ├── Inc/        # 3 个头文件
│   └── Src/        # 3 个源文件
└── cmake/          # CubeMX CMake 配置
```

### 已完成模块汇总 (10 个模块)

| 层级 | 模块 | 文件 | 功能 |
|------|------|------|------|
| BSP | systick | systick.h/c | DWT 微秒延时 + 超时计算 |
| BSP | led | led.h/c | 双 LED 控制 (PC13/PB0) |
| BSP | io_ctrl | io_ctrl.h/c | 6 路 GPIO 控制 (电源/唤醒/报警) |
| BSP | i2c_sw | i2c_sw.h/c | 软件 I2C (PB10/PB11) + CRC8 |
| BSP | usart_drv | usart_drv.h/c | 双串口 (USART1 调试 + USART2 通信) |
| BSP | bq76940 | bq76940.h/c | BQ76940 全功能驱动 (电压/温度/电流/保护/均衡) |
| BSP | can_drv | can_drv.h/c | CAN 驱动 (500kbps, 中断接收) |
| BSP | timer | timer.h/c | TIM2 定时器 (周期性/单次回调) |
| BSP | wdg | wdg.h/c | IWDG 看门狗封装 |
| App | protection | protection.h/c | 12 种故障检测 + 分级保护 |
| App | data_report | data_report.h/c | CAN + 串口数据上报 |
| App | bms_app | bms_app.h/c | 主循环 + 状态机 (100Hz) |

### 硬件引脚分配
| 引脚 | 功能 | 方向 |
|------|------|------|
| PC13 | LED1 | 推挽输出 (低亮) |
| PB0 | LED2 | 推挽输出 (低亮) |
| PA0 | BQ_POWER | 推挽输出 |
| PA1 | D_POWER | 推挽输出 |
| PA8 | WAKE_BQ | 推挽输出 |
| PB1 | STA_WAKE | 推挽输出 |
| PB2 | BQ1_ALERT | 推挽输出 |
| PB12 | IN_STA | 推挽输出 |
| PB8 | I2C1_SCL (HW) | AF 开漏 |
| PB9 | I2C1_SDA (HW) | AF 开漏 |
| PB10 | I2C_SW_SCL | 开漏输出 |
| PB11 | I2C_SW_SDA | 开漏输出 |
| PA9 | USART1_TX | AF 推挽 |
| PA10 | USART1_RX | 浮空输入 |
| PA2 | USART2_TX | AF 推挽 |
| PA3 | USART2_RX | 浮空输入 |
| PA11 | CAN_RX | 浮空输入 |
| PA12 | CAN_TX | AF 推挽 |

### 时钟和外设
| 外设 | 配置 | 用途 |
|------|------|------|
| SYSCLK | 72MHz (HSE+PLL) | 系统时钟 |
| APB1 | 36MHz | I2C1, USART2, TIM2 |
| APB2 | 72MHz | USART1, TIM1 |
| TIM1 | 1MHz→1kHz | HAL 时基 (1ms) |
| TIM2 | 1kHz | BSP 定时器 |
| I2C1 | 100kHz | 硬件 I2C (备用) |
| CAN1 | 500kbps | BMS 通信 |
| USART1 | 115200-8N1 | 调试控制台 |
| USART2 | 9600-8N1 | 外部通信 |
| IWDG | LSI 40kHz | 独立看门狗 |

### 编译结果
- **RAM:** 9312 B / 20 KB (45.47%)
- **FLASH:** 26728 B / 64 KB (40.78%)
- **编译器:** arm-none-eabi-gcc (C11, -O0 -g3 Debug)
- **警告/错误:** 0 / 0

## 技术决策
| 决策 | 理由 |
|------|------|
| 软件 I2C 不用 HAL GPIO 模式切换 | HAL_GPIO_Init() 太慢，改用直接寄存器操作 BSRR/BRR |
| CRC8 内置在 i2c_sw 中 | BQ76940 每个寄存器访问都需要 CRC8 |
| LED 采用枚举索引 API | 上层代码不需要知道引脚号，语义化使用 |
| CAN 使用 FIFO0 中断 + 环形缓冲区 | 解耦 ISR 和主循环，避免丢帧 |
| 保护采用故障确认计数器 | 防止瞬态干扰误触发（去抖） |
| TIM2 回调通过 main.c 分发 | 避免 HAL weak 函数冲突（TIM1 已被占用） |
| 使用 -ffunction-sections + --gc-sections | 未调用的函数不占用 FLASH |

## 遇到的问题
| 问题 | 解决方案 |
|------|---------|
| i2c_sw 中 sda_out/sda_in 使用 HAL_GPIO_Init 太慢 | 改用开漏模式的技巧：一直保持开漏输出，读前先写 ODR=1 释放总线 |
| 环形缓冲区使用固定大小数组成员 | 改为指针指向外部数组，支持不同 USART 不同大小 |
| main.c 已占用 HAL_TIM_PeriodElapsedCallback | timer.c 导出 bsp_timer_irq_handler()，在 main.c 中调用 |
| __HAL_IWDG_GET_COUNTER 在 F1 HAL 中不存在 | 直接读取 IWDG->RLR 寄存器 |
| __HAL_RCC_CLEAR_FLAG 在 F1 HAL 中不存在 | 使用 SET_BIT(RCC->CSR, RCC_CSR_RMVF) |
| App 代码被 --gc-sections 优化掉 | 正常行为 — 在 main.c 调用 bms_app_init() 后自动链接 |

## 资源
- 项目根目录: `D:\coding_codes\bms_learning\bms_9s_f103\bms_9s_f103`
- CMakeLists.txt: 已更新，包含所有 10 个模块
- BSP 头文件: `BSP/Inc/` (7 个)
- BSP 源文件: `BSP/Src/` (7 个)
- App 头文件: `App/Inc/` (3 个)
- App 源文件: `App/Src/` (3 个)

---

*每 2 次查看/浏览器/搜索操作后更新此文件*
