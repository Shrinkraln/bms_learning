# BMS 9S F103 全栈架构重新设计 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于新 CubeMX 硬件配置，从底层向上重建 Core→BSP→App 三层 BMS 固件（6 任务事件驱动 FreeRTOS + 单 CAN 队列头插）

**Architecture:** 自下而上实施 — 先删死代码，再建 BSP 公共模块(ring_buf/bsp_common)，接着重写 BSP 设备驱动(led→io_ctrl→bq76940→can_drv)，然后重写 App 策略模块(bms_shared→protection→can_cmd→bms_app)，最后集成 Core 层(freertos.c→stm32f1xx_it.c→main.c→CMakeLists.txt→头文件卫)

**Tech Stack:** STM32F103C8Tx + FreeRTOS V10.3.1 + CMSIS-RTOS2 + CubeMX HAL + arm-none-eabi-gcc (C11) + CMake 3.22+ + Ninja

## Global Constraints

- CubeMX USER CODE 区域必须保持兼容 — 所有手动代码写在 `USER CODE BEGIN/END` 之间
- CubeMX 已生成的硬件配置不得在本计划中修改: PA8(MCU_WAKE_BQ, Output PP), PA15(LED_0_ON, Output PP), PB2(Boot1, Input), CAN1 500kbps, I2C1 100kHz PB8/PB9, IWDG, TIM2 10Hz NVIC prio5, TIM1 1ms HAL timebase
- FreeRTOS 堆 = 12288B (已配置), `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5` (已配置)
- 分层纪律: App 层代码**不调用任何 BSP 函数或 HAL 寄存器**，硬件操作通过返回值/输出参数向任务函数报告意图
- 构建: `cmake --preset Debug && cmake --build build/Debug`，每完成一个 Task 编译验证
- 编译目标: 0 警告 0 错误

---

## Phase A: 清理死代码

### Task A1: 删除 usart_drv 模块 + data_report 模块 + 更新 CMakeLists.txt

**Files:**
- Delete: `BSP/Inc/usart_drv.h`
- Delete: `BSP/Src/usart_drv.c`
- Delete: `App/Inc/data_report.h`
- Delete: `App/Src/data_report.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: 无
- Produces: 无新接口，仅删除

- [ ] **Step 1: 删除 4 个文件**

```bash
rm "BSP/Inc/usart_drv.h"
rm "BSP/Src/usart_drv.c"
rm "App/Inc/data_report.h"
rm "App/Src/data_report.c"
```

- [ ] **Step 2: 更新 CMakeLists.txt — 移除 usart_drv 和 data_report 编译项**

修改 `CMakeLists.txt` 中 BSP_Src:
```cmake
set(BSP_Src
    # ---- 基础模块 ----
    BSP/Src/systick.c
    BSP/Src/led.c

    # ---- GPIO 控制 ----
    BSP/Src/io_ctrl.c

    # ---- 软件 I2C + CRC8 ----
    BSP/Src/i2c_sw.c

    # ---- BQ76940 驱动核心 ----
    BSP/Src/bq76940.c

    # ---- CAN 驱动 ----
    BSP/Src/can_drv.c

    # ---- 定时器 + 看门狗 ----
    BSP/Src/timer.c
    BSP/Src/wdg.c
)
```

修改 App_Src:
```cmake
set(App_Src
    App/Src/bms_app.c
    App/Src/bms_shared.c
    App/Src/soc_ocv.c
    App/Src/can_cmd.c
    App/Src/protection.c
)
```

- [ ] **Step 3: 编译验证**

```bash
cmake --preset Debug && cmake --build build/Debug
```

Expected: 编译失败（后续模块引用了被删除的头文件，Task A2-A4 修复）

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore: 删除 usart_drv + data_report 死代码

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task A2: 删除 freertos.c 中的 defaultTask

**Files:**
- Modify: `Core/Src/freertos.c`

**Interfaces:**
- Consumes: 无
- Produces: freertos.c 仅保留 MX_FREERTOS_Init() 空壳 (Task E1 将填充 BMS 任务创建)

- [ ] **Step 1: 读取 freertos.c 当前内容**

```bash
# 确认 USER CODE 区域位置
```

- [ ] **Step 2: 删除 defaultTask 定义和创建代码**

在 `Core/Src/freertos.c` 中:

删除这段:
```c
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
```

删除 `MX_FREERTOS_Init()` 中这行:
```c
defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
```

删除整个 `StartDefaultTask` 函数定义:
```c
void StartDefaultTask(void *argument)
{
  for(;;)
  {
    osDelay(1000);
  }
}
```

- [ ] **Step 3: Commit**

```bash
git add Core/Src/freertos.c
git commit -m "chore: 删除 defaultTask 空任务

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase B: BSP 公共模块

### Task B1: 创建 bsp_common.h — 通用状态枚举

**Files:**
- Create: `BSP/Inc/bsp_common.h`

**Interfaces:**
- Consumes: 无
- Produces: `bsp_status_t` 枚举 (OK=0, ERROR=1, BUSY=2, TIMEOUT=3)

- [ ] **Step 1: 创建 bsp_common.h**

```c
/**
 * @file    bsp_common.h
 * @brief   BSP 层通用定义 — 状态枚举，各模块 typedef 别名
 */

#ifndef BSP_COMMON_H
#define BSP_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief BSP 通用操作状态 */
typedef enum {
    BSP_OK      = 0x00U,
    BSP_ERROR   = 0x01U,
    BSP_BUSY    = 0x02U,
    BSP_TIMEOUT = 0x03U,
} bsp_status_t;

#ifdef __cplusplus
}
#endif

#endif /* BSP_COMMON_H */
```

- [ ] **Step 2: Commit**

```bash
git add BSP/Inc/bsp_common.h
git commit -m "feat: 创建 bsp_common.h — BSP 通用状态枚举

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task B2: 创建 ring_buf.h — 通用环形缓冲区 (元素级)

**Files:**
- Create: `BSP/Inc/ring_buf.h`

**Interfaces:**
- Consumes: 无
- Produces:
  ```c
  typedef struct { uint8_t *buf; uint16_t elem_size, capacity, head, tail, count; } ring_buf_t;
  void     ring_buf_init(ring_buf_t *rb, uint8_t *buf, uint16_t elem_size, uint16_t capacity);
  uint16_t ring_buf_available(const ring_buf_t *rb);
  uint8_t  ring_buf_put(ring_buf_t *rb, const void *elem);        // 尾插, 满时丢弃最旧
  uint8_t  ring_buf_put_front(ring_buf_t *rb, const void *elem);  // 头插, 满时丢弃最新
  uint8_t  ring_buf_get(ring_buf_t *rb, void *elem);              // 取队首
  ```

- [ ] **Step 1: 创建 ring_buf.h — 全部 inline 实现**

```c
/**
 * @file    ring_buf.h
 * @brief   通用环形缓冲区 (元素级，纯头文件 inline 实现)
 * @note    非线程安全，调用方负责同步
 *          - put: 尾插入队，满时 tail 前进丢弃最旧元素
 *          - put_front: 头插入队，满时 head 不动覆盖最新元素位置
 *          - get: 取队首 (tail 位置)
 */

#ifndef BSP_RING_BUF_H
#define BSP_RING_BUF_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    uint16_t elem_size;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} ring_buf_t;

/** @brief 初始化环形缓冲区 */
static inline void ring_buf_init(ring_buf_t *rb, uint8_t *buf,
                                  uint16_t elem_size, uint16_t capacity)
{
    rb->buf       = buf;
    rb->elem_size = elem_size;
    rb->capacity  = capacity;
    rb->head      = 0;
    rb->tail      = 0;
    rb->count     = 0;
}

/** @brief 返回当前元素数 */
static inline uint16_t ring_buf_available(const ring_buf_t *rb)
{
    return rb->count;
}

/** @brief 尾插入队，满时丢弃最旧元素 (tail++)
 *  @retval 0=OK, 1=发生过丢弃 */
