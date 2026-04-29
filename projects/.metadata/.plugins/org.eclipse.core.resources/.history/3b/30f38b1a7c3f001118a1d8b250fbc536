#include "bme280.h"
#include "spi2.h"
#include <stddef.h>   /* for NULL */



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
static int16_t  dig_T2;
static int16_t  dig_T3;

/* Pressure coefficients */
static uint16_t dig_P1;
static int16_t  dig_P2;
static int16_t  dig_P3;
static int16_t  dig_P4;
static int16_t  dig_P5;
static int16_t  dig_P6;
static int16_t  dig_P7;
static int16_t  dig_P8;
static int16_t  dig_P9;

/* Humidity coefficients */
static uint8_t  dig_H1;
static int16_t  dig_H2;
static uint8_t  dig_H3;
static int16_t  dig_H4;
static int16_t  dig_H5;
static int8_t   dig_H6;

/* Fine-resolution temperature value. Produced by the temperature
 * compensation formula and consumed by both the pressure and humidity
 * compensation formulas. This is why bme280_read_temperature() MUST
 * be called before bme280_read_pressure() or bme280_read_humidity(). */
static int32_t t_fine;


/* ------------------------------------------------------------------
 * bme280_read_calibration — private helper
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
 * `static` here means "file-local function, private to bme280.c" —
 * this helper is not part of the public API.
 * ------------------------------------------------------------------ */
static void bme280_read_calibration(void)
{
    /* Buffer large enough for the larger of the two burst reads. */
    uint8_t buf[26];

    /* ---- Region 1: 0x88–0xA1 (26 bytes) ---- */
    spi2_read_registers(BME280_REG_CALIB00, buf, 26U);

    /* Temperature coefficients (bytes 0–5)
     * Each is 16-bit, little-endian: low byte first, then high byte.
     * Cast high byte to 16-bit BEFORE shifting so the bits don't
     * fall off the top of a uint8_t. */
    dig_T1 = (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    dig_T2 =  (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    dig_T3 =  (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);

    /* Pressure coefficients (bytes 6–23) */
    dig_P1 = (uint16_t)(((uint16_t)buf[7]  << 8) | buf[6]);
    dig_P2 =  (int16_t)(((uint16_t)buf[9]  << 8) | buf[8]);
    dig_P3 =  (int16_t)(((uint16_t)buf[11] << 8) | buf[10]);
    dig_P4 =  (int16_t)(((uint16_t)buf[13] << 8) | buf[12]);
    dig_P5 =  (int16_t)(((uint16_t)buf[15] << 8) | buf[14]);
    dig_P6 =  (int16_t)(((uint16_t)buf[17] << 8) | buf[16]);
    dig_P7 =  (int16_t)(((uint16_t)buf[19] << 8) | buf[18]);
    dig_P8 =  (int16_t)(((uint16_t)buf[21] << 8) | buf[20]);
    dig_P9 =  (int16_t)(((uint16_t)buf[23] << 8) | buf[22]);

    /* buf[24] corresponds to register 0xA0, which is a reserved/unused
     * byte inside the calibration block. Skip it. */

    /* dig_H1 is a single byte at 0xA1 (buf[25]). */
    dig_H1 = buf[25];

    /* ---- Region 2: 0xE1–0xE7 (7 bytes) ---- */
    spi2_read_registers(BME280_REG_CALIB26, buf, 7U);

    /* dig_H2 is 16-bit signed, little-endian (bytes 0–1). */
    dig_H2 = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);

    /* dig_H3 is a single unsigned byte (byte 2). */
    dig_H3 = buf[2];

    /* dig_H4 is 12-bit signed, packed as:
     *   dig_H4[11:4] = buf[3] (register 0xE4, full byte)
     *   dig_H4[3:0]  = buf[4] bits [3:0] (register 0xE5 low nibble)
     * We shift the MSB byte left by 4 and OR in the low nibble of buf[4]. */
    dig_H4 = (int16_t)(((int16_t)buf[3] << 4) | (buf[4] & 0x0FU));

    /* dig_H5 is 12-bit signed, packed as:
     *   dig_H5[3:0]  = buf[4] bits [7:4] (register 0xE5 high nibble)
     *   dig_H5[11:4] = buf[5] (register 0xE6, full byte)
     * We shift the MSB byte left by 4 and OR in the high nibble of buf[4]
     * (shifted right by 4 to line it up with bits [3:0]). */
    dig_H5 = (int16_t)(((int16_t)buf[5] << 4) | (buf[4] >> 4));

    /* dig_H6 is a single signed byte (byte 6). */
    dig_H6 = (int8_t)buf[6];
}






/*
 * Initializes the BME280 sensor.
 *
 * STAGE 1 (current): Verifies SPI communication by reading the chip ID
 * register (0xD0) and comparing against the expected value (0x60).
 * This is a "hello world" sanity check — if this passes, we know SPI2
 * wiring, clocks, and the four-wire protocol are all working.
 *
 * STAGE 2 (not yet implemented): Read factory calibration coefficients.
 * STAGE 3 (not yet implemented): Configure oversampling and mode.
 */
bool bme280_init(void)
{
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
    (void)spi2_transmit_receive(BME280_REG_ID);

    /* Step 3: Send a dummy byte (0x00) so the SPI clock keeps ticking.
     * The sensor uses this exchange to send back the register contents on MISO. */
    chip_id = spi2_transmit_receive(0x00);

    /* Step 4: Pull CS HIGH to deselect the BME280 and end the transaction. */
    spi2_cs_disable();

    /* Step 5: Compare the chip ID against the expected value (0x60).
     * Return true on match, false on mismatch. */
    /* If chip ID doesn't match, abort — SPI communication is broken
         * or we're not talking to a BME280. */
        if (chip_id != BME280_CHIP_ID)
        {
            return false;
        }

        /* Chip ID is good. Read factory calibration coefficients from
         * the sensor's NVM into our static variables. These are needed
         * by every read_temperature/pressure/humidity call. */
        bme280_read_calibration();

        /* (Stage 3 — sensor configuration — will go here next.) */

        return true;
}
