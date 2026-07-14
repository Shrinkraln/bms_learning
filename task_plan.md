# Task Plan: BMS 9串 开发指导框架

## Goal
基于 BQ76940 参考项目代码分析结果，创建一份完整的 **BMS 9串代码开发指导框架文档**，包含 CubeMX 配置指南、CMake 工程搭建流程、各层代码的编写规范/模板/流程、以及编译调试方法。让开发者遵循此框架即可逐步完成代码实现。

## Current Phase
Phase 4 (审查交付)

## Phases

### Phase 1: 需求分析与参考代码研究 ✅
- [x] 分析参考项目 BMS_s940 的完整代码结构
- [x] 提取引脚配置映射表（完整）
- [x] 分析 BQ76940 驱动逻辑（I2C CRC通信、寄存器读写、保护配置）
- [x] 分析保护逻辑（OV/UV/OCD/SCD/OT/UT，硬件 + 软件双重保护）
- [x] 分析数据上报格式（USART1→QT上位机, USART2→HMI, CAN→CAN网络）
- [x] 记录所有发现到 findings.md
- **Status:** complete

### Phase 2: 编写开发指导框架文档 ✅
- [x] 创建 `DEVELOPMENT_GUIDE.md` — 总览：开发环境搭建、工具链安装、项目创建步骤
- [x] 创建 `CubeMX 配置指南` 章节 — 引脚表、外设配置、时钟树、生成配置
- [x] 创建 `CMake 工程搭建指南` 章节 — CMakeLists.txt 模板、链接脚本、工具链配置
- [x] 创建 `代码架构与分层设计` 章节 — 目录结构、模块划分、文件清单、数据流图
- [x] 创建 `BSP 驱动层编写流程` 章节 — 每个驱动的 StdPeriph→HAL 转换对照、代码模板
- [x] 创建 `BQ76940 驱动编写流程` 章节 — 寄存器操作、I2C CRC协议、保护配置、FET控制
- [x] 创建 `应用层编写流程` 章节 — main.c 主循环、保护判断、数据上报、指令解析
- [x] 创建 `VSCode 调试配置` 章节 — cortex-debug、J-Link/ST-Link 配置
- [x] 创建 `编译、烧录、验证` 章节 — 构建命令、OpenOCD 烧录、功能测试清单
- **Status:** complete

### Phase 3: 补充参考模板代码（骨架）✅
- [x] 创建 CMakeLists.txt 构建模板
- [x] 创建 STM32F103C8Tx_FLASH.ld 链接脚本模板
- [x] 创建 Core/Inc/ 头文件（main.h, stm32f1xx_hal_conf.h）
- [x] 创建 BSP/Inc/ 全部9个头文件（含完整宏定义和接口声明）
- [x] 创建 .vscode/ 全部4个配置文件
- [x] 创建 scripts/setup_cmsis_hal.sh
- [x] 创建 README.md
- [x] BSP/Src 和 App/Src 目录下的 .c 文件由开发者按指南自行实现
- **Status:** complete

### Phase 4: 文档审查与交付
- [ ] 检查文档逻辑完整性
- [ ] 确保所有关键信息从 findings.md 正确传达
- [ ] 补充参考项目关键代码片段对照
- [ ] 交付指导框架
- **Status:** pending

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| 输出开发指导框架而非完整代码 | 用户指定：创建指导框架和代码编写流程 |
| 保留已创建的头文件作为参考模板 | 头文件包含引脚定义、寄存器映射，是指导框架的重要部分 |
| 生成的 .c 文件作为骨架/留空 | 展示代码结构和接口，核心逻辑由用户按指南填充 |
| 使用 HAL 库 | CubeMX 默认，API更现代化，文档丰富 |
| 保持软件 I2C | BQ76940 CRC 协议需要灵活时序控制 |

## Notes
- 指导框架文档放在 `bms_9s_f103/DEVELOPMENT_GUIDE.md`
- 已创建的项目脚手架文件（CMakeLists.txt, .ld, headers）保留作为参考
- 开发者也完全可以采用 StdPeriph 库方案 — 指南会给出两种路径的对比