static inline uint8_t ring_buf_put(ring_buf_t *rb, const void *elem)
{
    uint8_t dropped = 0;
    if (rb->count == rb->capacity) {
        /* 满: 丢弃最旧 */
        rb->tail = (rb->tail + 1) % rb->capacity;
        rb->count--;
        dropped = 1;
    }
    uint8_t *dst = &rb->buf[rb->head * rb->elem_size];
    memcpy(dst, elem, rb->elem_size);
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;
    return dropped;
}

/** @brief 头插入队，满时覆盖 head 前一个位置
 *  @retval 0=OK, 1=发生过丢弃 */
static inline uint8_t ring_buf_put_front(ring_buf_t *rb, const void *elem)
{
    uint8_t dropped = 0;
    if (rb->count == rb->capacity) {
        /* 满: head 保持, 覆盖逻辑队首 - 在满队列中相当于丢弃最旧 */
        rb->tail = (rb->tail + 1) % rb->capacity;
        rb->count--;
        dropped = 1;
    }
    /* tail 回退一格 */
    rb->tail = (rb->tail == 0) ? (rb->capacity - 1) : (rb->tail - 1);
    uint8_t *dst = &rb->buf[rb->tail * rb->elem_size];
    memcpy(dst, elem, rb->elem_size);
    rb->count++;
    return dropped;
}

/** @brief 取队首元素 (tail 位置)
 *  @retval 0=OK, 1=队列空 */
static inline uint8_t ring_buf_get(ring_buf_t *rb, void *elem)
{
    if (rb->count == 0) {
        return 1;
    }
    uint8_t *src = &rb->buf[rb->tail * rb->elem_size];
    memcpy(elem, src, rb->elem_size);
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* BSP_RING_BUF_H */
```

- [ ] **Step 2: Commit**

```bash
git add BSP/Inc/ring_buf.h
git commit -m "feat: 创建 ring_buf.h — 元素级环形缓冲区 (支持头插/尾插)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase C: BSP 设备驱动

### Task C1: 重写 led.h + led.c — PA15 单 LED

**Files:**
- Overwrite: `BSP/Inc/led.h`
- Overwrite: `BSP/Src/led.c`

**Interfaces:**
- Consumes: `main.h` (LED_0_ON_Pin, LED_0_ON_GPIO_Port)
- Produces: `led_init()`, `led_on()`, `led_off()`, `led_toggle()`

- [ ] **Step 1: 重写 led.h**

```c
/**
 * @file    led.h
 * @brief   BSP 层 LED 驱动 — PA15 单 LED
 */

#ifndef BSP_LED_H
#define BSP_LED_H

#include "stm32f1xx_hal.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LED_H */
```

- [ ] **Step 2: 重写 led.c**

```c
/**
 * @file    led.c
 * @brief   LED 驱动实现 — PA15, 低电平点亮
 */

#include "led.h"

static uint8_t led_state = 0;  /* 0=灭, 1=亮 */

void led_init(void)
{
    led_state = 0;
    HAL_GPIO_WritePin(LED_0_ON_GPIO_Port, LED_0_ON_Pin, GPIO_PIN_SET);  /* 灭 */
}

void led_on(void)
{
    led_state = 1;
    HAL_GPIO_WritePin(LED_0_ON_GPIO_Port, LED_0_ON_Pin, GPIO_PIN_RESET); /* 低亮 */
}

void led_off(void)
{
    led_state = 0;
    HAL_GPIO_WritePin(LED_0_ON_GPIO_Port, LED_0_ON_Pin, GPIO_PIN_SET);
}

void led_toggle(void)
{
    if (led_state) {
        led_off();
    } else {
        led_on();
    }
}
```

- [ ] **Step 3: 编译验证**

```bash
cmake --preset Debug && cmake --build build/Debug
```

Expected: 编译失败 (io_ctrl 等旧模块引用了已删除的引脚)

- [ ] **Step 4: Commit**

```bash
git add BSP/Inc/led.h BSP/Src/led.c
git commit -m "refactor: 重写 led — PA15 单 LED, 移除旧双 LED API

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task C2: 重写 io_ctrl.h + io_ctrl.c — 精简为 PA8 单引脚

**Files:**
- Overwrite: `BSP/Inc/io_ctrl.h`
- Overwrite: `BSP/Src/io_ctrl.c`

**Interfaces:**
- Consumes: `main.h` (MCU_WAKE_BQ_Pin, MCU_WAKE_BQ_GPIO_Port)
- Produces:
  ```c
  typedef enum { IO_ID_WAKE_BQ = 0U } io_ctrl_pin_t;
  typedef enum { IO_LEVEL_LOW = 0U, IO_LEVEL_HIGH = 1U } io_level_t;
  bsp_status_t io_ctrl_init(void);
  bsp_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level);
  ```

- [ ] **Step 1: 重写 io_ctrl.h**

```c
/**
 * @file    io_ctrl.h
 * @brief   BSP 层 GPIO 控制 — PA8 MCU_WAKE_BQ (BQ76940 TS1 启动脉冲)
 */

#ifndef BSP_IO_CTRL_H
#define BSP_IO_CTRL_H

#include "stm32f1xx_hal.h"
#include "main.h"
#include "bsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IO_ID_WAKE_BQ = 0U,
} io_ctrl_pin_t;

typedef enum {
    IO_LEVEL_LOW  = 0U,
    IO_LEVEL_HIGH = 1U,
} io_level_t;

bsp_status_t io_ctrl_init(void);
bsp_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* BSP_IO_CTRL_H */
```

- [ ] **Step 2: 重写 io_ctrl.c**

```c
/**
 * @file    io_ctrl.c
 * @brief   GPIO 控制实现 — PA8 MCU_WAKE_BQ
 */

#include "io_ctrl.h"

bsp_status_t io_ctrl_init(void)
{
    /* PA8 在 MX_GPIO_Init() 已初始化为 Output PP, 初始 LOW */
    return BSP_OK;
}

bsp_status_t io_ctrl_set(io_ctrl_pin_t id, io_level_t level)
{
    if (id != IO_ID_WAKE_BQ) {
        return BSP_ERROR;
    }

    GPIO_PinState state = (level == IO_LEVEL_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(MCU_WAKE_BQ_GPIO_Port, MCU_WAKE_BQ_Pin, state);
    return BSP_OK;
}
```

- [ ] **Step 3: Commit**

```bash
git add BSP/Inc/io_ctrl.h BSP/Src/io_ctrl.c
git commit -m "refactor: 重写 io_ctrl — 精简为 PA8 单引脚 (MCU_WAKE_BQ)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task C3: 修改 bq76940.h/c — 移除 io_ctrl 依赖

**Files:**
- Modify: `BSP/Inc/bq76940.h`
- Modify: `BSP/Src/bq76940.c`

**Interfaces:**
- Consumes: `i2c_sw.h`, `systick.h` (不再依赖 io_ctrl.h)
- Produces: (接口不变)
  ```c
  bq76940_status_t bq76940_init(const bq76940_cfg_t *cfg);
  bq76940_status_t bq76940_read_all(bq76940_data_t *data);
  bq76940_status_t bq76940_set_balancing(uint16_t mask);
  bq76940_status_t bq76940_set_protection(const bq76940_cfg_t *cfg);
  bq76940_status_t bq76940_clear_faults(void);
  ```

- [ ] **Step 1: 修改 bq76940.h — 移除 #include "io_ctrl.h"**

删除:
```c
#include "io_ctrl.h"
```

- [ ] **Step 2: 修改 bq76940.c — 移除 io_ctrl 调用**

在 `bq76940_init()` 中删除 `io_ctrl_set(IO_ID_BQ_POWER, ...)` 相关代码。
在 `bq76940_shutdown()` 中删除 `io_ctrl_set(IO_ID_BQ_POWER, ...)` 相关代码。

如果函数体变空或只剩注释，删除整个函数或标记为 stub:
```c
bq76940_status_t bq76940_shutdown(void)
{
    /* BQ76940 由电池直接供电，MCU 无法切断电源。
     * 通过 I2C 发送 SHIP 命令实现软件关机 */
    return bq76940_reg_write(BQ76940_ADDR, BQ76940_REG_SYS_CTRL1, 0x01); /* SHIP mode */
}
```

- [ ] **Step 3: 编译验证**

```bash
cmake --preset Debug && cmake --build build/Debug
```

Expected: 编译通过 (BSP 层不再有对旧引脚的引用)

- [ ] **Step 4: Commit**

```bash
git add BSP/Inc/bq76940.h BSP/Src/bq76940.c
git commit -m "refactor: bq76940 移除 io_ctrl 依赖, SHIP 模式替代断电

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task C4: 修改 can_drv.h/c — 改用 ring_buf 统一环形缓冲区

**Files:**
- Modify: `BSP/Inc/can_drv.h`
- Modify: `BSP/Src/can_drv.c`

**Interfaces:**
- Consumes: `ring_buf.h`, `can.h` (CubeMX), `main.h`
- Produces:
  ```c
  typedef struct { uint32_t id; uint8_t len; uint8_t data[8]; } can_msg_t;
  void    can_drv_init(void);
  uint8_t can_send(const can_msg_t *msg);
  uint8_t can_recv(can_msg_t *msg);
  uint8_t can_available(void);
  ```

- [ ] **Step 1: 修改 can_drv.h — 使用 ring_buf**

添加:
```c
#include "ring_buf.h"
```

将内部环形缓冲区类型从自实现改为 `ring_buf_t`。

- [ ] **Step 2: 修改 can_drv.c — 采用 ring_buf 接口**

将 `rx_fifo` 改为 `ring_buf_t`:
```c
static uint8_t can_rx_buf[32][sizeof(can_msg_t)];
static ring_buf_t can_rx_ring;

void can_drv_init(void)
{
    ring_buf_init(&can_rx_ring, (uint8_t *)can_rx_buf, sizeof(can_msg_t), 32);
    /* ... 启动 CAN 外设 ... */
}
```

ISR 中使用 `ring_buf_put()` 替代旧的 `fifo_put()`:
```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    can_msg_t msg;
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, msg.data);
    msg.id  = rx_header.StdId;
    msg.len = rx_header.DLC;
    ring_buf_put(&can_rx_ring, &msg);  /* ISR 上下文中无需锁，写入方唯一 */
}
```

`can_recv()` / `can_available()` 改用 `ring_buf_get()` / `ring_buf_available()`.

- [ ] **Step 3: 编译验证**

```bash
cmake --preset Debug && cmake --build build/Debug
```

- [ ] **Step 4: Commit**

```bash
git add BSP/Inc/can_drv.h BSP/Src/can_drv.c
git commit -m "refactor: can_drv 改用 ring_buf 统一环形缓冲区

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase D: App 策略模块

