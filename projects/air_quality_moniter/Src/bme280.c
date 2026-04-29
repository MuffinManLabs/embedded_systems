#include "bme280.h"
#include "spi2.h"
#include <stdbool.h>
#include "usart2.h"

/* ------------------------------------------------------------------
 * Factory calibration coefficients
 *
 * These values are read once from the BME280's non-volatile memory
 * during bme280_init() and used by the compensation formulas for
 * every temperature, pressure, and humidity reading thereafter.
 *
 * Names match the BME280 datasheet section 4.2.2 Table 16 exactly
 * so the code can be cross-referenced directly against the datasheet.
 * Signed vs unsigned types are taken from the same table — getting
 * these wrong will produce completely incorrect readings.
 *
 * `static` here means "file scope, not visible to other .c files" —
 * nothing outside bme280.c has any business touching these directly.
 * ------------------------------------------------------------------ */

/* Temperature coefficients */
static uint16_t dig_T1;
static int16_t dig_T2;
static int16_t dig_T3;

/* Pressure coefficients */
static uint16_t dig_P1;
static int16_t dig_P2;
static int16_t dig_P3;
static int16_t dig_P4;
static int16_t dig_P5;
static int16_t dig_P6;
static int16_t dig_P7;
static int16_t dig_P8;
static int16_t dig_P9;

/* Humidity coefficients */
static uint8_t dig_H1;
static int16_t dig_H2;
static uint8_t dig_H3;
static int16_t dig_H4;
static int16_t dig_H5;
static int8_t dig_H6;

/* Fine-resolution temperature value. Produced by the temperature
 * compensation formula and consumed by both the pressure and humidity
 * compensation formulas. This is why bme280_read_temperature() MUST
 * be called before bme280_read_pressure() or bme280_read_humidity(). */
static int32_t t_fine;

/* ------------------------------------------------------------------
 * Sensor configuration values
 *
 * Bytes written to the three control registers during bme280_configure().
 * Profile is based on "indoor navigation" from BME280 datasheet
 * section 3.5.3, adjusted for a stationary indoor air-quality monitor:
 *   - Temperature oversampling ×2  (low-noise; used in P/H compensation)
 *   - Pressure oversampling   ×16  (max — pressure is the noisiest signal)
 *   - Humidity oversampling   ×1   (humidity barely benefits from OS)
 *   - Mode = normal               (continuous cycling, pairs with IIR)
 *   - t_sb = 1000 ms              (~1 Hz update rate)
 *   - IIR filter coefficient = 16 (max — suppresses door slams etc.)
 *   - 4-wire SPI                  (we are not using 3-wire mode)
 *
 * Each value is computed by packing the relevant bit fields into the
 * register layout from datasheet Tables 19, 22, and 26.
 * ------------------------------------------------------------------ */

/* config (0xF5): t_sb=101, filter=100, reserved=0, spi3w_en=0
 *   bit:  7 6 5 | 4 3 2 | 1 | 0
 *         1 0 1 | 1 0 0 | 0 | 0   = 0xB0 */
#define BME280_CONFIG_VALUE     0xB0U

/* ctrl_hum (0xF2): reserved=00000, osrs_h=001
 *   bit:  7 6 5 4 3 | 2 1 0
 *         0 0 0 0 0 | 0 0 1     = 0x01 */
#define BME280_CTRL_HUM_VALUE   0x01U

/* ctrl_meas (0xF4): osrs_t=010, osrs_p=101, mode=11
 *   bit:  7 6 5 | 4 3 2 | 1 0
 *         0 1 0 | 1 0 1 | 1 1   = 0x57 */
#define BME280_CTRL_MEAS_VALUE  0x57U

/* ============================================================
 * Forward declarations — private helpers
 * ============================================================ */
static bool bme280_verify_id(void);
static bool bme280_read_calibration(void);
static bool bme280_configure(void);
static int32_t bme280_compensate_temp(int32_t adc_T);
static uint32_t bme280_compensate_press(int32_t adc_P);
static uint32_t bme280_compensate_hum(int32_t adc_H);

/* ============================================================
 * Public API
 * ============================================================ */

