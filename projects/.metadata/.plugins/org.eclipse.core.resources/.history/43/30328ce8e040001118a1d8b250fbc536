#ifndef BME280_H_
#define BME280_H_

#include <stdint.h>
#include <stdbool.h>

/* ---- BME280 Register Addresses ----
 * These are the full 8-bit addresses from the BME280 datasheet Table 18.
 * When sending over SPI, bit 7 is replaced by the read/write flag:
 *   bit 7 = 1 for read, bit 7 = 0 for write.
 * The SPI read/write functions will handle that bit. */

/* Chip identification — always reads 0x60 */
#define BME280_REG_ID           0xD0

/* Soft reset — write 0xB6 to trigger full reset */
#define BME280_REG_RESET        0xE0
#define BME280_RESET_VALUE      0xB6

/* Control and configuration */
#define BME280_REG_CTRL_HUM     0xF2    /* Humidity oversampling settings      */
#define BME280_REG_STATUS       0xF3    /* Measuring/update status flags       */
#define BME280_REG_CTRL_MEAS    0xF4    /* Temp/pressure oversampling + mode   */
#define BME280_REG_CONFIG       0xF5    /* Standby time, filter, SPI mode      */

/* Raw data output (read only) */
#define BME280_REG_PRESS_MSB    0xF7    /* Pressure bits [19:12]               */
#define BME280_REG_PRESS_LSB    0xF8    /* Pressure bits [11:4]                */
#define BME280_REG_PRESS_XLSB   0xF9    /* Pressure bits [3:0]                 */
#define BME280_REG_TEMP_MSB     0xFA    /* Temperature bits [19:12]            */
#define BME280_REG_TEMP_LSB     0xFB    /* Temperature bits [11:4]             */
#define BME280_REG_TEMP_XLSB    0xFC    /* Temperature bits [3:0]              */
#define BME280_REG_HUM_MSB      0xFD    /* Humidity bits [15:8]                */
#define BME280_REG_HUM_LSB      0xFE    /* Humidity bits [7:0]                 */

/* Calibration data — starting addresses for burst reads */
#define BME280_REG_CALIB00      0x88    /* calib00..calib25 (0x88 to 0xA1)     */
#define BME280_REG_CALIB26      0xE1    /* calib26..calib41 (0xE1 to 0xF0)     */

/* Expected chip ID value */
#define BME280_CHIP_ID          0x60



/* Initializes the BME280 sensor.
 * Reads the chip ID register (0xD0) and verifies it returns 0x60 to
 * confirm SPI wiring and communication. Then reads the factory calibration
 * coefficients from the sensor and stores them in RAM for later use by the
 * compensation formulas. Finally configures oversampling and mode with
 * sensible defaults (see bme280.c).
 * Call this once after spi2_init().
 * Returns true on success, false if the chip ID doesn't match. */
bool bme280_init(void);


/* Reads temperature from the BME280 data registers, applies the
 * compensation formula using factory calibration coefficients,
 * and returns the temperature in degrees Celsius multiplied by 100.
 * Example: a return value of 2350 means 23.50°C. */
int32_t bme280_read_temperature(void);


/* Reads pressure from the BME280 data registers, applies the
 * compensation formula using factory calibration coefficients,
 * and returns the pressure in Pascals in Q24.8 fixed-point format
 * (24 integer bits, 8 fractional bits — i.e. the value is Pa * 256).
 * Divide by 256 to get Pascals. Divide by 25600 to get hectopascals (hPa).
 * Example: a return value of 24674867 means 24674867 / 256 = 96386.2 Pa
 * = 963.86 hPa.
 *
 * NOTE: bme280_read_temperature() must be called before this function,
 * because the temperature compensation produces an intermediate value
 * (t_fine) that the pressure formula depends on. */
uint32_t bme280_read_pressure(void);


/* Reads humidity from the BME280 data registers, applies the
 * compensation formula using factory calibration coefficients,
 * and returns the relative humidity in Q22.10 fixed-point format
 * (22 integer bits, 10 fractional bits — i.e. the value is %RH * 1024).
 * Divide by 1024 to get percent relative humidity.
 * Example: a return value of 47445 means 47445 / 1024 = 46.33 %RH.
 *
 * NOTE: bme280_read_temperature() must be called before this function,
 * because the temperature compensation produces an intermediate value
 * (t_fine) that the humidity formula depends on. */
uint32_t bme280_read_humidity(void);


#endif /* BME280_H_ */