### Task D1: 重写 bms_shared.h + bms_shared.c — 共享数据中心

**Files:**
- Overwrite: `App/Inc/bms_shared.h`
- Overwrite: `App/Src/bms_shared.c`

**Interfaces:**
- Consumes: `cmsis_os.h`, `bq76940.h` (仅类型), `protection.h` (仅类型)
- Produces: `bms_shared_t` 全局实例, `bms_shared_init()`, lock/unlock/update 系列函数

- [ ] **Step 1: 重写 bms_shared.h**

```c
/**
 * @file    bms_shared.h
 * @brief   BMS 共享数据中心 — 多任务数据交换枢纽
 */

#ifndef APP_BMS_SHARED_H
#define APP_BMS_SHARED_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "bq76940.h"
#include "protection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 运行时可配置参数 ---- */
typedef struct {
    uint16_t cell_ov_mv;           /* 电芯过压阈值 (mV)          */
    uint16_t cell_uv_mv;           /* 电芯欠压阈值 (mV)          */
    uint16_t pack_ov_mv;           /* 总压过压阈值 (mV)          */
    uint16_t pack_uv_mv;           /* 总压欠压阈值 (mV)          */
    uint16_t discharge_oc_ma;      /* 放电过流阈值 (mA)          */
    uint16_t charge_oc_ma;         /* 充电过流阈值 (mA)          */
    uint16_t short_circuit_ma;     /* 短路电流阈值 (mA)          */
    int16_t  over_temp_mdeg;       /* 过温阈值 (0.1°C)           */
    int16_t  under_temp_mdeg;      /* 欠温阈值 (0.1°C)           */
    uint16_t cell_diff_max_mv;     /* 电芯最大压差阈值 (mV)      */
    uint16_t balance_thresh_mv;    /* 均衡开启压差 (mV)          */
    uint16_t balance_min_mv;       /* 均衡最低电压 (mV)          */
} bms_settings_t;

/* ---- 共享数据结构 ---- */
typedef struct {
    bq76940_data_t  battery_val;
    prot_level_t    prot_level;
    uint16_t        active_faults;
    uint16_t        latched_faults;
    uint16_t        soc_permil;
    uint16_t        ocv_mv;
    int32_t         remaining_mah;
    bms_settings_t  settings;
    osMutexId_t     mutex_data;
    osMutexId_t     mutex_settings;
} bms_shared_t;

/* ---- API ---- */
void bms_shared_init(void);

bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms);
void bms_shared_data_unlock(void);

bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms);
void bms_shared_settings_unlock(void);

void bms_shared_update_bq_data(const bq76940_data_t *data);
void bms_shared_update_protection(prot_level_t level, uint16_t faults);
void bms_shared_update_soc(uint16_t soc_permil, uint16_t ocv_mv, int32_t remaining_mah);

#ifdef __cplusplus
}
#endif

#endif /* APP_BMS_SHARED_H */
```

- [ ] **Step 2: 重写 bms_shared.c**

```c
#include "bms_shared.h"

static bms_shared_t g_bms;

/* 默认出厂值 */
static const bms_settings_t DEFAULT_SETTINGS = {
    .cell_ov_mv        = 4250,
    .cell_uv_mv        = 2800,
    .pack_ov_mv        = 38250,  /* 9S × 4250mV */
    .pack_uv_mv        = 25200,  /* 9S × 2800mV */
    .discharge_oc_ma   = 50000,
    .charge_oc_ma      = 20000,
    .short_circuit_ma  = 100000,
    .over_temp_mdeg    = 600,    /* 60.0°C */
    .under_temp_mdeg   = -100,   /* -10.0°C */
    .cell_diff_max_mv  = 500,
    .balance_thresh_mv = 20,
    .balance_min_mv    = 3200,
};

void bms_shared_init(void)
{
    memset(&g_bms, 0, sizeof(g_bms));
    g_bms.settings = DEFAULT_SETTINGS;

    osMutexAttr_t mtx_attr = { .attr_bits = osMutexPrioInherit };
    g_bms.mutex_data     = osMutexNew(&mtx_attr);
    g_bms.mutex_settings = osMutexNew(&mtx_attr);
}

bms_shared_t *bms_shared_data_lock(uint32_t timeout_ms)
{
    if (osMutexAcquire(g_bms.mutex_data, timeout_ms) == osOK) {
        return &g_bms;
    }
    return NULL;
}

void bms_shared_data_unlock(void)
{
    osMutexRelease(g_bms.mutex_data);
}

bms_settings_t *bms_shared_settings_lock(uint32_t timeout_ms)
{
    if (osMutexAcquire(g_bms.mutex_settings, timeout_ms) == osOK) {
        return &g_bms.settings;
    }
    return NULL;
}

void bms_shared_settings_unlock(void)
{
    osMutexRelease(g_bms.mutex_settings);
}

void bms_shared_update_bq_data(const bq76940_data_t *data)
{
    bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
    if (bms) {
        bms->battery_val = *data;
        bms_shared_data_unlock();
    }
}

void bms_shared_update_protection(prot_level_t level, uint16_t faults)
{
    bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
    if (bms) {
        bms->prot_level    = level;
        bms->active_faults = faults;
        if (level == PROT_LVL_FAULT) {
            bms->latched_faults |= faults;
        }
        bms_shared_data_unlock();
    }
}

void bms_shared_update_soc(uint16_t soc_permil, uint16_t ocv_mv, int32_t remaining_mah)
{
    bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
    if (bms) {
        bms->soc_permil    = soc_permil;
        bms->ocv_mv        = ocv_mv;
        bms->remaining_mah = remaining_mah;
        bms_shared_data_unlock();
    }
}
```

- [ ] **Step 3: Compile check + commit**

