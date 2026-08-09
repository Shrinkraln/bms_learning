---
spec_dev:
  version: 1
  feature: usart2-debug-printf
  status: active
  covers:
    - "bms_9s_f103/Core/Src/main.c"
  sync_commit: null
---

# printf 重定向到 USART2 (debug 输出)

## 背景与目标

当前 `__io_putchar()` 将 printf 输出重定向到 USART1（PA9/PA10），USART1 同时承担 flymcu 串口下载功能。为将 debug 输出与下载通道分离，改用 USART2（PA2/PA3）作为 printf 输出通道。

**成功标准**：
1. `printf()` 输出从 USART2 (PA2) 发出，而非 USART1 (PA9)
2. USART1 初始化与外设配置不受影响
3. 代码实现保持 CubeMX 生成风格（修改仅限于 USER CODE 块内）

## 非目标

- 不修改 USART2 的 CubeMX 初始化（`MX_USART2_UART_Init()` 已在 `usart.c` 中由 CubeMX 生成，在 `main()` 中已调用）
- 不修改 `usart.c`、`usart.h`、`syscalls.c`、`stm32f1xx_it.c` 等 CubeMX 生成文件
- 不扩展 BSP 层 `usart_drv`（其 `usart_id_t` 枚举、`usart_get_handle()` 等维持现状）
- 不影响 USART1 的硬件功能（flymcu 下载使用 bootloader，在固件运行前完成）

## 术语表

- **USART1 (PA9/PA10)**：串口下载通道，flymcu 通过该串口烧录固件
- **USART2 (PA2/PA3)**：debug 输出通道，printf 重定向目标
- **`__io_putchar`**：Picolibc/newlib 的字符输出钩子，由 `_write()` (syscalls.c) 逐字节调用

## 影响面

| 模块 | 文件 | 改动性质 |
|------|------|---------|
| 固件-main | `Core/Src/main.c` | USER CODE 0：修改 `__io_putchar` handle + 注释；USER CODE 2：更新注释 |

## 已确认的关键决策

- **修改 `__io_putchar` 使用 `huart2` 而非新增独立 debug 通道**：改动最小（1 行代码 + 注释），全项目仅 1 处 `printf` 调用，统一到 USART2 避免双通道维护成本。用户已确认"仅 USART2"策略
- **不改 BSP usart_drv 层**：`usart_drv` 定位为 USART1 中断接收调试控制台（其 `usart_drv_init()` 当前无调用），printf 直接走 `HAL_UART_Transmit` 不经过 BSP 层，无需扩展 `usart_id_t` 枚举

---

## MODIFIED Requirements

### Requirement: printf 输出至 USART2

固件 SHALL 将 `printf()` 的标准输出通过 USART2 (PA2) 发送，而非 USART1 (PA9)。（原行为：`__io_putchar` 使用 `&huart1`，printf 输出至 USART1）

#### Scenario: 启动时 printf 从 USART2 输出

- **GIVEN** `MX_USART2_UART_Init()` 已完成（`main()` 中先于 printf 调用）
- **WHEN** 固件执行 `printf("BMS starting...\r\n")` 及其他后续 printf 调用
- **THEN** 字符数据从 USART2 TX (PA2) 发出，USART1 TX (PA9) 无输出

#### Scenario: printf 使用阻塞发送

- **GIVEN** USART2 已初始化
- **WHEN** 任意代码调用 `printf()`
- **THEN** `__io_putchar` 通过 `HAL_UART_Transmit(&huart2, ..., HAL_MAX_DELAY)` 逐字节阻塞发送（行为与原先 USART1 一致）

---

## REMOVED Requirements

- **printf 输出至 USART1**：`__io_putchar` 不再绑定 `huart1`，移除该行为。USART1 硬件（时钟、引脚、NVIC）仍然初始化，仅 printf 不再使用

---

## 方案设计

### 改动点

**文件**：`Core/Src/main.c`，两处 USER CODE 块：

**USER CODE 0（线段 67-73）**——修改 `__io_putchar` 的目标 handle：

```c
/* USER CODE BEGIN 0 */
/* ---- printf 重定向到 USART2 (debug 输出) ---- */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 0 */
```

**USER CODE 2（线段 110-114）**——更新注释以保持一致性：

```c
/* USER CODE BEGIN 2 */
/* ---- printf 重定向到 USART2 (debug 输出) ---- */
/* (__io_putchar 定义在 USER CODE 0 区, 使用 huart2) */
printf("BMS starting...\r\n");
/* USER CODE END 2 */
```

### 不动内容

- `usart.c`：`MX_USART1_UART_Init()` + `MX_USART2_UART_Init()` 保持不变
- `usart.h`：`extern huart1` + `extern huart2` 声明不变
- `syscalls.c`：`_write()` → `__io_putchar()` 链路不变
- `stm32f1xx_it.c`：USART1/USART2 IRQ handler 不变
- `usart_drv.h/c`：BSP 层维持 USART1 环缓冲架构

### 错误处理

- 与原先 USART1 行为一致：`HAL_MAX_DELAY` 无限等待，USART2 对端未接时调用线程卡死。debug 输出场景可接受。不新增风险

## 测试与验收策略

| Scenario / 检查项 | 维度 | 执行方式 | 验收证据 |
|-------------------|------|---------|---------|
| 启动时 printf 从 USART2 输出 | integration | 验收任务 | 串口工具在 PA2 上收到 "BMS starting..." |
| USART1 无 printf 输出 | integration | 验收任务 | 串口工具在 PA9 上无数据 |
| 固件编译通过 | unit | 任务内 | GCC/CMake 编译成功 |

## 风险与边缘情况

- **USART2 未连接时 printf 卡死**：与原来 USART1 行为一致，`HAL_MAX_DELAY` 无限等待。仅在 debug 场景下使用，非生产路径
- **FreeRTOS 多任务 printf 无互斥**：与原来行为一致，不引入新问题。如后续需要线程安全，应在独立 spec 中处理
- **CubeMX 重新生成代码**：修改仅在 USER CODE 块内，重新生成不会被覆盖

## 开放问题

- 无
