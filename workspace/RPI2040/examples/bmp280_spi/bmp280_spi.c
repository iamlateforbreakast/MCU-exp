/* Reads temperature and pressure from a Bosch BMP280 over SPI on the RP2040.
 *
 * Wiring (default spi0 pins, matching the official Raspberry Pi Foundation
 * pico-examples/spi/bme280_spi example - BME280 uses the same SPI protocol
 * and register map as BMP280, just with extra humidity registers this
 * example doesn't touch): BMP280 SCK -> GPIO18, MOSI/SDI -> GPIO19,
 * MISO/SDO -> GPIO16, CS -> GPIO17, VCC -> 3V3, GND -> GND.
 *
 * Unlike I2C, SPI has no ACK/NACK: wrong wiring won't fail a transaction,
 * it'll just shift garbage bits. The chip-id readback is the only real way
 * to tell if the sensor is actually there - it's checked in a loud, retrying
 * loop, following the lesson learned the hard way in the bmp280_i2c example
 * (silently pressing on with bad data produced a clean-looking "0.00/0.00"
 * instead of an obvious error).
 *
 * The compensation formulas are the same Bosch fixed-point routines used in
 * bmp280_i2c.c (bus-agnostic - only the register transport differs), cross-
 * checked against a verified reference implementation of Bosch's official
 * bmp280_compensate_T_int32/bmp280_compensate_P_int64.
 */
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define SPI_SCK_PIN 18
#define SPI_MOSI_PIN 19
#define SPI_MISO_PIN 16
#define SPI_CS_PIN 17
#define SPI_BAUDRATE (500 * 1000)

#define READ_BIT 0x80

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

static inline void cs_select(void)
{
    gpio_put(SPI_CS_PIN, 0); /* active low */
}

static inline void cs_deselect(void)
{
    gpio_put(SPI_CS_PIN, 1);
}

static bool reg_write(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg & 0x7f, data}; /* clear the read bit for writes */
    cs_select();
    int ret = spi_write_blocking(SPI_PORT, buf, 2);
    cs_deselect();
    return ret == 2;
}

static bool reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
    uint8_t addr = reg | READ_BIT;
    cs_select();
    int wret = spi_write_blocking(SPI_PORT, &addr, 1);
    int rret = spi_read_blocking(SPI_PORT, 0, buf, len);
    cs_deselect();
    return wret == 1 && rret == (int)len;
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

    /* spi_init() defaults to SPI mode 0 (CPOL=0, CPHA=0), which is what the official
     * pico-examples/spi/bme280_spi example relies on without setting it explicitly -
     * Bosch's sensors in this family support mode 0. */
    spi_init(SPI_PORT, SPI_BAUDRATE);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(SPI_CS_PIN);
    gpio_set_dir(SPI_CS_PIN, GPIO_OUT);
    cs_deselect();

    reg_write(REG_RESET, SOFT_RESET_CMD);
    sleep_ms(10);

    /* Keep retrying (rather than silently pressing on with garbage data) until the
     * sensor actually responds - see the file comment on why this matters more for
     * SPI than it might seem, since SPI has no ACK/NACK to detect bad wiring. */
    uint8_t chip_id = 0;
    while (!reg_read(REG_CHIP_ID, &chip_id, 1) || chip_id != CHIP_ID_BMP280) {
        printf("BMP280 not responding over SPI (chip id read: 0x%02x, expected 0x%02x). "
               "Check wiring: SCK->GPIO18, MOSI->GPIO19, MISO->GPIO16, CS->GPIO17.\n",
               chip_id, CHIP_ID_BMP280);
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
            printf("SPI read failed\n");
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