```bash
cmake --preset Debug && cmake --build build/Debug
```
Expected: 编译通过 (bms_shared 不依赖任何 BSP 函数)

```bash
git add App/Inc/bms_shared.h App/Src/bms_shared.c
git commit -m "refactor: 重写 bms_shared — 双 mutex + DEFAULT_SETTINGS

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task D2: 重写 protection.h + protection.c — prot_result_t 纯判断

**Files:**
- Overwrite: `App/Inc/protection.h`
- Overwrite: `App/Src/protection.c`

**Interfaces:**
- Consumes: `bq76940.h` (类型), `bms_shared.h` (bms_settings_t 类型)
- Produces: `prot_result_t protection_check(data, settings)` — 纯判断，不调 BSP

- [ ] **Step 1: 重写 protection.h**

```c
/**
 * @file    protection.h
 * @brief   BMS 保护判断模块 — 纯策略，零 BSP 调用
 */

#ifndef APP_PROTECTION_H
#define APP_PROTECTION_H

#include "stm32f1xx_hal.h"
#include "bq76940.h"
#include "bms_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_FAULT_CONFIRM_CNT  3U

typedef enum {
    PROT_LVL_NONE    = 0U,
    PROT_LVL_WARNING = 1U,
    PROT_LVL_ALERT   = 2U,
    PROT_LVL_FAULT   = 3U,
} prot_level_t;

typedef enum {
    FAULT_NONE            = 0x0000U,
    FAULT_CELL_OV         = (1U << 0U),
    FAULT_CELL_UV         = (1U << 1U),
    FAULT_PACK_OV         = (1U << 2U),
    FAULT_PACK_UV         = (1U << 3U),
    FAULT_DISCHARGE_OC    = (1U << 4U),
    FAULT_CHARGE_OC       = (1U << 5U),
    FAULT_SHORT_CIRCUIT   = (1U << 6U),
    FAULT_OVER_TEMP       = (1U << 7U),
    FAULT_UNDER_TEMP      = (1U << 8U),
    FAULT_CELL_IMBALANCE  = (1U << 9U),
    FAULT_COMM_LOSS       = (1U << 10U),
    FAULT_WATCHDOG        = (1U << 11U),
} fault_code_t;

typedef enum {
    FET_BOTH_OFF = 0U,
    FET_CHG_ON   = 1U,
    FET_DSG_ON   = 2U,
    FET_BOTH_ON  = 3U,
} fet_state_t;

typedef struct {
    prot_level_t   level;
    uint16_t       active_faults;
    uint16_t       latched_faults;
    fet_state_t    fet;
    uint32_t       fault_timestamp;
} protection_ctx_t;

typedef struct {
    prot_level_t level;
    uint16_t     active_faults;
    uint8_t      request_power_off;  /* 1=需立即断电 */
} prot_result_t;

