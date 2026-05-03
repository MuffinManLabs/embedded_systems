/*
 * rcc.c - Reset and Clock Control driver for STM32F407VGT6
 *
 * Configures SYSCLK = 168 MHz from the Discovery board's 8 MHz HSE crystal
 * via the main PLL.
 *
 * Reference:
 *   RM0090 Section 3.5   (flash interface - wait states & ART)
 *   RM0090 Section 3.9.1 (FLASH_ACR bit definitions, F405xx/07xx variant)
 *   RM0090 Section 6.2   (RCC clocks overview)
 *   RM0090 Section 6.3   (RCC register map, F405xx/07xx variant)
 *   STM32F407VGT6 datasheet Table 13 (clock characteristics, max speeds)
 */

#include "rcc.h"
#include "stm32f407xx.h"


/* ============================================================================
 * Bit positions (private to this file)
 *
 * Industry-standard practice: name every magic number. These #defines mirror
 * the bit names used in RM0090, so cross-referencing the manual is direct.
 * ============================================================================ */

/* RCC_CR - Clock control register */
#define RCC_CR_HSEON                (1U << 16)  /* HSE oscillator enable        */
#define RCC_CR_HSERDY               (1U << 17)  /* HSE oscillator ready flag    */
#define RCC_CR_PLLON                (1U << 24)  /* Main PLL enable              */
#define RCC_CR_PLLRDY               (1U << 25)  /* Main PLL locked flag         */

/* RCC_PLLCFGR - PLL configuration register
 * Layout: PLLQ[27:24] PLLSRC[22] PLLP[17:16] PLLN[14:6] PLLM[5:0] */
#define RCC_PLLCFGR_PLLM_POS        0U
#define RCC_PLLCFGR_PLLN_POS        6U
#define RCC_PLLCFGR_PLLP_POS        16U
#define RCC_PLLCFGR_PLLSRC_HSE      (1U << 22)  /* PLL source = HSE (vs HSI)    */
#define RCC_PLLCFGR_PLLQ_POS        24U

/* RCC_CFGR - Clock configuration register
 * Layout (relevant fields): PPRE2[15:13] PPRE1[12:10] HPRE[7:4] SWS[3:2] SW[1:0] */
#define RCC_CFGR_SW_MASK            (3U << 0)
#define RCC_CFGR_SW_PLL             (2U << 0)   /* Select PLL as SYSCLK source  */
#define RCC_CFGR_SWS_MASK           (3U << 2)
#define RCC_CFGR_SWS_PLL            (2U << 2)   /* SYSCLK is PLL (status)       */
#define RCC_CFGR_HPRE_POS           4U
#define RCC_CFGR_HPRE_DIV1          (0U << 4)   /* AHB prescaler = 1            */
#define RCC_CFGR_PPRE1_POS          10U
#define RCC_CFGR_PPRE1_DIV4         (5U << 10)  /* APB1 prescaler = 4 -> 42 MHz */
#define RCC_CFGR_PPRE2_POS          13U
#define RCC_CFGR_PPRE2_DIV2         (4U << 13)  /* APB2 prescaler = 2 -> 84 MHz */

/* FLASH_ACR - Flash access control register (F405xx/07xx variant)
 * LATENCY is a 3-bit field [2:0] on this chip; the F42xxx/F43xxx variant
 * widened it to 4 bits, so do not reuse this header on those chips. */
#define FLASH_ACR_LATENCY_MASK      (7U << 0)   /* 3-bit field [2:0]            */
#define FLASH_ACR_LATENCY_5WS       (5U << 0)   /* 5 wait states for 168 MHz    */
#define FLASH_ACR_PRFTEN            (1U << 8)   /* Prefetch buffer enable       */
#define FLASH_ACR_ICEN              (1U << 9)   /* Instruction cache enable     */
#define FLASH_ACR_DCEN              (1U << 10)  /* Data cache enable            */
#define FLASH_ACR_ICRST             (1U << 11)  /* Instruction cache reset      */
#define FLASH_ACR_DCRST             (1U << 12)  /* Data cache reset             */


/* ============================================================================
 * PLL parameters for SYSCLK = 168 MHz from HSE = 8 MHz
 *
 *   VCO_in  = HSE / M    = 8 MHz / 4   = 2 MHz       (target: 1-2 MHz)
 *   VCO_out = VCO_in x N = 2 MHz x 168 = 336 MHz     (valid: 100-432 MHz)
 *   SYSCLK  = VCO_out / P = 336 / 2    = 168 MHz     (max: 168 MHz)
 *   USB_CLK = VCO_out / Q = 336 / 7    = 48 MHz      (USB requires 48 MHz)
 * ============================================================================ */
#define PLL_M_VAL   4U
#define PLL_N_VAL   168U
#define PLL_P_VAL   0U   /* P=2, encoded as 00 (the PLLP field encoding)        */
#define PLL_Q_VAL   7U


/* ============================================================================
 * Private helpers - one per step in the canonical RCC init sequence
 * ============================================================================ */
static void rcc_enable_hse(void);
static void rcc_configure_flash_latency(void);
static void rcc_configure_bus_prescalers(void);
static void rcc_configure_pll(void);
static void rcc_enable_pll_and_wait(void);
static void rcc_switch_sysclk_to_pll(void);


/* ============================================================================
 * Public API
 *
 * Sequence (industry-standard pattern; see RM0090 Sections 3.5, 6.2, 6.3
 * for individual register details).
 * ============================================================================ */
