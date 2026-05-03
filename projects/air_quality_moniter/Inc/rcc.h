#ifndef RCC_H
#define RCC_H

/*
 * RCC (Reset and Clock Control) driver
 *
 * Configures the system clock to run at 168 MHz using the external 8 MHz
 * crystal (HSE) on the STM32F4 Discovery board, via the main PLL.
 *
 * Reference:
 *   RM0090 Section 6 (RCC for STM32F405/07/15/17)
 *   STM32F407VGT6 datasheet Table 13 (clock characteristics)
 */

#include <stdint.h>

/* ------------------------------------------------------------------------
 * Clock frequencies after rcc_init_clock_168mhz()
 * ------------------------------------------------------------------------
 * Other drivers (USART, SPI, SysTick, etc.) should derive their timing
 * from these constants rather than hardcoding numbers.
 */
#define RCC_SYSCLK_HZ   168000000UL  /* System clock                     */
#define RCC_HCLK_HZ     168000000UL  /* AHB bus clock (= SYSCLK / 1)     */
#define RCC_PCLK1_HZ     42000000UL  /* APB1 bus clock (= HCLK / 4)      */
#define RCC_PCLK2_HZ     84000000UL  /* APB2 bus clock (= HCLK / 2)      */

/* ------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------ */

/**
 * Configure SYSCLK to 168 MHz from the 8 MHz external crystal (HSE).
 *
 * Sequence (per RM0090 Section 3.5.1 + 6.3.2):
 *   1. Enable HSE and wait for ready
 *   2. Configure flash latency to 5 wait states + enable ART (prefetch + caches)
 *   3. Configure AHB / APB bus prescalers
 *   4. Configure main PLL (M=4, N=168, P=2, Q=7) sourced from HSE
 *   5. Enable PLL and wait for lock
 *   6. Switch SYSCLK source to PLL
 *
 * Must be called once at startup, before initializing any peripheral
 * whose timing depends on bus clocks (USART baud, SPI prescaler, SysTick,
 * I2C timing, etc.).
 */
void rcc_init_clock_168mhz(void);

#endif /* RCC_H */