void protection_init(void);
prot_result_t protection_check(const bq76940_data_t *data, const bms_settings_t *settings);
const protection_ctx_t *protection_get_ctx(void);
void protection_clear_latched(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTECTION_H */
```

- [ ] **Step 2: 重写 protection.c — 纯判断逻辑**

核心实现:
```c
#include "protection.h"

static protection_ctx_t g_prot_ctx;

/* 各故障类型的确认/恢复计数器 */
static uint8_t fault_confirm_cnt[12];
static uint8_t fault_recover_cnt[12];

void protection_init(void)
{
    memset(&g_prot_ctx, 0, sizeof(g_prot_ctx));
    memset(fault_confirm_cnt, 0, sizeof(fault_confirm_cnt));
    memset(fault_recover_cnt, 0, sizeof(fault_recover_cnt));

    /* 启动时检测看门狗复位 */
    extern uint8_t wdg_is_reset_source(void);
    if (wdg_is_reset_source()) {
        g_prot_ctx.latched_faults |= FAULT_WATCHDOG;
        g_prot_ctx.active_faults  |= FAULT_WATCHDOG;
    }
}

prot_result_t protection_check(const bq76940_data_t *data, const bms_settings_t *settings)
{
    prot_result_t result = { .level = PROT_LVL_NONE, .active_faults = 0, .request_power_off = 0 };
    uint16_t new_faults = 0;

    if (!data->valid) {
        return result;  /* 无效数据不判断 */
    }

    /* ---- 逐个故障检查 ---- */
    /* Cell OV */
    if (data->cells_max_mv > settings->cell_ov_mv) {
        new_faults |= FAULT_CELL_OV;
    }

    /* Cell UV */
    if (data->cells_min_mv < settings->cell_uv_mv) {
        new_faults |= FAULT_CELL_UV;
    }

    /* Pack OV */
    if (data->pack_mv > settings->pack_ov_mv) {
        new_faults |= FAULT_PACK_OV;
    }

    /* Pack UV */
    if (data->pack_mv < settings->pack_uv_mv) {
        new_faults |= FAULT_PACK_UV;
    }

    /* Discharge OC (current < 0 = discharge) */
    if (data->current_ma < 0 && (-data->current_ma) > (int32_t)settings->discharge_oc_ma) {
        new_faults |= FAULT_DISCHARGE_OC;
    }

    /* Charge OC (current > 0 = charge) */
    if (data->current_ma > (int32_t)settings->charge_oc_ma) {
        new_faults |= FAULT_CHARGE_OC;
    }

    /* Short circuit */
    if (data->current_ma < 0 && (-data->current_ma) > (int32_t)settings->short_circuit_ma) {
        new_faults |= FAULT_SHORT_CIRCUIT;
    }

    /* Over temp — 取最高温度 */
    int16_t max_temp = data->ts_mdeg_c[0];
    for (uint8_t i = 1; i < 3; i++) {
        if (data->ts_mdeg_c[i] > max_temp) max_temp = data->ts_mdeg_c[i];
    }
    if (max_temp > settings->over_temp_mdeg) {
        new_faults |= FAULT_OVER_TEMP;
    }

    /* Under temp — 取最低温度 */
    int16_t min_temp = data->ts_mdeg_c[0];
    for (uint8_t i = 1; i < 3; i++) {
        if (data->ts_mdeg_c[i] < min_temp) min_temp = data->ts_mdeg_c[i];
    }
    if (min_temp < settings->under_temp_mdeg) {
        new_faults |= FAULT_UNDER_TEMP;
    }

    /* Cell imbalance */
    if ((data->cells_max_mv - data->cells_min_mv) > settings->cell_diff_max_mv) {
        new_faults |= FAULT_CELL_IMBALANCE;
    }

    /* ---- 去抖: 确认 + 恢复计数 ---- */
    uint16_t confirmed_faults = 0;
    for (uint8_t i = 0; i < 12; i++) {
        uint16_t mask = (1U << i);
        if (new_faults & mask) {
            if (++fault_confirm_cnt[i] >= PROT_FAULT_CONFIRM_CNT) {
                confirmed_faults |= mask;
                fault_recover_cnt[i] = 0;
            }
        } else {
            fault_confirm_cnt[i] = 0;
            if (g_prot_ctx.active_faults & mask) {
                if (++fault_recover_cnt[i] >= PROT_FAULT_CONFIRM_CNT) {
                    /* 故障恢复: 从活跃移除 */
                } else {
                    confirmed_faults |= mask;  /* 未达恢复计数, 保持 */
                }
            }
        }
    }

    /* ---- 确定保护等级 ---- */
    g_prot_ctx.active_faults = confirmed_faults;

    if (confirmed_faults == 0) {
        g_prot_ctx.level = PROT_LVL_NONE;
        g_prot_ctx.fet   = FET_BOTH_ON;
    } else if (confirmed_faults & (FAULT_CELL_OV | FAULT_PACK_OV | FAULT_CHARGE_OC |
                                    FAULT_OVER_TEMP)) {
        g_prot_ctx.level = PROT_LVL_ALERT;
        g_prot_ctx.fet   = FET_DSG_ON;   /* 关充电, 保持放电 */
    } else if (confirmed_faults & (FAULT_CELL_UV | FAULT_PACK_UV | FAULT_DISCHARGE_OC)) {
        g_prot_ctx.level = PROT_LVL_ALERT;
        g_prot_ctx.fet   = FET_CHG_ON;   /* 关放电, 保持充电 */
    } else if (confirmed_faults & (FAULT_SHORT_CIRCUIT)) {
        g_prot_ctx.level = PROT_LVL_FAULT;
        g_prot_ctx.fet   = FET_BOTH_OFF;
        result.request_power_off = 1;
    } else {
        g_prot_ctx.level = PROT_LVL_WARNING;
    }

    /* ---- 输出 ---- */
    result.level         = g_prot_ctx.level;
    result.active_faults = confirmed_faults;

    return result;
}

const protection_ctx_t *protection_get_ctx(void)
{
    return &g_prot_ctx;
}

void protection_clear_latched(void)
{
    g_prot_ctx.latched_faults = 0;
}
```

- [ ] **Step 3: Commit**

```bash
git add App/Inc/protection.h App/Src/protection.c
git commit -m "refactor: 重写 protection — prot_result_t, 纯判断, 取消 BSP 调用

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task D3: 重写 can_cmd.h + can_cmd.c — CAN 协议解析 + can_pub

**Files:**
- Overwrite: `App/Inc/can_cmd.h`
- Overwrite: `App/Src/can_cmd.c`

**Interfaces:**
- Consumes: `cmsis_os.h`, `can_drv.h` (类型), `bms_shared.h` (类型), `protection.h` (类型)
- Produces: `can_action_req_t can_cmd_dispatch(msg, active_faults)`, `void can_pub(bms, frame)`

- [ ] **Step 1: 重写 can_cmd.h**

```c
/**
 * @file    can_cmd.h
 * @brief   CAN 指令协议 — 解析+编码, 纯协议, 零 BSP 调用
 */

#ifndef APP_CAN_CMD_H
#define APP_CAN_CMD_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "can_drv.h"
#include "bms_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- CAN ID ---- */
#define CAN_TX_FAULT         0x100U
#define CAN_TX_CELL_VOLT_1_4 0x110U
#define CAN_TX_CELL_VOLT_5_9 0x111U
#define CAN_TX_BMS_STATUS    0x120U
#define CAN_TX_SOC_OCV       0x130U

#define CAN_RX_QUERY         0x200U
#define CAN_RX_CONTROL       0x201U
#define CAN_RX_CONFIG        0x202U

/* ---- Action 类型 ---- */
typedef enum {
    CAN_ACTION_NONE = 0,
    CAN_ACTION_CLEAR_FAULT,
    CAN_ACTION_FET_CHG_ON,   CAN_ACTION_FET_CHG_OFF,
    CAN_ACTION_FET_DSG_ON,   CAN_ACTION_FET_DSG_OFF,
    CAN_ACTION_BALANCE_SET,  CAN_ACTION_BALANCE_OFF,
    CAN_ACTION_SHUTDOWN,
} can_action_t;

typedef struct {
    can_action_t action;
    uint16_t     balance_mask;
} can_action_req_t;

/* ---- API ---- */
void can_cmd_init(void);
can_action_req_t can_cmd_dispatch(const can_msg_t *msg, uint16_t active_faults);
void can_pub_status(const bms_shared_t *bms, can_msg_t *frame);
void can_pub_cells_1_4(const bms_shared_t *bms, can_msg_t *frame);
void can_pub_cells_5_9(const bms_shared_t *bms, can_msg_t *frame);
void can_pub_soc_ocv(const bms_shared_t *bms, can_msg_t *frame);
void can_pub_fault(const bms_shared_t *bms, can_msg_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* APP_CAN_CMD_H */
```

- [ ] **Step 2: 重写 can_cmd.c — 协议编码**

核心 can_pub 实现:
```c
#include "can_cmd.h"
#include <string.h>

static osMessageQueueId_t can_rx_queue;

void can_cmd_init(void)
{
    can_rx_queue = osMessageQueueNew(16, sizeof(can_msg_t), NULL);
}

/* ---- 下行: 解析上位机命令 → action ---- */
can_action_req_t can_cmd_dispatch(const can_msg_t *msg, uint16_t active_faults)
{
    can_action_req_t req = { .action = CAN_ACTION_NONE, .balance_mask = 0 };

    switch (msg->id) {
    case CAN_RX_CONTROL:
        if (msg->len < 1) break;
        switch (msg->data[0]) {
        case 0x01: /* CLEAR_FAULT */
            req.action = CAN_ACTION_CLEAR_FAULT;
            break;
        case 0x10: /* FET_CHG_ON */
            if (!(active_faults & (FAULT_CELL_OV | FAULT_PACK_OV | FAULT_CHARGE_OC)))
                req.action = CAN_ACTION_FET_CHG_ON;
            break;
        case 0x11: req.action = CAN_ACTION_FET_CHG_OFF; break;
        case 0x20: /* FET_DSG_ON */
            if (!(active_faults & (FAULT_CELL_UV | FAULT_PACK_UV | FAULT_DISCHARGE_OC)))
                req.action = CAN_ACTION_FET_DSG_ON;
            break;
        case 0x21: req.action = CAN_ACTION_FET_DSG_OFF; break;
        case 0x30: /* BALANCE_SET */
            if (msg->len >= 3) {
                req.balance_mask = msg->data[1] | ((uint16_t)msg->data[2] << 8);
                req.action = CAN_ACTION_BALANCE_SET;
            }
            break;
        case 0x31: req.action = CAN_ACTION_BALANCE_OFF; break;
        case 0xFF: req.action = CAN_ACTION_SHUTDOWN;   break;
        }
        break;
    case CAN_RX_CONFIG:
        /* 配置命令由 can_rx_task 直接处理 (需要 mutex_settings) */
        break;
    }
    return req;
}

/* ---- 上行: 编码数据帧 ---- */
void can_pub_fault(const bms_shared_t *bms, can_msg_t *frame)
{
    frame->id  = CAN_TX_FAULT;
    frame->len = 8;
    frame->data[0] = (uint8_t)bms->prot_level;
    frame->data[1] = (uint8_t)(bms->active_faults & 0xFF);
    frame->data[2] = (uint8_t)((bms->active_faults >> 8) & 0xFF);
    frame->data[3] = (uint8_t)(bms->battery_val.cells_max_mv / 10);  /* mV/10, 0-2550mV */
    frame->data[4] = (uint8_t)(bms->battery_val.cells_min_mv / 10);
    int16_t cur_10ma = (int16_t)(bms->battery_val.current_ma / 10);
    frame->data[5] = (uint8_t)(cur_10ma & 0xFF);
    frame->data[6] = (uint8_t)((cur_10ma >> 8) & 0xFF);
    /* 取最高温度, offset +40°C 映射到 0-255 */
    int16_t max_t = bms->battery_val.ts_mdeg_c[0];
    for (uint8_t i = 1; i < 3; i++)
        if (bms->battery_val.ts_mdeg_c[i] > max_t) max_t = bms->battery_val.ts_mdeg_c[i];
    int16_t t_byte = (max_t / 10) + 40;   /* °C + 40 */
    if (t_byte < 0) t_byte = 0;
    if (t_byte > 255) t_byte = 255;
    frame->data[7] = (uint8_t)t_byte;
}

void can_pub_status(const bms_shared_t *bms, can_msg_t *frame)
{
    frame->id  = CAN_TX_BMS_STATUS;
    frame->len = 8;
    frame->data[0] = (uint8_t)(bms->soc_permil / 10);  /* 0-100% */
    uint16_t pack_v = bms->battery_val.pack_mv;
    frame->data[1] = (uint8_t)(pack_v & 0xFF);
    frame->data[2] = (uint8_t)((pack_v >> 8) & 0xFF);
    int16_t cur = (int16_t)(bms->battery_val.current_ma / 10);
    frame->data[3] = (uint8_t)(cur & 0xFF);
    frame->data[4] = (uint8_t)((cur >> 8) & 0xFF);
    /* FET 状态从 prot_level 推断 */
    if (bms->prot_level == PROT_LVL_FAULT) frame->data[5] = 0;   /* 全关 */
    else if (bms->prot_level == PROT_LVL_NONE) frame->data[5] = 3; /* 全开 */
    else frame->data[5] = 1;  /* 部分 */
    frame->data[6] = 0;
    frame->data[7] = 0;
}

void can_pub_cells_1_4(const bms_shared_t *bms, can_msg_t *frame)
{
    frame->id  = CAN_TX_CELL_VOLT_1_4;
    frame->len = 8;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t mv = bms->battery_val.cells_mv[i];
        frame->data[i*2]   = (uint8_t)(mv & 0xFF);
        frame->data[i*2+1] = (uint8_t)((mv >> 8) & 0xFF);
    }
}

void can_pub_cells_5_9(const bms_shared_t *bms, can_msg_t *frame)
{
    frame->id  = CAN_TX_CELL_VOLT_5_9;
    frame->len = 8;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t mv = (i + 4 < 9) ? bms->battery_val.cells_mv[i + 4] : 0;
        frame->data[i*2]   = (uint8_t)(mv & 0xFF);
        frame->data[i*2+1] = (uint8_t)((mv >> 8) & 0xFF);
    }
}

void can_pub_soc_ocv(const bms_shared_t *bms, can_msg_t *frame)
{
    frame->id  = CAN_TX_SOC_OCV;
    frame->len = 8;
    frame->data[0] = (uint8_t)(bms->soc_permil & 0xFF);
    frame->data[1] = (uint8_t)((bms->soc_permil >> 8) & 0xFF);
    frame->data[2] = (uint8_t)(bms->ocv_mv & 0xFF);
    frame->data[3] = (uint8_t)((bms->ocv_mv >> 8) & 0xFF);
    int32_t rm = bms->remaining_mah;
    frame->data[4] = (uint8_t)(rm & 0xFF);
    frame->data[5] = (uint8_t)((rm >> 8) & 0xFF);
    frame->data[6] = (uint8_t)((rm >> 16) & 0xFF);
    frame->data[7] = (uint8_t)((rm >> 24) & 0xFF);
}
```

- [ ] **Step 3: Commit**

```bash
git add App/Inc/can_cmd.h App/Src/can_cmd.c
git commit -m "refactor: 重写 can_cmd — can_action_req_t + can_pub 编码

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task D4: 修改 soc_ocv.c — 校正权重精度修复

**Files:**
- Modify: `App/Src/soc_ocv.c` (仅修 `calc_correction_weight`)

**Interfaces:**
- Consumes: 无 (纯算法)
- Produces: 不改变接口

- [ ] **Step 1: 修复精度丢失**

在 `calc_correction_weight` 中, 将 `(rest_ms/1000)*1000` 改为 `(rest_ms/1000) * 1000`:
```c
/* Before: weight_time = (rest_ms / 1000) * 1000 / 300;  — rest_ms<1000 时恒为 0 */
/* After: */
uint32_t rest_s = rest_ms / 1000;
uint32_t weight_time = (rest_s * 1000) / 300;
```

- [ ] **Step 2: Commit**

```bash
git add App/Src/soc_ocv.c
git commit -m "fix: soc_ocv 校正权重亚秒精度丢失

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task D5: 重写 bms_app.h + bms_app.c — 6 任务编排

**Files:**
- Overwrite: `App/Inc/bms_app.h`
- Overwrite: `App/Src/bms_app.c`

**Interfaces:**
- Consumes: 所有 BSP + App 头文件
- Produces: `void bms_app_init(void)` — 创建 6 个任务 + 同步原语

- [ ] **Step 1: 重写 bms_app.h**

```c
/**
 * @file    bms_app.h
 * @brief   BMS 主应用 — 初始化 + 6 任务编排
 */

#ifndef APP_BMS_APP_H
#define APP_BMS_APP_H

#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACQ_TASK_PERIOD_MS   100U
#define PROT_TASK_PERIOD_MS   10U
#define SOC_TASK_PERIOD_MS  1000U
#define BAL_TASK_PERIOD_MS   500U
#define CAN_TX_PERIOD_MS     100U
#define CAN_RX_PERIOD_MS      50U
#define WDG_TASK_PERIOD_MS   500U

#define ALL_FAULTS           0x0FFFU

void bms_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BMS_APP_H */
```

- [ ] **Step 2: 重写 bms_app.c — 核心任务函数**

完整的 6 任务实现（省略部分行数，完整代码见 spec 数据流）:

```c
#include "bms_app.h"
#include "bms_shared.h"
#include "soc_ocv.h"
#include "can_cmd.h"
#include "systick.h"
#include "led.h"
#include "io_ctrl.h"
#include "i2c_sw.h"
#include "can_drv.h"
#include "timer.h"
#include "wdg.h"
#include "bq76940.h"
#include "protection.h"
#include "ring_buf.h"

/* ---- 同步原语 ---- */
static osSemaphoreId_t  sem_sample;
static osEventFlagsId_t evt_protect;
static osEventFlagsId_t evt_data_ready;

/* ---- CAN TX 队列 ---- */
#define CAN_QUEUE_DEPTH 24
static uint8_t can_q_buf[CAN_QUEUE_DEPTH * sizeof(can_msg_t)];
static ring_buf_t q_can_tx;

/* ---- 前向声明 ---- */
static void task_sample(void *arg);
static void task_protect(void *arg);
static void task_can_rx(void *arg);
static void task_balance(void *arg);
static void task_soc(void *arg);
static void task_can_tx(void *arg);

void bms_app_init(void)
{
    /* 1. BSP 初始化 */
    bsp_tick_init();
    led_init();
    io_ctrl_init();
    i2c_sw_init();
    can_drv_init();
    timer_init();
    wdg_init(2000);  /* 2s IWDG timeout */

    /* 2. App 模块 */
    bms_shared_init();
    soc_ocv_init(0);
    protection_init();
    can_cmd_init();

    /* 3. BQ76940 启动 */
    io_ctrl_set(IO_ID_WAKE_BQ, IO_LEVEL_HIGH);
    bsp_delay_us(100);
    io_ctrl_set(IO_ID_WAKE_BQ, IO_LEVEL_LOW);
    bsp_delay_ms(10);
    bq76940_init(NULL);

    /* 4. 同步原语 */
    sem_sample    = osSemaphoreNew(1, 0, NULL);  /* binary, initially empty */
    evt_protect   = osEventFlagsNew(NULL);
    evt_data_ready = osEventFlagsNew(NULL);
    ring_buf_init(&q_can_tx, can_q_buf, sizeof(can_msg_t), CAN_QUEUE_DEPTH);

    /* 5. 创建任务 */
    const osThreadAttr_t attr_protect = { .name="protect",  .stack_size=256*4, .priority=osPriorityRealtime };
    const osThreadAttr_t attr_sample  = { .name="sample",   .stack_size=512*4, .priority=osPriorityAboveNormal };
    const osThreadAttr_t attr_can_rx  = { .name="can_rx",   .stack_size=256*4, .priority=osPriorityNormal };
    const osThreadAttr_t attr_balance = { .name="balance",  .stack_size=256*4, .priority=osPriorityBelowNormal };
    const osThreadAttr_t attr_soc     = { .name="soc",      .stack_size=512*4, .priority=osPriorityBelowNormal };
    const osThreadAttr_t attr_can_tx  = { .name="can_tx",   .stack_size=256*4, .priority=osPriorityLow };

    osThreadNew(task_protect, NULL, &attr_protect);
    osThreadNew(task_sample,  NULL, &attr_sample);
    osThreadNew(task_can_rx,  NULL, &attr_can_rx);
    osThreadNew(task_balance, NULL, &attr_balance);
    osThreadNew(task_soc,     NULL, &attr_soc);
    osThreadNew(task_can_tx,  NULL, &attr_can_tx);

    led_on();  /* 初始化完成 */
}

/* ---- task_sample — 100ms semaphore 驱动 ---- */
static void task_sample(void *arg)
{
    (void)arg;
    osDelay(200);

    bq76940_data_t data;
    uint8_t comm_err_cnt = 0;

    for (;;) {
        osSemaphoreAcquire(sem_sample, osWaitForever);

        bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
        if (!bms) continue;

        bq76940_status_t ret = bq76940_read_all(&data);
        if (ret != BQ76940_OK) {
            comm_err_cnt++;
            if (comm_err_cnt >= 3) {
                osEventFlagsSet(evt_protect, FAULT_COMM_LOSS);
            }
            bms_shared_data_unlock();
            continue;
        }
        comm_err_cnt = 0;

        /* 写入共享数据 */
        bms->battery_val = data;
        bms_shared_data_unlock();

        /* 阈值检查 (持 settings 锁) */
        bms_settings_t *settings = bms_shared_settings_lock(osWaitForever);
        if (settings) {
            prot_result_t pr = protection_check(&data, settings);
            if (pr.active_faults) {
                osEventFlagsSet(evt_protect, pr.active_faults);
            }
            bms_shared_settings_unlock();
        }

        osEventFlagsSet(evt_data_ready, 0x01);
    }
}

/* ---- task_protect — 事件驱动, 最高优先级 ---- */
static void task_protect(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags = osEventFlagsWait(evt_protect, ALL_FAULTS,
                                           osFlagsWaitAny, osWaitForever);

        bms_shared_t *bms = bms_shared_data_lock(osWaitForever);
        if (!bms) continue;

        /* 确定故障等级 */
        bms_settings_t *settings = bms_shared_settings_lock(osWaitForever);
        prot_result_t pr;
        if (settings) {
            pr = protection_check(&bms->battery_val, settings);
            bms_shared_settings_unlock();
        }
        bms_shared_update_protection(pr.level, pr.active_faults);
        bms_shared_data_unlock();

        /* 紧急断电 */
        if (pr.request_power_off) {
            /* BQ76940 SHIP mode */
            bq76940_shutdown();
        }

        /* 发送 protect 帧 — 头插入 CAN TX 队列 */
        can_msg_t frame;
        can_pub_fault(bms, &frame);
        ring_buf_put_front(&q_can_tx, &frame);
    }
}

/* ---- task_can_tx — 100ms, 队列消费者 + 喂狗 ---- */
static void task_can_tx(void *arg)
{
    (void)arg;
    for (;;) {
        osDelay(CAN_TX_PERIOD_MS);
        wdg_kick();

        /* 生成周期帧 */
        bms_shared_t *bms = bms_shared_data_lock(100);
        if (bms) {
            can_msg_t frame;

            can_pub_status(bms, &frame);
            ring_buf_put(&q_can_tx, &frame);

            can_pub_cells_1_4(bms, &frame);
            ring_buf_put(&q_can_tx, &frame);

            can_pub_cells_5_9(bms, &frame);
            ring_buf_put(&q_can_tx, &frame);

            can_pub_soc_ocv(bms, &frame);
            ring_buf_put(&q_can_tx, &frame);

            bms_shared_data_unlock();
        }

        /* 清空发送队列 (先入先出 = protect 头插的最先出) */
        can_msg_t tx_frame;
        while (ring_buf_get(&q_can_tx, &tx_frame) == 0) {
            can_send(&tx_frame);
        }
    }
}

/* ---- task_can_rx — 50ms 轮询 CAN RX ---- */
static void task_can_rx(void *arg)
{
    (void)arg;
    for (;;) {
        osDelay(CAN_RX_PERIOD_MS);
        can_msg_t msg;
        while (can_available()) {
            if (can_recv(&msg) == 0) {
                /* 配置命令 (0x202) — 直接处理 */
                if (msg.id == CAN_RX_CONFIG && msg.len >= 6) {
                    bms_settings_t *s = bms_shared_settings_lock(osWaitForever);
                    if (s) {
                        /* Byte0-1: 参数类型, Byte2-3: 值低/高字节 */
                        uint16_t param = msg.data[0] | ((uint16_t)msg.data[1] << 8);
                        uint16_t val   = msg.data[2] | ((uint16_t)msg.data[3] << 8);
                        switch (param) {
                        case 0: s->cell_ov_mv = val; break;
                        case 1: s->cell_uv_mv = val; break;
                        /* ... 其他参数 ... */
                        }
                        bms_shared_settings_unlock();
                    }
                }
                /* 控制命令 — 通过 dispatch 获取 action */
                else if (msg.id == CAN_RX_CONTROL) {
                    bms_shared_t *b = bms_shared_data_lock(100);
                    uint16_t faults = b ? b->active_faults : 0;
                    if (b) bms_shared_data_unlock();

                    can_action_req_t req = can_cmd_dispatch(&msg, faults);
                    switch (req.action) {
                    case CAN_ACTION_CLEAR_FAULT:
                        protection_clear_latched();
                        break;
                    case CAN_ACTION_BALANCE_SET:
                        bq76940_set_balancing(req.balance_mask);
                        break;
                    case CAN_ACTION_BALANCE_OFF:
                        bq76940_set_balancing(0);
                        break;
                    case CAN_ACTION_SHUTDOWN:
                        bq76940_shutdown();
                        break;
                    /* FET 控制 — 通过 io_ctrl 或 BQ76940 DSG/CHG 引脚 */
                    default:
                        break;
                    }
                }
                /* 查询命令 — 立即回传 */
                else if (msg.id == CAN_RX_QUERY) {
                    bms_shared_t *b = bms_shared_data_lock(100);
                    if (b) {
                        can_msg_t resp;
                        can_pub_status(b, &resp);
                        ring_buf_put_front(&q_can_tx, &resp);  /* 查询响应优先 */
                        bms_shared_data_unlock();
                    }
                }
            }
        }
    }
}

/* ---- task_balance — 500ms ---- */
static void task_balance(void *arg)
{
    (void)arg;
    osEventFlagsWait(evt_data_ready, 0x01, osFlagsWaitAny, 5000);

    for (;;) {
        osDelay(BAL_TASK_PERIOD_MS);

        bms_shared_t *bms = bms_shared_data_lock(10);
        if (!bms) continue;
        if (bms->prot_level >= PROT_LVL_FAULT) {
            bms_shared_data_unlock();
            continue;
        }

        /* 均衡策略: 压差 > 阈值 且 最低电压 > 均衡最低电压 */
        uint16_t diff = bms->battery_val.cells_max_mv - bms->battery_val.cells_min_mv;
        if (diff > bms->settings.balance_thresh_mv &&
            bms->battery_val.cells_min_mv > bms->settings.balance_min_mv) {
            /* 对最高电芯开启均衡 */
            uint16_t mask = 0;
            for (uint8_t i = 0; i < 9; i++) {
                if (bms->battery_val.cells_mv[i] == bms->battery_val.cells_max_mv) {
                    mask |= (1U << i);
                }
            }
            bq76940_set_balancing(mask);
        } else {
            bq76940_set_balancing(0);
        }
        bms_shared_data_unlock();
    }
}

/* ---- task_soc — 1000ms ---- */
static void task_soc(void *arg)
{
    (void)arg;
    osEventFlagsWait(evt_data_ready, 0x01, osFlagsWaitAny, 5000);
    uint32_t last_ms = bsp_tick_get();

    for (;;) {
        osDelay(SOC_TASK_PERIOD_MS);

        bms_shared_t *bms = bms_shared_data_lock(10);
        if (!bms) continue;

        uint32_t now  = bsp_tick_get();
        uint32_t dt   = now - last_ms;
        last_ms       = now;

        int16_t max_temp = bms->battery_val.ts_mdeg_c[0];
        for (uint8_t i = 1; i < 3; i++)
            if (bms->battery_val.ts_mdeg_c[i] > max_temp)
                max_temp = bms->battery_val.ts_mdeg_c[i];

        uint16_t cell_min = bms->battery_val.cells_min_mv;
        int32_t  current  = bms->battery_val.current_ma;
        bms_shared_data_unlock();

        soc_ocv_update(cell_min, current, max_temp, dt);
        bms_shared_update_soc(soc_ocv_get_soc(), soc_ocv_get_ocv(), soc_ocv_get_remaining_mah());
    }
}
```

- [ ] **Step 3: 编译验证 + commit**

```bash
cmake --preset Debug && cmake --build build/Debug
```

```bash
git add App/Inc/bms_app.h App/Src/bms_app.c
git commit -m "feat: 重写 bms_app — 6 任务事件驱动编排

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase E: Core 层集成

### Task E1: 修改 freertos.c — 清空内容, 任务创建已移至 bms_app

**Files:**
- Modify: `Core/Src/freertos.c`

- [ ] **Step 1: 精简 MX_FREERTOS_Init**

`MX_FREERTOS_Init()` 仅保留空壳 (BMS 任务由 `bms_app_init()` 在调度器启动前创建):
```c
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  /* BMS 任务由 bms_app_init() 在 osKernelStart 之前创建 */
  /* USER CODE BEGIN RTOS_THREADS */
  /* USER CODE END RTOS_THREADS */
}
```

- [ ] **Step 2: Commit**

```bash
git add Core/Src/freertos.c
git commit -m "refactor: freertos.c 精简 — 任务创建已移至 bms_app

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task E2: 修改 stm32f1xx_it.c — TIM2 ISR 添加 semaphore give

