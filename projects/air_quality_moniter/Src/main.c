#include "stm32f407xx.h"
#include "usart2.h"
#include "spi2.h"
#include "bme280.h"
#include "rcc.h"

/*
 * Crude busy-wait delay of approximately 1 second.
 *
 * The CPU is currently running on the 16 MHz HSI (no PLL configured yet).
 * An empty volatile loop takes roughly 6–10 cycles per iteration depending
 * on compiler optimization level, so ~2 million iterations approximates
 * one second. Exact timing is not critical here — we just need to read
 * the BME280 at roughly the rate it produces fresh data (t_sb = 1000 ms).
 *
 * The `volatile` qualifier on the loop counter prevents the compiler
 * from optimizing the entire loop away.
 *
 * TODO: replace with SysTick-based delay once that driver exists.
 */
static void delay_approx_1s(void) {
	for (volatile uint32_t i = 0U; i < 2000000U; i++) {
		/* intentionally empty */
	}
}

int main(void) {
	/* Step 0: Bring SYSCLK up to 168 MHz before initializing any
	 * peripheral whose timing depends on bus clocks. Must run first. */
	rcc_init_clock_168mhz();

	/* Step 1: Initialize the debug serial port (USART2) first.
	 * We need this up and running before anything else so we can print
	 * status messages about the rest of the startup sequence. */
	usart2_init();
	usart2_send_string("\r\n--- Air Quality Monitor starting ---\r\n");

	/* Step 2: Initialize the SPI2 peripheral so we can talk to the BME280. */
	usart2_send_string("Initializing SPI2... ");
	spi2_init();
	usart2_send_string("OK\r\n");

	/* Step 3: Run full BME280 init (chip ID check + calibration read +
	 * configure control registers). The status code tells us exactly
	 * which stage failed if anything goes wrong. */
	usart2_send_string("BME280 init... ");
	bme280_status_t bme_status = bme280_init();

	switch (bme_status) {
	case BME280_OK:
		usart2_send_string("OK\r\n");
		break;

	case BME280_ERR_ID_MISMATCH:
		usart2_send_string("FAIL: chip ID mismatch (Stage 1)\r\n");
		usart2_send_string("  Check SPI wiring, CS pin, and 3.3V power.\r\n");
		while (1) { /* halt */
		}

	case BME280_ERR_CALIB_INVALID:
		usart2_send_string("FAIL: calibration invalid (Stage 2)\r\n");
		usart2_send_string(
				"  dig_T1 was 0x0000 or 0xFFFF - likely MISO stuck.\r\n");
		while (1) { /* halt */
		}

	case BME280_ERR_CONFIG_VERIFY:
		usart2_send_string("FAIL: config register verify (Stage 3)\r\n");
		usart2_send_string(
				"  A control register did not read back the value written.\r\n");
		while (1) { /* halt */
		}

	default:
		/* Defensive: if the enum gains new values later and we
		 * forget to update this switch, we still halt loudly. */
		usart2_send_string("FAIL: unknown status code\r\n");
		while (1) { /* halt */
		}
	}

	/* Infinite loop — bare-metal main() must never return.
	 *
	 * Each iteration: read a coherent T/P/H sample from the BME280
	 * (one SPI burst read, see bme280_read_data), then print it
	 * over USART2 in human-readable units, then wait ~1 second
	 * before the next read. */
	bme280_data_t reading;

	while (1) {
		/* ---- Read one coherent sample from the BME280. ---- */
		(void) bme280_read_data(&reading); /* Always returns BME280_OK currently */

		/* ---- Temperature: temp_x100 holds (°C × 100). ----
		 * Split into whole and fractional parts. The fractional part
		 * needs zero-padding to 2 digits so values like 0.05 don't print
		 * as "0.5". For negative temperatures, the % operator in C99
		 * preserves the sign on the remainder, so we take its absolute
		 * value before printing. */
		int32_t t_whole = reading.temp_x100 / 100;
		int32_t t_frac = reading.temp_x100 % 100;
		if (t_frac < 0) {
			t_frac = -t_frac;
		}

		usart2_send_string("T: ");
		usart2_send_int(t_whole);
		usart2_send_byte('.');
		if (t_frac < 10) {
			usart2_send_byte('0');
		} /* zero-pad */
		usart2_send_int(t_frac);
		usart2_send_string(" C   ");

		/* ---- Pressure: press_x256 holds (Pa × 256), i.e. Q24.8. ----
		 * First divide by 256 to get whole Pascals, then split into
		 * hPa whole and fractional parts (1 hPa = 100 Pa). */
		uint32_t p_pa = reading.press_x256 / 256U;
		uint32_t hpa_whole = p_pa / 100U;
		uint32_t hpa_frac = p_pa % 100U;

		usart2_send_string("P: ");
		usart2_send_int((int32_t) hpa_whole);
		usart2_send_byte('.');
		if (hpa_frac < 10U) {
			usart2_send_byte('0');
		}
		usart2_send_int((int32_t) hpa_frac);
		usart2_send_string(" hPa   ");

		/* ---- Humidity: hum_x1024 holds (%RH × 1024), i.e. Q22.10. ----
		 * Whole part = hum_x1024 / 1024.
		 * Fractional part scaled to 2 decimal digits = (hum_x1024 % 1024) * 100 / 1024. */
		uint32_t h_whole = reading.hum_x1024 / 1024U;
		uint32_t h_frac = ((reading.hum_x1024 % 1024U) * 100U) / 1024U;

		usart2_send_string("H: ");
		usart2_send_int((int32_t) h_whole);
		usart2_send_byte('.');
		if (h_frac < 10U) {
			usart2_send_byte('0');
		}
		usart2_send_int((int32_t) h_frac);
		usart2_send_string(" %RH\r\n");

		/* Wait ~1 second to roughly match the BME280's t_sb cycle. */
		delay_approx_1s();
	}
}
