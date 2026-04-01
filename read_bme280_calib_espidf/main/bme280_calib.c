/*******************************************************************************
 * bme280_calib.c  —  Read BME280 factory calibration via ESP-IDF I2C driver
 *
 * Reads all calibration coefficients from your specific BME280 chip and
 * prints a ready-to-paste #define block for smart_home.h.
 *
 * WIRING (temporarily disconnect BME280 from DE10-Nano JP1 first):
 *
 *   BME280 (Waveshare)    ESP32 DevKit
 *   ------------------    ----------------
 *   VCC                -> 3.3V
 *   GND                -> GND
 *   SDA                -> GPIO 21   (pin labeled "21" on the board)
 *   SCL                -> GPIO 22   (pin labeled "22" on the board)
 *   ADDR / SDO         -> GND       (I2C address stays 0x76)
 *
 * Build & flash:
 *   idf.py set-target esp32
 *   idf.py build
 *   idf.py -p COM3 flash monitor
 *
 * After reading the values:
 *   1. Copy the printed #define block into smart_home.h
 *   2. Re-flash ESP32 with AT firmware (esptool command from before)
 *   3. Reconnect BME280 to DE10-Nano JP1
 *   4. make && ./smart_home_monitor on DE10-Nano
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

/* ---- Configuration -------------------------------------------------------- */
#define I2C_MASTER_PORT     I2C_NUM_0
#define I2C_SDA_GPIO        21
#define I2C_SCL_GPIO        22
#define I2C_FREQ_HZ         100000      /* 100 kHz — safe for all BME280 modules */
#define I2C_TIMEOUT_MS      100

#define BME280_ADDR         0x76        /* ADDR/SDO -> GND */

/* ---- BME280 registers ----------------------------------------------------- */
#define REG_CHIP_ID         0xD0
#define REG_CALIB_T_P       0x88        /* 26 bytes: T1-T3, P1-P9, skip, H1 */
#define REG_CALIB_H         0xE1        /* 7  bytes: H2-H6                   */

static const char *TAG = "bme280_calib";

/* ---- I2C helpers ---------------------------------------------------------- */

static esp_err_t i2c_master_init(void)
{
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_GPIO,
        .scl_io_num       = I2C_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_DISABLE,  /* Waveshare has built-in pull-ups */
        .scl_pullup_en    = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_PORT, &cfg);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_MASTER_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

/* Write one byte (register address) then read `len` bytes back */
static esp_err_t i2c_read_reg(uint8_t reg, uint8_t *buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    /* Write register address */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd,
                                          pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) return err;

    /* Read bytes */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1)
        i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, buf + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd,
                               pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err;
}

/* ---- Main task ------------------------------------------------------------ */