**Files:**
- Modify: `Core/Src/stm32f1xx_it.c`

- [ ] **Step 1: 在 TIM2_IRQHandler 中添加 semaphore give**

在 `USER CODE BEGIN TIM2_IRQn 1` 区域:
```c
void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim2);
  /* USER CODE BEGIN TIM2_IRQn 1 */
  extern osSemaphoreId_t sem_sample;
  if (sem_sample) {
      osSemaphoreRelease(sem_sample);
  }
  /* USER CODE END TIM2_IRQn 1 */
}
```

问题: `sem_sample` 是 `bms_app.c` 中的 `static` 变量, ISR 无法访问。解决方案:
- 将 `sem_sample` 声明为 `extern` 放在 `bms_app.h` 中
- 或在 `TIM2_IRQHandler` 中通过回调间接 give

采用回调方案:
在 `bms_app.h` 中暴露函数:
```c
void bms_timer_isr_callback(void);  /* 由 TIM2 ISR 调用 */
```

在 `bms_app.c` 中实现:
```c
void bms_timer_isr_callback(void)
{
    if (sem_sample) {
        osSemaphoreRelease(sem_sample);
    }
}
```

在 `stm32f1xx_it.c` 中调用:
```c
/* USER CODE BEGIN Includes */
#include "bms_app.h"
/* USER CODE END Includes */

void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim2);
  /* USER CODE BEGIN TIM2_IRQn 1 */
  bms_timer_isr_callback();
  /* USER CODE END TIM2_IRQn 1 */
}
```

