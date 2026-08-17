/* Reads temperature and pressure from a Bosch BMP280 over I2C on the RP2040.
 *
 * Wiring (default i2c0 pins): BMP280 SDA -> GPIO4, SCL -> GPIO5, VCC -> 3V3,
 * GND -> GND. Add a couple of external ~4.7k pull-ups if your breakout board
 * doesn't already have them.
 *
 * BMP280_I2C_ADDR below assumes the sensor's SDO pin is tied low (0x76);
 * many breakout boards tie SDO high instead, giving address 0x77 - check
 * your board if the chip-id read fails.
 *
 * The register map, control-register values (ctrl_meas=0x27, config=0xA0)
 * and pin choice follow the official Raspberry Pi Foundation
 * pico-examples/i2c/bmp280_i2c example. The compensation formulas are
 * Bosch's official fixed-point bmp280_compensate_T_int32/
 * bmp280_compensate_P_int64 routines (from the BMP280 datasheet /
 * BoschSensortec/BMP280_driver), reimplemented here rather than copied.
 */
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define BMP280_I2C_ADDR 0x76

#define REG_CHIP_ID 0xD0
#define REG_RESET 0xE0
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG 0xF5
#define REG_PRESSURE_MSB 0xF7
#define REG_CALIB_START 0x88

#define CHIP_ID_BMP280 0x58
#define SOFT_RESET_CMD 0xB6

typedef struct {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
} bmp280_calib_t;

/* All I2C calls check their return value - a NACK (wrong address, no
 * pull-ups, bad wiring) otherwise fails silently, leaving read buffers
 * uninitialized. That previously showed up as a clean "0.00"/"0.00" reading
 * (uninitialized stack locals happening to read back as zero) instead of a
 * clear error, which is what you actually want to see when debugging wiring. */
static bool reg_write(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_write_blocking(I2C_PORT, BMP280_I2C_ADDR, buf, 2, false) == 2;
}

static bool reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
    if (i2c_write_blocking(I2C_PORT, BMP280_I2C_ADDR, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(I2C_PORT, BMP280_I2C_ADDR, buf, len, false) == (int)len;
}

static bool bmp280_read_calibration(bmp280_calib_t *calib)
{
    uint8_t buf[24];
    if (!reg_read(REG_CALIB_START, buf, sizeof(buf))) {
        return false;
    }

    calib->dig_t1 = (uint16_t)(buf[0] | (buf[1] << 8));
    calib->dig_t2 = (int16_t)(buf[2] | (buf[3] << 8));
    calib->dig_t3 = (int16_t)(buf[4] | (buf[5] << 8));
    calib->dig_p1 = (uint16_t)(buf[6] | (buf[7] << 8));
    calib->dig_p2 = (int16_t)(buf[8] | (buf[9] << 8));
    calib->dig_p3 = (int16_t)(buf[10] | (buf[11] << 8));
    calib->dig_p4 = (int16_t)(buf[12] | (buf[13] << 8));
    calib->dig_p5 = (int16_t)(buf[14] | (buf[15] << 8));
    calib->dig_p6 = (int16_t)(buf[16] | (buf[17] << 8));
    calib->dig_p7 = (int16_t)(buf[18] | (buf[19] << 8));
    calib->dig_p8 = (int16_t)(buf[20] | (buf[21] << 8));
    calib->dig_p9 = (int16_t)(buf[22] | (buf[23] << 8));
    return true;
}

static bool bmp280_read_raw(int32_t *adc_temp, int32_t *adc_pressure)
{
    uint8_t buf[6];
    if (!reg_read(REG_PRESSURE_MSB, buf, sizeof(buf))) {
        return false;
    }

    *adc_pressure = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    *adc_temp = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    return true;
}

/* Returns temperature in hundredths of a degree C (e.g. 2534 = 25.34 C), and
 * sets *t_fine for use by bmp280_compensate_pressure. Uses "/" rather than
 * ">>" wherever the dividend's sign isn't guaranteed non-negative, to match
 * Bosch's reference formula exactly (right-shift only equals division for
 * non-negative operands; adc_t itself is always non-negative, but dig_t2/
 * dig_t3 are signed, so downstream products can go negative). */
static int32_t bmp280_compensate_temp(int32_t adc_t, const bmp280_calib_t *c, int32_t *t_fine)
{
    int32_t var1, var2;
    var1 = (((adc_t / 8) - ((int32_t)c->dig_t1 << 1)) * (int32_t)c->dig_t2) / 2048;
    var2 = (((((adc_t / 16) - (int32_t)c->dig_t1) * ((adc_t / 16) - (int32_t)c->dig_t1)) / 4096) *
            (int32_t)c->dig_t3) / 16384;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) / 256;
}

