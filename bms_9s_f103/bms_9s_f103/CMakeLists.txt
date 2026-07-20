# 语法：cmake_minimum_required(VERSION <version>)
# 该指令必须放在 CMakeLists.txt 的最顶部
# 示例：要求 CMake 版本不低于 3.22
cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
#使用c11标准
set(CMAKE_C_STANDARD 11)
#强制支持
set(CMAKE_C_STANDARD_REQUIRED ON)
#允许编译器扩展
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
#定义项目名变量
set(CMAKE_PROJECT_NAME bms_9s_f103)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
#定义项目名
project(${CMAKE_PROJECT_NAME})
#输出
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
#定义语言参数
enable_language(C ASM)

# Create an executable object type
#添加可执行文件
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
# 添加子目录
add_subdirectory(cmake/stm32cubemx)

# ============================================================
# BSP 层 — 板级驱动
# 采用 CubeMX OBJECT 库模式，与 STM32_Drivers / FreeRTOS 风格一致
# 写完一个 .c 就取消对应注释，编译验证
# ============================================================

# BSP 头文件路径
set(BSP_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/Inc
)

# BSP 源文件 (按开发流程 Step8-14 逐步添加，添加后注释掉占位文件)
set(BSP_Src
    # ---- Step 8:  基础模块 ----
    BSP/Src/systick.c
    BSP/Src/led.c

    # ---- Step 9:  GPIO 控制 ----
    BSP/Src/io_ctrl.c

    # ---- Step 10: 软件 I2C + CRC8 ----
    BSP/Src/i2c_sw.c

    # ---- Step 11: 串口驱动 (USART1 + USART2) ----
    BSP/Src/usart_drv.c

    # ---- Step 12: BQ76940 驱动核心 ----
    BSP/Src/bq76940.c

    # ---- Step 13: CAN 驱动 ----
    BSP/Src/can_drv.c

    # ---- Step 14: 定时器 + 看门狗 ----
    BSP/Src/timer.c
    BSP/Src/wdg.c
)

add_library(BSP OBJECT)
target_sources(BSP PRIVATE ${BSP_Src})
target_include_directories(BSP PUBLIC ${BSP_Include_Dirs})
target_link_libraries(BSP PUBLIC stm32cubemx)

# ============================================================
# App 层 — 应用逻辑 (保护判断, 数据上报, 采集调度)
# ============================================================

# App 头文件路径
set(App_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
)

# App 源文件 (按开发流程 Step15，添加后注释掉占位文件)
set(App_Src
    # ---- Step 15: 应用层 ----
    App/Src/protection.c
    App/Src/data_report.c
    App/Src/bms_app.c
)

add_library(App OBJECT)
target_sources(App PRIVATE ${App_Src})
target_include_directories(App PUBLIC ${App_Include_Dirs})
target_link_libraries(App PUBLIC stm32cubemx BSP)

# ============================================================
# 可执行文件配置
# ============================================================

# Link directories setup
#链接目录
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
#源文件
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src
)

# Add include paths
#为目标指定头文件路径
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Inc

)

# Add project symbols (macros)
#宏定义
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
#移除错误的库依赖
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
# 为目标链接库
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    BSP
    App

    # Add user defined libraries
)

# ============================================================
# 编译后处理 — 生成 .hex / .bin / 输出内存使用报告
# (工具链前缀由 CMakePresets.json 中的 toolchainFile 设置)
# ============================================================
if(NOT CMAKE_OBJCOPY)
    set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
endif()
if(NOT CMAKE_SIZE)
    set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)
endif()

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE}
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "生成 .hex .bin 和内存报告..."
)