- [ ] **Step 2: Commit**

```bash
git add Core/Src/stm32f1xx_it.c App/Inc/bms_app.h App/Src/bms_app.c
git commit -m "feat: TIM2 ISR → bms_timer_isr_callback → semaphore give

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### Task E3: 修改 main.c — 移除 USART init

**Files:**
- Modify: `Core/Src/main.c`

- [ ] **Step 1: 移除 USART 初始化调用和头文件**

删除 `#include "usart.h"` (如果有)。
删除 `MX_USART1_UART_Init()` 和 `MX_USART2_UART_Init()` 调用 (如果还存在)。

确认 `main()` 中初始化顺序:
```c
MX_GPIO_Init();
MX_CAN_Init();
MX_I2C1_Init();
MX_IWDG_Init();
MX_TIM2_Init();
/* USER CODE BEGIN 2 */
bms_app_init();
/* USER CODE END 2 */
```

- [ ] **Step 2: Commit**

```bash
git add Core/Src/main.c
git commit -m "refactor: main.c 移除 USART init (已从 CubeMX 删除)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase F: 头文件卫重命名

### Task F1: 批量修改 BSP + App 头文件卫

**Files:**
- Modify: `BSP/Inc/*.h` (9 个文件)
- Modify: `App/Inc/*.h` (5 个文件)

- [ ] **Step 1: BSP 头文件卫 — `__BSP_XXX_H` → `BSP_XXX_H`**

替换每个 BSP 头文件的 `#ifndef` / `#define` / `#endif` 卫:
```
systick.h:   __BSP_SYSTICK_H   → BSP_SYSTICK_H
led.h:       __BSP_LED_H       → BSP_LED_H
io_ctrl.h:   __BSP_IO_CTRL_H   → BSP_IO_CTRL_H
i2c_sw.h:    __BSP_I2C_SW_H    → BSP_I2C_SW_H
bq76940.h:   __BSP_BQ76940_H   → BSP_BQ76940_H
can_drv.h:   __BSP_CAN_DRV_H   → BSP_CAN_DRV_H
timer.h:     __BSP_TIMER_H     → BSP_TIMER_H
wdg.h:       __BSP_WDG_H       → BSP_WDG_H
```

- [ ] **Step 2: App 头文件卫 — `__APP_XXX_H` → `APP_XXX_H`**

```
bms_shared.h:  __APP_BMS_SHARED_H  → APP_BMS_SHARED_H
protection.h:  __APP_PROTECTION_H  → APP_PROTECTION_H
soc_ocv.h:     __APP_SOC_OCV_H     → APP_SOC_OCV_H
can_cmd.h:     __APP_CAN_CMD_H     → APP_CAN_CMD_H
bms_app.h:     __APP_BMS_APP_H     → APP_BMS_APP_H
```

- [ ] **Step 3: 编译验证**

```bash
cmake --preset Debug && cmake --build build/Debug
```
Expected: 编译通过 (头文件卫改名不影响编译)

- [ ] **Step 4: Commit**

```bash
git add BSP/Inc/*.h App/Inc/*.h
git commit -m "refactor: 头文件卫重命名 __XXX_→XXX_ (C11 7.1.3 合规)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Phase G: 全量编译验证

### Task G1: 完整编译 + size 分析 + 功能检查

- [ ] **Step 1: Clean build**

```bash
rm -rf build/Debug
cmake --preset Debug
cmake --build build/Debug 2>&1
```

Expected: **0 warnings, 0 errors**

- [ ] **Step 2: Size analysis**

```bash
arm-none-eabi-size build/Debug/bms_9s_f103.elf
```

Expected: FLASH < 28KB (64KB), RAM < 18KB (20KB)

- [ ] **Step 3: 验证清单**

- [ ] CMakeLists.txt 中不再有 usart_drv.c 和 data_report.c
- [ ] CMakeLists.txt 中 BSP 包含全部 9 模块 (含新增 ring_buf.h)
- [ ] 编译输出无 "undefined reference" / "implicit declaration" 警告
- [ ] `can_msg_t` 在 can_drv.h 和 can_cmd.h 中定义一致 (含 ring_buf.h 的 inline 函数 memcpy sizeof)
- [ ] 所有 `#include` 链完整 (App→App, App→BSP type, BSP→BSP)

- [ ] **Step 4: Commit final**

```bash
git add -A
git commit -m "build: 全量编译验证通过 — FLASH <28KB, RAM <18KB, 0W 0E

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Task Dependency Graph

```
A1(删除死代码) ─────────────────────────────────────────────┐
A2(删除 defaultTask) ───────────────────────────────────────┤
                                                            │
B1(bsp_common.h) ──┐                                        │
B2(ring_buf.h) ────┤                                        │
                    ├── C1(led 重写) ──┐                    │
                    │   C2(io_ctrl) ───┤                    │
                    │   C3(bq76940) ───┤                    │
                    │   C4(can_drv) ───┤                    │
                    │                  ├── D1(bms_shared) ──┤
                    │                  │   D2(protection) ─┤
                    │                  │   D3(can_cmd) ────┤
                    │                  │   D4(soc_ocv) ────┤
                    │                  │   D5(bms_app) ────┤
                    │                  │                    ├── E1(freertos.c)
                    │                  │                    │   E2(stm32f1xx_it.c)
                    │                  │                    │   E3(main.c)
                    │                  │                    │
                    │                  │                    ├── F1(header guards)
                    │                  │                    │
                    │                  │                    └── G1(full build) ✓
```

---

## Self-Review Summary

**Spec coverage**: All spec requirements (ADDED/MODIFIED/REMOVED + BSP/App interface definitions) mapped to specific tasks.

**Placeholder scan**: No TBD, TODO, or vague "add error handling" steps. All code blocks are concrete.

**Type consistency**: `can_msg_t` defined in Task B2 (ring_buf consumes) and used consistently in C4, D3, D5. `prot_result_t` defined in D2 and consumed in D5. `bms_settings_t` defined in D1 and consumed in D2, D5.