void rcc_init_clock_168mhz(void)
{
    rcc_enable_hse();
    rcc_configure_flash_latency();
    rcc_configure_bus_prescalers();
    rcc_configure_pll();
    rcc_enable_pll_and_wait();
    rcc_switch_sysclk_to_pll();
}


/* ============================================================================
 * Step 1 - Enable HSE (8 MHz external crystal) and wait for it to stabilize
 *
 * The HSE provides a more accurate reference than the HSI for the PLL.
 * RM0090 Section 6.2.1.
 * ============================================================================ */
static void rcc_enable_hse(void)
{
    /* Turn on the external oscillator circuit. */
    RCC->CR |= RCC_CR_HSEON;

    /* Wait for hardware to confirm the crystal is oscillating cleanly.
     * Typically takes a few hundred microseconds. */
    while ((RCC->CR & RCC_CR_HSERDY) == 0U) {
        /* spin */
    }
}


/* ============================================================================
 * Step 2 - Configure flash latency and enable ART accelerator
 *
 * At 168 MHz the flash array cannot deliver instructions in a single HCLK
 * cycle. Five wait states are required (RM0090 Table 11, 2.7-3.6 V range).
 *
 * The ART Accelerator (prefetch buffer + instruction cache + data cache)
 * hides most of these wait cycles for typical code.
 *
 * MUST run before raising HCLK to 168 MHz, otherwise the CPU reads garbage.
 *
 * Cache-reset sequencing per RM0090 3.9.1:
 *   - ICRST/DCRST may only be written when ICEN/DCEN are 0.
 *   - We rely on the reset state (ACR = 0, caches disabled) to allow the
 *     reset bits to be set in the first write, then clear them while
 *     enabling caches in the second write.
 * ============================================================================ */
static void rcc_configure_flash_latency(void)
{
    /* Write 1: set wait states and pulse the cache-reset bits while caches
     * are still disabled. */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_ICRST
               | FLASH_ACR_DCRST;

    /* Write 2: clear the reset bits and enable prefetch + I-cache + D-cache. */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;
}


/* ============================================================================
 * Step 3 - Configure AHB / APB bus prescalers
 *
 *   HCLK  = SYSCLK / 1 = 168 MHz   (CPU + AHB peripherals; max 168 MHz)
 *   PCLK1 = HCLK   / 4 =  42 MHz   (APB1 peripherals;    max  42 MHz)
 *   PCLK2 = HCLK   / 2 =  84 MHz   (APB2 peripherals;    max  84 MHz)
 *
 * Configure these BEFORE switching SYSCLK to PLL. Otherwise PCLK1 would be
 * 168 MHz the instant SYSCLK jumps to 168 MHz - 4x over its 42 MHz cap.
 * ============================================================================ */
static void rcc_configure_bus_prescalers(void)
{
    uint32_t cfgr = RCC->CFGR;

    /* Clear the three prescaler fields with their full widths:
     *   HPRE  is 4 bits  [7:4]  -> mask 0xF
     *   PPRE1 is 3 bits  [12:10] -> mask 0x7
     *   PPRE2 is 3 bits  [15:13] -> mask 0x7  */
    cfgr &= ~((0xFU << RCC_CFGR_HPRE_POS)
            | (7U   << RCC_CFGR_PPRE1_POS)
            | (7U   << RCC_CFGR_PPRE2_POS));

    cfgr |= RCC_CFGR_HPRE_DIV1
          | RCC_CFGR_PPRE1_DIV4
          | RCC_CFGR_PPRE2_DIV2;

    RCC->CFGR = cfgr;
}


/* ============================================================================
 * Step 4 - Configure the main PLL (M, N, P, Q, source)
 *
 * RCC_PLLCFGR can only be written while the PLL is OFF - which it is here,
 * because PLLON is cleared at reset and we have not yet enabled it.
 * ============================================================================ */
static void rcc_configure_pll(void)
{
    /* Single write: build the full register value from named fields.
     * Avoids a partial-update window where the PLL could see an inconsistent
     * configuration mid-write. */
    RCC->PLLCFGR = (PLL_M_VAL << RCC_PLLCFGR_PLLM_POS)
                 | (PLL_N_VAL << RCC_PLLCFGR_PLLN_POS)
                 | (PLL_P_VAL << RCC_PLLCFGR_PLLP_POS)
                 | (PLL_Q_VAL << RCC_PLLCFGR_PLLQ_POS)
                 | RCC_PLLCFGR_PLLSRC_HSE;
}


/* ============================================================================
 * Step 5 - Enable the PLL and wait for it to lock
 *
 * After PLLON is set, the analog VCO needs ~200 us to settle onto the target
 * frequency. PLLRDY goes high when lock is achieved.
 * ============================================================================ */
static void rcc_enable_pll_and_wait(void)
{
    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {
        /* spin */
    }
}


/* ============================================================================
 * Step 6 - Switch SYSCLK source from HSI to PLL
 *
 * Up to this point, the CPU has been running on the 16 MHz HSI. The PLL is
 * locked but unused. Writing SW selects the PLL output as SYSCLK; SWS
 * confirms the switch has actually occurred (it lags SW by a few cycles).
 *
 * Immediately after SWS reads PLL, the CPU is running at 168 MHz and any
 * peripheral configured assuming 16 MHz will be wrong by a factor of 10.5x.
 * That is why this is the LAST step.
 * ============================================================================ */
static void rcc_switch_sysclk_to_pll(void)
{
    uint32_t cfgr = RCC->CFGR;

    cfgr &= ~RCC_CFGR_SW_MASK;
    cfgr |=  RCC_CFGR_SW_PLL;
    RCC->CFGR = cfgr;

    while ((RCC->CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {
        /* spin */
    }
}
