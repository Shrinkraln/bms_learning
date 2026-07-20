/**
 * @file    wdg.c
 * @brief   BSP 层独立看门狗 (IWDG) 驱动实现
 * @note    IWDG 由 LSI (~40kHz) 驱动
 *          Prescaler 和 Reload 值与超时关系:
 *          - /4:   0.1ms/tick  → Reload=4095 → ~409ms
 *          - /8:   0.2ms/tick  → Reload=4095 → ~819ms
 *          - /16:  0.4ms/tick  → Reload=4095 → ~1.6s
 *          - /32:  0.8ms/tick  → Reload=4095 → ~3.2s
 *          - /64:  1.6ms/tick  → Reload=4095 → ~6.5s
 *          - /128: 3.2ms/tick  → Reload=4095 → ~13.1s
 *          - /256: 6.4ms/tick  → Reload=4095 → ~26.2s
 *
 *          注意: CubeMX 已调用 MX_IWDG_Init() 配置了硬件参数
 *          本模块提供应用层封装
 */

#include "wdg.h"
#include "iwdg.h"

/* ============================================================
 * 模块内变量
 * ============================================================ */

/** @brief 当前超时设置 (ms) */
static uint32_t wdg_timeout_ms = 0U;

/** @brief 看门狗是否已初始化 */
static uint8_t wdg_initialized = 0U;

/* ============================================================
 * 初始化
 * ============================================================ */

wdg_status_t wdg_init(uint32_t timeout_ms)
{
    /* 如果 CubeMX 已初始化 IWDG，此处仅做参数记录和刷新 */
    /* IWDG 一旦使能无法重配置，除非硬件复位 */

    if (wdg_initialized == 1U) {
        /* 已初始化，仅更新超时记录 */
        wdg_timeout_ms = timeout_ms;
        return WDG_OK;
    }

    /* 首次初始化: 刷新计数器 */
    HAL_IWDG_Refresh(&hiwdg);
    wdg_timeout_ms = timeout_ms;
    wdg_initialized = 1U;

    return WDG_OK;
}

/* ============================================================
 * 喂狗
 * ============================================================ */

wdg_status_t wdg_kick(void)
{
    if (wdg_initialized == 0U) {
        return WDG_ERROR;
    }

    /* 检查是否在 ISR 中调用（不推荐在 ISR 中喂狗） */
    if ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0U) {
        /* 在 ISR 中，跳过喂狗（避免隐藏 ISR 死循环问题） */
        return WDG_OK;
    }

    HAL_IWDG_Refresh(&hiwdg);
    return WDG_OK;
}

/* ============================================================
 * 状态查询
 * ============================================================ */

uint16_t wdg_get_counter(void)
{
    /* STM32F1 HAL 没有 __HAL_IWDG_GET_COUNTER 宏，直接读取重载寄存器 */
    /* 注意：IWDG_RLR 读取需要在 KR=0x5555（解锁）之后 */
    return (uint16_t)(IWDG->RLR);
}

uint8_t wdg_is_reset_source(void)
{
    /* 检查 RCC 控制/状态寄存器中的 IWDG 复位标志 */
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) {
        SET_BIT(RCC->CSR, RCC_CSR_RMVF);
        return 1U;
    }
    return 0U;
}

/* ============================================================
 * 调试辅助
 * ============================================================ */

void wdg_debug_pause(void)
{
    /* 使能 DBGMCU 中的 IWDG 暂停位
     * 在调试模式下（连接调试器），IWDG 计数器暂停 */
    __HAL_DBGMCU_FREEZE_IWDG();
}