/*
 * Initializes the BME280 sensor. Returns BME280_OK on success, or
 * a specific BME280_ERR_* code identifying which stage failed.
 *
 * Stage 1: Verify SPI communication via chip ID.
 * Stage 2: Read factory calibration coefficients from sensor NVM.
 * Stage 3: Configure oversampling, mode, standby time, and filter.
 */
bme280_status_t bme280_init(void) {
	/* Stage 1: Bail out immediately if the sensor doesn't respond
	 * with the expected chip ID — no point trying to read calibration
	 * or configure a sensor we can't even talk to. */
	if (!bme280_verify_id()) {
		return BME280_ERR_ID_MISMATCH;
	}

	/* Stage 2: Read factory calibration coefficients from the sensor's
	 * NVM into our static variables. Needed by every read_temperature,
	 * read_pressure, and read_humidity call. Failure here typically
	 * means a wiring/power issue (rail-stuck MISO line). */
	if (!bme280_read_calibration()) {
		return BME280_ERR_CALIB_INVALID;
	}

	/* Stage 3: Configure oversampling, mode, standby time, and IIR
	 * filter, then verify each control register read-back. */
	if (!bme280_configure()) {
		return BME280_ERR_CONFIG_VERIFY;
	}

	return BME280_OK;
}




/* ------------------------------------------------------------------
 * bme280_read_data — public API; full body in bme280.h.
 *
 * Implementation outline:
 *   1. Single SPI burst read of all 8 data registers (0xF7..0xFE).
 *      One CS-low-to-CS-high transaction guarantees the BME280's
 *      shadow registers stay coherent — no risk of mixing bytes from
 *      two different measurement cycles (datasheet section 4.1).
 *   2. Reassemble 8 raw bytes into three ADC integers:
 *        - 20-bit adc_T and adc_P  (3 bytes each, MSB:LSB:XLSB[7:4])
 *        - 16-bit adc_H            (2 bytes, MSB:LSB)
 *      Stored in int32_t per Bosch's recommendation — the compensation
 *      formulas perform signed math that can underflow if values are
 *      stored as unsigned.
 *   3. Run the three compensation helpers from datasheet section 4.2.3.
 *      compensate_temp() must run first because it produces t_fine,
 *      which the pressure and humidity formulas both consume.
 *   4. Write the three fixed-point results into the caller's struct.
 * ------------------------------------------------------------------ */
bme280_status_t bme280_read_data(bme280_data_t *reading)
{
    /* 8 raw bytes from the data registers (P, T, H — see datasheet
     * sections 5.4.7 to 5.4.9 for the per-register bit layouts).
     *
     * Buffer layout after the burst read:
     *   buf[0]: press_msb   buf[1]: press_lsb   buf[2]: press_xlsb
     *   buf[3]: temp_msb    buf[4]: temp_lsb    buf[5]: temp_xlsb
     *   buf[6]: hum_msb     buf[7]: hum_lsb
     */
    uint8_t buf[BME280_DATA_LEN];

    /* Single burst read — required for shadow-register coherence. */
    spi2_read_registers(BME280_REG_DATA_START, buf, BME280_DATA_LEN);

    /* ---- Reassemble raw 20-bit pressure and temperature, and 16-bit
     *      humidity, from the byte buffer.
     *
     * Pressure and temperature: the MSB carries bits [19:12], LSB
     * carries [11:4], and XLSB carries [3:0] in the upper nibble of
     * the byte (lower nibble is unused). Position each piece by
     * shifting and OR them together.
     *
     * Humidity: just MSB << 8 | LSB, since it is only 16 bits wide.
     *
     * Cast each byte to int32_t before shifting so the upper bits do
     * not fall off the top of a uint8_t during the shift.
     */
    int32_t adc_P = ((int32_t)buf[0] << 12) |
                    ((int32_t)buf[1] <<  4) |
                    ((int32_t)buf[2] >>  4);

    int32_t adc_T = ((int32_t)buf[3] << 12) |
                    ((int32_t)buf[4] <<  4) |
                    ((int32_t)buf[5] >>  4);

    int32_t adc_H = ((int32_t)buf[6] <<  8) |
                     (int32_t)buf[7];

    /* ---- Run compensation formulas (datasheet section 4.2.3).
     * Temperature MUST be first — it sets the file-scope t_fine
     * variable used by the pressure and humidity formulas. */
    reading->temp_x100  = bme280_compensate_temp(adc_T);
    reading->press_x256 = bme280_compensate_press(adc_P);
    reading->hum_x1024  = bme280_compensate_hum(adc_H);

    return BME280_OK;
}





