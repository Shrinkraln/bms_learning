# printf 重定向到 USART2 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `__io_putchar` 从 `huart1` 改为 `huart2`，使 printf 输出走 USART2 (PA2) 而非 USART1 (PA9)

**Architecture:** 修改 `Core/Src/main.c` 两个 USER CODE 块——USER CODE 0 中的 `__io_putchar`（重定向目标）和 USER CODE 2 中的注释（保持一致）。USART2 初始化已在 `MX_USART2_UART_Init()` 中完成，无需改动

**Tech Stack:** STM32F103 + HAL + Picolibc + GCC (arm-none-eabi) + CMake

## 全局约束

- 修改仅限于 `main.c` 中 `USER CODE BEGIN/END` 标记块内
- 保持 CubeMX 代码风格（CubeMX 重新生成不会覆盖）
- 不修改 `usart.c`、`usart.h`、`syscalls.c`、`stm32f1xx_it.c`、`usart_drv.h/c`

---

### Task 1: 修改 `__io_putchar` 使用 huart2

**文件：**
- 修改：`bms_9s_f103/Core/Src/main.c:68-72`（USER CODE 0）
- 修改：`bms_9s_f103/Core/Src/main.c:111-112`（USER CODE 2 注释）

**接口：**
- 消费：`extern UART_HandleTypeDef huart2;`（已通过 `usart.h` include 声明）
- 生产：无新增接口，`__io_putchar` 签名不变

- [ ] **Step 1: 编译验证修改前固件**

用 GCC 编译当前固件，确认基线通过。

Run: `cmake --preset Debug` then `cmake --build build/Debug`
Expected: 编译成功，0 errors。

（`cmake/stm32cubemx/CMakeLists.txt` 中预设名为 `Debug`，工具链文件由 `CMakePresets.json` 指定）

- [ ] **Step 2: 修改 USER CODE 0 — `__io_putchar`**

将 `Core/Src/main.c` 第 68-72 行替换为：

```c
/* USER CODE BEGIN 0 */
/* ---- printf 重定向到 USART2 (debug 输出) ---- */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 0 */
```

**改动**：第 68 行注释从 "重定向到 USART1" → "重定向到 USART2 (debug 输出)"；第 70 行 `&huart1` → `&huart2`。

- [ ] **Step 3: 修改 USER CODE 2 — 注释**

将 `Core/Src/main.c` 第 111-112 行替换为：

```c
  /* ---- printf 重定向到 USART2 (debug 输出) ---- */
  /* (__io_putchar 定义在 USER CODE 0 区, 使用 huart2) */
```

**改动**：仅注释文本从 "USART1" → "USART2 (debug 输出)"，增加 `huart2` 说明。第 113 行 `printf(...)` 不动。

- [ ] **Step 4: 编译验证**

Run: `cmake --build build/Debug`
Expected: 编译成功，0 errors，0 warnings（如原本无 warning）。

- [ ] **Step 5: 提交**

```bash
git add bms_9s_f103/Core/Src/main.c
git commit -m "feat: redirect printf to USART2 for debug output

- Change __io_putchar from huart1 to huart2
- Update comments in USER CODE 0 and USER CODE 2
- USART1 hardware (PA9/PA10) unchanged, reserved for flymcu download"
```

---

### Task 2: 验收 — 实机验证（可选，需硬件接入）

**前置条件**：USB-TTL 模块接入 PA2 (USART2 TX)，串口工具配置 115200/8N1

- [ ] **Step 1: 烧录固件**

用 J-Link 或 flymcu（USART1）烧录新固件。

- [ ] **Step 2: 验证 USART2 有输出**

打开串口工具（115200/8N1），连接 USART2 TX (PA2)，上电/复位。
Expected: 收到 `BMS starting...`。

- [ ] **Step 3: 验证 USART1 无 printf 输出**

打开另一个串口工具，连接 USART1 TX (PA9)。
Expected: PA9 无任何 printf 字符输出（flymcu 握手期间可能有短暂的 bootloader 信号，属正常）。

---

## 还原操作

如需回退：

```bash
git revert <commit-hash>
```

或手动将 `&huart2` 改回 `&huart1`，注释中的 "USART2" 改回 "USART1"。