static void bme280_read_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(500));   /* let UART settle */

    printf("\n\n===== BME280 Calibration Reader (ESP-IDF) =====\n\n");

    /* Init I2C */
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
        goto done;
    }
    ESP_LOGI(TAG, "I2C master init OK (SDA=GPIO%d, SCL=GPIO%d, 100kHz)",
             I2C_SDA_GPIO, I2C_SCL_GPIO);

    /* Read and verify chip ID */
    uint8_t chip_id = 0;
    err = i2c_read_reg(REG_CHIP_ID, &chip_id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot reach BME280 at 0x%02X: %s",
                 BME280_ADDR, esp_err_to_name(err));
        ESP_LOGE(TAG, "Check wiring: SDA->GPIO21, SCL->GPIO22, VCC->3.3V, ADDR->GND");
        goto done;
    }
    if (chip_id != 0x60) {
        ESP_LOGE(TAG, "Wrong chip ID: 0x%02X (expected 0x60 for BME280)", chip_id);
        ESP_LOGE(TAG, "Check ADDR/SDO pin — should be connected to GND for 0x76");
        goto done;
    }
    ESP_LOGI(TAG, "Chip ID OK: 0x%02X = BME280", chip_id);

    /* Read temperature + pressure calibration block (reg 0x88, 26 bytes) */
    uint8_t calib[26] = {0};
    err = i2c_read_reg(REG_CALIB_T_P, calib, 26);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read T/P calibration block: %s", esp_err_to_name(err));
        goto done;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    /* Read humidity calibration block (reg 0xE1, 7 bytes) */
    uint8_t hcal[7] = {0};
    err = i2c_read_reg(REG_CALIB_H, hcal, 7);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read humidity calibration block: %s", esp_err_to_name(err));
        goto done;
    }

    /* Decode — BME280 datasheet Section 4.2.2 */
    uint16_t T1 = (uint16_t)((calib[1]  << 8) | calib[0]);
    int16_t  T2 = (int16_t) ((calib[3]  << 8) | calib[2]);
    int16_t  T3 = (int16_t) ((calib[5]  << 8) | calib[4]);

    uint16_t P1 = (uint16_t)((calib[7]  << 8) | calib[6]);
    int16_t  P2 = (int16_t) ((calib[9]  << 8) | calib[8]);
    int16_t  P3 = (int16_t) ((calib[11] << 8) | calib[10]);
    int16_t  P4 = (int16_t) ((calib[13] << 8) | calib[12]);
    int16_t  P5 = (int16_t) ((calib[15] << 8) | calib[14]);
    int16_t  P6 = (int16_t) ((calib[17] << 8) | calib[16]);
    int16_t  P7 = (int16_t) ((calib[19] << 8) | calib[18]);
    int16_t  P8 = (int16_t) ((calib[21] << 8) | calib[20]);
    int16_t  P9 = (int16_t) ((calib[23] << 8) | calib[22]);

    uint8_t  H1 = calib[25];
    int16_t  H2 = (int16_t)((hcal[1] << 8) | hcal[0]);
    uint8_t  H3 = hcal[2];
    int16_t  H4 = (int16_t)((hcal[3] << 4) | (hcal[4] & 0x0F));
    int16_t  H5 = (int16_t)((hcal[5] << 4) | ((hcal[4] >> 4) & 0x0F));
    int8_t   H6 = (int8_t)hcal[6];

    /* Sanity check */
    if (T1 == 0 || T1 == 0xFFFF || P1 == 0 || P1 == 0xFFFF) {
        ESP_LOGW(TAG, "Calibration data looks invalid (all 0x00 or 0xFF).");
        ESP_LOGW(TAG, "Double-check wiring and try again.");
    }

    /* Print ready-to-paste block — use plain printf so it appears cleanly
     * in the monitor without ESP log timestamps/tags in the way */
    printf("\n");
    printf("================================================================\n");
    printf("  COPY EVERYTHING BELOW INTO smart_home.h\n");
    printf("  Replace the existing BME280_PRECAL_* defines.\n");
    printf("================================================================\n");
    printf("\n");
    printf("#define BME280_PRECAL_T1    %uU\n",  T1);
    printf("#define BME280_PRECAL_T2    %d\n",   T2);
    printf("#define BME280_PRECAL_T3    %d\n",   T3);
    printf("\n");
    printf("#define BME280_PRECAL_P1    %uU\n",  P1);
    printf("#define BME280_PRECAL_P2    %d\n",   P2);
    printf("#define BME280_PRECAL_P3    %d\n",   P3);
    printf("#define BME280_PRECAL_P4    %d\n",   P4);
    printf("#define BME280_PRECAL_P5    %d\n",   P5);
    printf("#define BME280_PRECAL_P6    %d\n",   P6);
    printf("#define BME280_PRECAL_P7    %d\n",   P7);
    printf("#define BME280_PRECAL_P8    %d\n",   P8);
    printf("#define BME280_PRECAL_P9    %d\n",   P9);
    printf("\n");
    printf("#define BME280_PRECAL_H1    %uU\n",  (unsigned)H1);
    printf("#define BME280_PRECAL_H2    %d\n",   H2);
    printf("#define BME280_PRECAL_H3    %uU\n",  (unsigned)H3);
    printf("#define BME280_PRECAL_H4    %d\n",   H4);
    printf("#define BME280_PRECAL_H5    %d\n",   H5);
    printf("#define BME280_PRECAL_H6    %d\n",   (int)H6);
    printf("\n");
    printf("================================================================\n");
    printf("  END OF COPY BLOCK\n");
    printf("================================================================\n");
    printf("\n");
    printf("Next steps:\n");
    printf("  1. Paste the block above into smart_home.h\n");
    printf("  2. Re-flash ESP32 with AT firmware (esptool command from before)\n");
    printf("  3. Reconnect BME280 to DE10-Nano JP1 pins\n");
    printf("  4. On DE10-Nano:  make  then  ./smart_home_monitor\n");
    printf("\n");

done:
    i2c_driver_delete(I2C_MASTER_PORT);
    ESP_LOGI(TAG, "Done. You can now close the monitor (Ctrl+])");
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(bme280_read_task, "bme280_calib", 4096, NULL, 5, NULL);
}