/* ============================================================
 * Private helpers
 * ============================================================ */

/* ------------------------------------------------------------------
 * bme280_verify_id — Stage 1 helper
 *
 * Verifies SPI communication by reading the chip ID register (0xD0)
 * and comparing against the expected value (0x60). This is a
 * "hello world" sanity check — if this passes, we know SPI2 wiring,
 * clocks, and the four-wire protocol are all working.
 *
 * Returns true if the chip ID matches, false otherwise.
 *
 * `static` here means "file-local function, private to bme280.c".
 * ------------------------------------------------------------------ */
static bool bme280_verify_id(void) {
	/* The BME280 SPI read protocol is a two-byte exchange:
	 *   Byte 1: register address with bit 7 = 1 (read flag)
	 *   Byte 2: dummy byte — sensor sends register contents back on MISO
	 *
	 * For the chip ID at 0xD0, bit 7 is already 1, so we send 0xD0 as-is.
	 * CS must stay LOW for the entire two-byte transaction. */

	uint8_t chip_id;

	/* Step 1: Pull CS LOW to select the BME280. */
	spi2_cs_enable();

	/* Step 2: Send the register address byte.
	 * The byte the sensor returns during this exchange is garbage — ignore it. */
	(void) spi2_transmit_receive(BME280_REG_ID);

	/* Step 3: Send a dummy byte (0x00) so the SPI clock keeps ticking.
	 * The sensor uses this exchange to send back the register contents on MISO. */
	chip_id = spi2_transmit_receive(0x00U);

	/* Step 4: Pull CS HIGH to deselect the BME280 and end the transaction. */
	spi2_cs_disable();

	return (chip_id == BME280_CHIP_ID);
}

/* ------------------------------------------------------------------
 * bme280_read_calibration — Stage 2 helper
 *
 * Reads all 33 bytes of factory calibration data from the BME280's
 * NVM over SPI and parses them into the static coefficient variables
 * above. Called once by bme280_init() after the chip ID check.
 *
 * Calibration data lives in two regions of the BME280 register map:
 *   Region 1: 0x88–0xA1 (26 bytes) — dig_T1..T3, dig_P1..P9, dig_H1
 *   Region 2: 0xE1–0xE7  (7 bytes) — dig_H2..H6
 *
 * Each multi-byte coefficient is stored little-endian (low byte at
 * the lower address, high byte at the higher address). dig_H4 and
 * dig_H5 are 12-bit values packed awkwardly across shared bytes —
 * see comments in the Region 2 parsing section below.
 *
 * Returns false if dig_T1 reads back as 0x0000 or 0xFFFF, which
 * indicates a probable communication failure (MISO stuck low, MISO
 * floating high, or the sensor not powered) — real silicon never
 * has dig_T1 at either rail. Returns true otherwise.
 *
 * `static` here means "file-local function, private to bme280.c".
 * ------------------------------------------------------------------ */