/* Returns pressure in Pa as a Q24.8 fixed-point value (divide by 256.0 for Pa).
 * Uses "/" rather than ">>" throughout to match Bosch's reference formula
 * exactly - intermediate values here aren't guaranteed non-negative, and
 * right-shift only equals division for non-negative operands. */
static uint32_t bmp280_compensate_pressure(int32_t adc_p, const bmp280_calib_t *c, int32_t t_fine)
{
    int64_t var1, var2, p;
    var1 = (int64_t)t_fine - 128000;
    var2 = var1 * var1 * (int64_t)c->dig_p6;
    var2 = var2 + ((var1 * (int64_t)c->dig_p5) * 131072LL);
    var2 = var2 + (((int64_t)c->dig_p4) * 34359738368LL);
    var1 = ((var1 * var1 * (int64_t)c->dig_p3) / 256) + ((var1 * (int64_t)c->dig_p2) * 4096LL);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)c->dig_p1) / 8589934592LL;
    if (var1 == 0) {
        return 0; /* avoid divide-by-zero */
    }
    p = 1048576 - adc_p;
    p = (((p * 2147483648LL) - var2) * 3125) / var1;
    var1 = ((int64_t)c->dig_p9 * (p / 8192) * (p / 8192)) / 33554432LL;
    var2 = ((int64_t)c->dig_p8 * p) / 524288LL;
    p = ((p + var1 + var2) / 256) + ((int64_t)c->dig_p7 * 16LL);
    return (uint32_t)p;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000); /* give a USB CDC terminal time to attach before the first prints */

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    reg_write(REG_RESET, SOFT_RESET_CMD);
    sleep_ms(10);

    /* Keep retrying (rather than silently pressing on with garbage data) until the
     * sensor actually responds - this is the one thing most likely to need fixing
     * on first wiring, so make it loud and impossible to miss instead of a single
     * easy-to-miss warning. */
    uint8_t chip_id = 0;
    while (!reg_read(REG_CHIP_ID, &chip_id, 1) || chip_id != CHIP_ID_BMP280) {
        printf("BMP280 not responding at I2C address 0x%02x (chip id read: 0x%02x, "
               "expected 0x%02x). Check wiring/pull-ups, and try address 0x77 if this "
               "board ties SDO high instead of low.\n",
               BMP280_I2C_ADDR, chip_id, CHIP_ID_BMP280);
        sleep_ms(1000);
    }
    printf("BMP280 found (chip id 0x%02x)\n", chip_id);

    bmp280_calib_t calib;
    while (!bmp280_read_calibration(&calib)) {
        printf("Failed to read calibration data, retrying...\n");
        sleep_ms(1000);
    }

    /* Normal mode, temperature/pressure oversampling x1, ~1s standby, filter off. */
    reg_write(REG_CTRL_MEAS, 0x27);
    reg_write(REG_CONFIG, 0xA0);
    sleep_ms(10); /* let the first conversion complete before reading */

    while (1) {
        int32_t adc_t, adc_p, t_fine;
        if (!bmp280_read_raw(&adc_t, &adc_p)) {
            printf("I2C read failed\n");
            sleep_ms(500);
            continue;
        }

        int32_t temp_c_x100 = bmp280_compensate_temp(adc_t, &calib, &t_fine);
        uint32_t pressure_pa_q24_8 = bmp280_compensate_pressure(adc_p, &calib, t_fine);

        printf("Temperature: %.2f C, Pressure: %.2f hPa\n",
               temp_c_x100 / 100.0, (pressure_pa_q24_8 / 256.0) / 100.0);

        sleep_ms(500);
    }
}
