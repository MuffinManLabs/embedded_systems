#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/*
 * SysTick driver
 *
 * Wraps the ARM Cortex-M4 System Timer to provide a 1 ms time base for
 * the air quality monitor. Used for blocking init delays (delay_ms)
 * and non-blocking periodic scheduling (systick_get_ms) in the main
 * loop.
 *
 * Reference:
 *   Cortex-M4 Generic User Guide ARM DUI 0553B, Section 4.4
 *   RM0090 Section 7.2 (SysTick clock source on STM32F405/07/15/17)
 */

/* ------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------ */

/*
 * Initializes the SysTick timer to fire an interrupt every 1 ms.
 *
 * Configures three of the four SysTick registers (STCSR, STRVR, STCVR)
 * to count down at HCLK rate. The fourth register (STCALIB) is
 * read-only calibration data and is not modified.
 *
 * Must be called once at startup, after rcc_init_clock_168mhz().
 */
void systick_init(void);

/*
 * Blocks the calling function for at least `ms` milliseconds.
 *
 * The CPU continues running and remains responsive to interrupts;
 * only the caller's forward progress is paused. Intended for one-off
 * init delays (e.g. waiting after a sensor power-up). Should not be
 * used inside the main loop, where systick_get_ms() with a non-blocking
 * pattern is preferred.
 */
void delay_ms(uint32_t ms);

/*
 * Returns the number of milliseconds elapsed since systick_init() was
 * called.
 *
 * Wraps to zero after approximately 49.7 days of continuous operation
 * (UINT32_MAX milliseconds). Callers comparing two timestamps should
 * use unsigned subtraction so the comparison survives the wrap:
 *
 *     if ((systick_get_ms() - last_event) >= INTERVAL_MS) { ... }
 */
uint32_t systick_get_ms(void);

#endif /* SYSTICK_H */