static bool bme280_read_calibration(void) {
	/* Buffer large enough for the larger of the two burst reads. */
	uint8_t buf[26];

	/* ---- Region 1: 0x88–0xA1 (26 bytes) ---- */
	spi2_read_registers(BME280_REG_CALIB00, buf, 26U);

	/* Temperature coefficients (bytes 0–5)
	 * Each is 16-bit, little-endian: low byte first, then high byte.
	 * Cast high byte to 16-bit BEFORE shifting so the bits don't
	 * fall off the top of a uint8_t. */
	dig_T1 = (uint16_t) (((uint16_t) buf[1] << 8) | buf[0]);
	dig_T2 = (int16_t) (((uint16_t) buf[3] << 8) | buf[2]);
	dig_T3 = (int16_t) (((uint16_t) buf[5] << 8) | buf[4]);

	/* Pressure coefficients (bytes 6–23) */
	dig_P1 = (uint16_t) (((uint16_t) buf[7] << 8) | buf[6]);
	dig_P2 = (int16_t) (((uint16_t) buf[9] << 8) | buf[8]);
	dig_P3 = (int16_t) (((uint16_t) buf[11] << 8) | buf[10]);
	dig_P4 = (int16_t) (((uint16_t) buf[13] << 8) | buf[12]);
	dig_P5 = (int16_t) (((uint16_t) buf[15] << 8) | buf[14]);
	dig_P6 = (int16_t) (((uint16_t) buf[17] << 8) | buf[16]);
	dig_P7 = (int16_t) (((uint16_t) buf[19] << 8) | buf[18]);
	dig_P8 = (int16_t) (((uint16_t) buf[21] << 8) | buf[20]);
	dig_P9 = (int16_t) (((uint16_t) buf[23] << 8) | buf[22]);

	/* buf[24] corresponds to register 0xA0, which is a reserved/unused
	 * byte inside the calibration block. Skip it. */

	/* dig_H1 is a single byte at 0xA1 (buf[25]). */
	dig_H1 = buf[25];

	/* ---- Region 2: 0xE1–0xE7 (7 bytes) ---- */
	spi2_read_registers(BME280_REG_CALIB26, buf, 7U);

	/* dig_H2 is 16-bit signed, little-endian (bytes 0–1). */
	dig_H2 = (int16_t) (((uint16_t) buf[1] << 8) | buf[0]);

	/* dig_H3 is a single unsigned byte (byte 2). */
	dig_H3 = buf[2];

	/* dig_H4 is 12-bit signed, packed as:
	 *   dig_H4[11:4] = buf[3] (register 0xE4, full byte)
	 *   dig_H4[3:0]  = buf[4] bits [3:0] (register 0xE5 low nibble)
	 * We shift the MSB byte left by 4 and OR in the low nibble of buf[4]. */
	dig_H4 = (int16_t) (((int16_t) buf[3] << 4) | (buf[4] & 0x0FU));

	/* dig_H5 is 12-bit signed, packed as:
	 *   dig_H5[3:0]  = buf[4] bits [7:4] (register 0xE5 high nibble)
	 *   dig_H5[11:4] = buf[5] (register 0xE6, full byte)
	 * We shift the MSB byte left by 4 and OR in the high nibble of buf[4]
	 * (shifted right by 4 to line it up with bits [3:0]). */
	dig_H5 = (int16_t) (((int16_t) buf[5] << 4) | (buf[4] >> 4));

	/* dig_H6 is a single signed byte (byte 6). */
	dig_H6 = (int8_t) buf[6];

	/* Smoke test: dig_T1 is a 16-bit unsigned coefficient that on real
	 * silicon is always somewhere around 0x6900–0x7500. If it reads as
	 * 0x0000 or 0xFFFF, SPI is returning rail-stuck garbage — almost
	 * certainly a wiring or power issue, not bad calibration data. */
	if ((dig_T1 == 0x0000U) || (dig_T1 == 0xFFFFU)) {
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------
 * bme280_configure — Stage 3 helper
 *
 * Configures oversampling, measurement mode, standby time, and IIR
 * filter by writing the three control registers in the order required
 * by the BME280 datasheet:
 *   1. config    (0xF5) — t_sb, filter, spi3w_en
 *      Must be written while the sensor is in sleep mode (writes are
 *      silently ignored in normal mode per datasheet section 5.4.6).
 *      The sensor boots in sleep mode by default, so we just need to
 *      write this register before we transition into normal mode.
 *   2. ctrl_hum  (0xF2) — humidity oversampling
 *      Per datasheet section 5.4.3, writes here only take effect after
 *      a subsequent write to ctrl_meas. So we write ctrl_hum first
 *      and let the ctrl_meas write below commit it.
 *   3. ctrl_meas (0xF4) — temp/pressure oversampling and mode
 *      The mode field (bits 1:0 = 11) transitions the sensor from
 *      sleep into normal mode. After this write the sensor begins
 *      its automatic measurement/standby cycle.
 *
 * After writing, all three registers are read back and compared
 * against their expected values. This catches:
 *   - SPI wiring or signal integrity issues
 *   - Misordered writes (e.g. config attempted in normal mode)
 *   - The sensor not actually being a BME280
 * The cost is three extra reads at init time (~negligible) and the
 * safety net is real. This is standard "write-then-verify" practice
 * for bring-up code.
 *
 * Returns true if all three registers read back the expected values,
 * false on any mismatch.
 *
 * `static` here means "file-local function, private to bme280.c".
 * ------------------------------------------------------------------ */
static bool bme280_configure(void) {
	uint8_t readback;

	/* ---- Write phase ---- */
	/* Order matters: config first (sensor is still in sleep), then
	 * ctrl_hum, then ctrl_meas (which commits ctrl_hum AND activates
	 * normal mode). See function header comment for details. */
	spi2_write_register(BME280_REG_CONFIG, BME280_CONFIG_VALUE);
	spi2_write_register(BME280_REG_CTRL_HUM, BME280_CTRL_HUM_VALUE);
	spi2_write_register(BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VALUE);

	/* ---- Verify phase ---- */
	/* Read each register back and compare against the value we just
	 * wrote. Bail out on the first mismatch — there is no useful
	 * recovery, the caller (bme280_init) will simply return false
	 * to main() which will halt the boot sequence. */

	spi2_read_registers(BME280_REG_CONFIG, &readback, 1U);
	if (readback != BME280_CONFIG_VALUE) {
		return false;
	}

	spi2_read_registers(BME280_REG_CTRL_HUM, &readback, 1U);
	if (readback != BME280_CTRL_HUM_VALUE) {
		return false;
	}

	spi2_read_registers(BME280_REG_CTRL_MEAS, &readback, 1U);
	if (readback != BME280_CTRL_MEAS_VALUE) {
		return false;
	}

	return true;
}

/* ------------------------------------------------------------------
 * bme280_compensate_temp — convert raw temperature ADC value to °C × 100.
 *
 * Transcribed verbatim from BME280 datasheet section 4.2.3 (revision 1.1
 * of the formula). Uses the factory calibration coefficients dig_T1..T3
 * loaded during bme280_init().
 *
 * SIDE EFFECT: writes the file-scope variable t_fine, which is consumed
 * by bme280_compensate_press() and bme280_compensate_hum(). Therefore
 * this function MUST be called before either of those two.
 *
 * Parameters:
 *   adc_T - raw 20-bit temperature value reassembled from temp_msb,
 *           temp_lsb, temp_xlsb (datasheet section 5.4.8). Stored in
 *           int32_t per Bosch's recommendation — the formula performs
 *           signed arithmetic that can produce negative intermediates.
 *
 * Returns:
 *   Temperature in °C × 100. Resolution 0.01 °C.
 *   Example: a return of 5123 represents 51.23 °C.
 * ------------------------------------------------------------------ */
static int32_t bme280_compensate_temp(int32_t adc_T) {
	int32_t var1, var2, T;

	/* First-order calibration term */
	var1 = ((((adc_T >> 3) - ((int32_t) dig_T1 << 1))) * ((int32_t) dig_T2))
			>> 11;

	/* Second-order calibration term (compensates non-linearity) */
	var2 = (((((adc_T >> 4) - ((int32_t) dig_T1))
			* ((adc_T >> 4) - ((int32_t) dig_T1))) >> 12) * ((int32_t) dig_T3))
			>> 14;

	/* t_fine: shared with pressure and humidity compensation. */
	t_fine = var1 + var2;

	/* Final scaling: produces °C × 100 in an int32_t. */
	T = (t_fine * 5 + 128) >> 8;

	return T;
}

/* ------------------------------------------------------------------
 * bme280_compensate_press — convert raw pressure ADC value to Pa × 256.
 *
 * Transcribed verbatim from BME280 datasheet section 4.2.3, the 64-bit
 * version (BME280_compensate_P_int64). Bosch provides two pressure
 * compensation functions:
 *   - 32-bit: ~1 Pa noise floor. Inadequate when filter coefficient is
 *     high (we use 16, the maximum).
 *   - 64-bit: best accuracy. Recommended when the platform supports
 *     64-bit math, which the STM32F407 (Cortex-M4) does via compiler
 *     emulation of int64_t using two 32-bit registers.
 *
 * Uses calibration coefficients dig_P1..P9 and the t_fine value
 * produced by bme280_compensate_temp().
 *
 * PRECONDITION: bme280_compensate_temp() must have been called earlier
 * in this measurement cycle so t_fine is current.
 *
 * Parameters:
 *   adc_P - raw 20-bit pressure value reassembled from press_msb,
 *           press_lsb, press_xlsb (datasheet section 5.4.7).
 *
 * Returns:
 *   Pressure in Q24.8 fixed-point Pascals (i.e. Pa × 256).
 *   Example: a return of 24674867 represents 24674867 / 256 = 96386.2 Pa
 *   = 963.86 hPa.
 *   Returns 0 if the calculation would divide by zero (var1 underflow).
 * ------------------------------------------------------------------ */
static uint32_t bme280_compensate_press(int32_t adc_P) {
	int64_t var1, var2, p;

	var1 = ((int64_t) t_fine) - 128000;
	var2 = var1 * var1 * (int64_t) dig_P6;
	var2 = var2 + ((var1 * (int64_t) dig_P5) << 17);
	var2 = var2 + (((int64_t) dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t) dig_P3) >> 8)
			+ ((var1 * (int64_t) dig_P2) << 12);
	var1 = (((((int64_t) 1) << 47) + var1)) * ((int64_t) dig_P1) >> 33;

	/* Guard against division by zero — var1 can underflow on very
	 * unusual calibration data or invalid raw input. */
	if (var1 == 0) {
		return 0;
	}

	p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t) dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t) dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t) dig_P7) << 4);

	return (uint32_t) p;
}

