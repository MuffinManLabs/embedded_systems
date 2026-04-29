#ifndef BME280_H_
#define BME280_H_

#include <stdint.h>

/* ---- BME280 Register Addresses ----
 * These are the full 8-bit addresses from the BME280 datasheet Table 18.
 * When sending over SPI, bit 7 is replaced by the read/write flag:
 *   bit 7 = 1 for read, bit 7 = 0 for write.
 * The SPI read/write functions will handle that bit. */

/* Chip identification — always reads 0x60 */
#define BME280_REG_ID           0xD0
#define BME280_CHIP_ID          0x60 /* Expected chip ID value */

/* Soft reset — write 0xB6 to trigger full reset */
#define BME280_REG_RESET        0xE0
#define BME280_RESET_VALUE      0xB6

/* Control and configuration */
#define BME280_REG_CTRL_HUM     0xF2    /* Humidity oversampling settings      */
#define BME280_REG_STATUS       0xF3    /* Measuring/update status flags       */
#define BME280_REG_CTRL_MEAS    0xF4    /* Temp/pressure oversampling + mode   */
#define BME280_REG_CONFIG       0xF5    /* Standby time, filter, SPI mode      */

/* data registers (read only) */
#define BME280_REG_DATA_START    0xF7    /* Start of data burst region (P/T/H, 8 bytes through 0xFE) */
#define BME280_DATA_LEN    8U    /* P_msb, P_lsb, P_xlsb, T_msb, T_lsb, T_xlsb, H_msb, H_lsb */


/* Calibration data — starting addresses for burst reads */
#define BME280_REG_CALIB00      0x88    /* calib00..calib25 (0x88 to 0xA1)     */
#define BME280_REG_CALIB26      0xE1    /* calib26..calib41 (0xE1 to 0xF0)     */




/* ------------------------------------------------------------------
 * Return status from bme280_init().
 *
 * Used to identify which stage of initialization failed so the caller
 * (main.c) can print a useful diagnostic instead of just "init failed".
 *
 * Convention: 0 means success; any non-zero value indicates a specific
 * error stage. This matches the standard pattern used by HAL_StatusTypeDef
 * and most production embedded driver libraries.
 * ------------------------------------------------------------------ */
typedef enum {
    BME280_OK                 = 0,    /* Initialization successful            */
    BME280_ERR_ID_MISMATCH,           /* Stage 1: chip ID did not return 0x60 */
    BME280_ERR_CALIB_INVALID,         /* Stage 2: calibration data looks bad  */
    BME280_ERR_CONFIG_VERIFY          /* Stage 3: register read-back mismatch */
} bme280_status_t;



/* ------------------------------------------------------------------
 * Compensated sensor reading.
 *
 * One snapshot of all three values from a single coherent BME280
 * measurement cycle. Filled by bme280_read_data().
 *
 * All three values are stored as fixed-point integers — see the unit
 * comments on each field for the conversion factor. Using fixed point
 * lets us avoid floating-point math entirely, which keeps the driver
 * portable across MCUs without an FPU and makes results bit-exact
 * reproducible (no float rounding error).
 *
 * Datasheet section 4.2.3 defines the formats produced by Bosch's
 * compensation formulas.
 * ------------------------------------------------------------------ */
typedef struct {
    int32_t  temp_x100;    /* temperature × 100,  e.g. 2345 means 23.45 °C    */
    uint32_t press_x256;   /* pressure × 256,     divide by 256 for Pa        */
    uint32_t hum_x1024;    /* humidity × 1024,    divide by 1024 for %RH      */
} bme280_data_t;





/* Initializes the BME280 sensor.
 * Verifies the chip ID, reads factory calibration coefficients into RAM,
 * and configures oversampling/mode/filter. Call once after spi2_init().
 *
 * Returns BME280_OK on success, or a specific BME280_ERR_* code identifying
 * which initialization stage failed. See bme280_status_t for details. */
bme280_status_t bme280_init(void);




/* ------------------------------------------------------------------
 * bme280_read_data — read one complete sensor sample.
 *
 * Performs a single SPI burst read of all 8 data registers
 * (0xF7…0xFE), reassembles the bytes into raw ADC values, and runs
 * the Bosch compensation formulas to produce human-readable
 * temperature, pressure, and humidity values. Results are written
 * into the caller-supplied bme280_data_t struct.
 *
 * Doing the read as a single CS-low-to-CS-high transaction is required
 * for data coherence: the BME280 freezes its data registers via shadow
 * registers only for the duration of one burst read (datasheet
 * section 4.1). Splitting the read across multiple SPI transactions
 * could mix bytes from two different measurement cycles.
 *
 * Internal call order (datasheet section 4.2.3):
 *   1. compensate_T must run first — it produces the t_fine value
 *      that the pressure and humidity formulas both consume.
 *   2. compensate_P and compensate_H run after, in any order.
 *
 * Parameters:
 *   reading - pointer to caller-owned bme280_data_t to fill in.
 *
 * Returns:
 *   BME280_OK on success. Currently no error paths are defined for
 *   this function (SPI is blocking and assumed reliable on a wired
 *   3.3 V link); the return type is kept for API consistency with
 *   bme280_init() and to leave room for future error reporting.
 * ------------------------------------------------------------------ */
bme280_status_t bme280_read_data(bme280_data_t *reading);




#endif /* BME280_H_ */