/* ------------------------------------------------------------------
 * bme280_compensate_hum — convert raw humidity ADC value to %RH × 1024.
 *
 * Transcribed verbatim from BME280 datasheet section 4.2.3 (revision 1.0
 * of the formula). Uses calibration coefficients dig_H1..H6 and the
 * t_fine value produced by bme280_compensate_temp().
 *
 * PRECONDITION: bme280_compensate_temp() must have been called earlier
 * in this measurement cycle so t_fine is current.
 *
 * The formula clamps the result to the physical range [0, 100] %RH
 * before returning — Bosch handles the clamping internally so callers
 * do not need to add their own bounds check.
 *
 * Parameters:
 *   adc_H - raw 16-bit humidity value reassembled from hum_msb and
 *           hum_lsb (datasheet section 5.4.9).
 *
 * Returns:
 *   Relative humidity in Q22.10 fixed-point (i.e. %RH × 1024).
 *   Example: a return of 47445 represents 47445 / 1024 = 46.33 %RH.
 * ------------------------------------------------------------------ */
static uint32_t bme280_compensate_hum(int32_t adc_H) {
	int32_t v_x1_u32r;

	v_x1_u32r = (t_fine - ((int32_t) 76800));

	v_x1_u32r = (((((adc_H << 14) - (((int32_t) dig_H4) << 20)
			- (((int32_t) dig_H5) * v_x1_u32r)) + ((int32_t) 16384)) >> 15)
			* (((((((v_x1_u32r * ((int32_t) dig_H6)) >> 10)
					* (((v_x1_u32r * ((int32_t) dig_H3)) >> 11)
							+ ((int32_t) 32768))) >> 10) + ((int32_t) 2097152))
					* ((int32_t) dig_H2) + 8192) >> 14));

	v_x1_u32r = (v_x1_u32r
			- (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
					* ((int32_t) dig_H1)) >> 4));

	/* Clamp to [0, 100] %RH (419430400 = 100 × 4194304;
	 * after the final >>12 this becomes 100 × 1024 = 102400). */
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

	return (uint32_t) (v_x1_u32r >> 12);
}

/* ============================================================
 * Debug / bring-up (remove for production)
 * ============================================================ */

