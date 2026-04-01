/*******************************************************************************
 * read_bme280_calib_esp32.ino
 *
 * Reads BME280 factory calibration coefficients using an ESP32 and prints
 * them ready to paste into smart_home.h.
 *
 * WIRING (temporary — disconnect from DE10-Nano first):
 *
 *   BME280 (Waveshare)    ESP32 DevKit
 *   ------------------    ------------
 *   VCC                -> 3.3V
 *   GND                -> GND
 *   SDA                -> GPIO 21  (ESP32 default SDA)
 *   SCL                -> GPIO 22  (ESP32 default SCL)
 *   ADDR / SDO         -> GND      (keeps I2C address 0x76 — same as DE10-Nano)
 *
 * The Waveshare module has built-in 4.7kΩ pull-ups, so no extra resistors.
 *
 * HOW TO USE:
 *   1. Disconnect BME280 from DE10-Nano JP1 pins.
 *   2. Wire BME280 to ESP32 as above.
 *   3. In Arduino IDE: Board = "ESP32 Dev Module" (or your specific board).
 *   4. Upload this sketch.
 *   5. Open Serial Monitor at 115200 baud.
 *   6. Copy the printed #define block.
 *   7. Paste into smart_home.h, replacing the BME280_PRECAL_* lines.
 *   8. Re-flash ESP32 with AT firmware (esptool command from earlier).
 *   9. Reconnect BME280 to DE10-Nano JP1 pins.
 *  10. Run `make` on DE10-Nano and restart ./smart_home_monitor.
 *******************************************************************************/

#include <Wire.h>

#define BME280_ADDR     0x76    /* ADDR/SDO -> GND */
#define SDA_PIN         21      /* ESP32 default */
#define SCL_PIN         22      /* ESP32 default */

/* BME280 register addresses */
#define REG_CHIP_ID     0xD0
#define REG_CALIB_T_P   0x88    /* 26 bytes: T1-T3, P1-P9, reserved, H1 */
#define REG_CALIB_H     0xE1    /* 7  bytes: H2-H6 */

/* ---- I2C helpers --------------------------------------------------------- */
static uint8_t read_reg(uint8_t reg)
{
    Wire.beginTransmission(BME280_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    Wire.requestFrom((uint8_t)BME280_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

static bool read_block(uint8_t reg, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(BME280_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((uint8_t)BME280_ADDR, len);
    for (uint8_t i = 0; i < len; i++) {
        if (!Wire.available()) return false;
        buf[i] = Wire.read();
    }
    return true;
}

/* -------------------------------------------------------------------------- */
void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(200);

    Serial.println("\n===== BME280 Calibration Reader (ESP32) =====");

    /* Check chip ID */
    uint8_t chip_id = read_reg(REG_CHIP_ID);
    if (chip_id == 0xFF) {
        Serial.println("ERROR: Cannot reach BME280 (got 0xFF).");
        Serial.println("Check wiring: SDA->GPIO21, SCL->GPIO22, VCC->3.3V, ADDR->GND.");
        return;
    }
    if (chip_id != 0x60) {
        Serial.print("ERROR: Unexpected chip ID 0x");
        Serial.print(chip_id, HEX);
        Serial.println(" (BME280 should be 0x60). Check ADDR pin and I2C address.");
        return;
    }
    Serial.println("Chip ID OK (0x60 = BME280)");

    /* Read calibration bytes */
    uint8_t calib[26] = {0};
    uint8_t hcal[7]   = {0};

    if (!read_block(REG_CALIB_T_P, calib, 26)) {
        Serial.println("ERROR: Failed to read temperature/pressure calibration block.");
        return;
    }
    delay(10);
    if (!read_block(REG_CALIB_H, hcal, 7)) {
        Serial.println("ERROR: Failed to read humidity calibration block.");
        return;
    }

    /* Decode — matches BME280 datasheet Section 4.2.2 */
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

    /* Sanity check — a blank/unconnected read gives all 0x00 or 0xFF */
    if (T1 == 0 || T1 == 0xFFFF || P1 == 0 || P1 == 0xFFFF) {
        Serial.println("WARNING: Calibration data looks wrong (all zeros or all 0xFF).");
        Serial.println("Double-check wiring and retry.");
    }

    /* Print ready-to-paste block */
    Serial.println("\n");
    Serial.println("========================================================");
    Serial.println("  COPY EVERYTHING BETWEEN THE LINES INTO smart_home.h");
    Serial.println("  Replace the existing BME280_PRECAL_* defines.");
    Serial.println("========================================================");
    Serial.println();

    Serial.print("#define BME280_PRECAL_T1    "); Serial.print(T1);  Serial.println("U");
    Serial.print("#define BME280_PRECAL_T2    "); Serial.println(T2);
    Serial.print("#define BME280_PRECAL_T3    "); Serial.println(T3);
    Serial.println();
    Serial.print("#define BME280_PRECAL_P1    "); Serial.print(P1);  Serial.println("U");
    Serial.print("#define BME280_PRECAL_P2    "); Serial.println(P2);
    Serial.print("#define BME280_PRECAL_P3    "); Serial.println(P3);
    Serial.print("#define BME280_PRECAL_P4    "); Serial.println(P4);
    Serial.print("#define BME280_PRECAL_P5    "); Serial.println(P5);
    Serial.print("#define BME280_PRECAL_P6    "); Serial.println(P6);
    Serial.print("#define BME280_PRECAL_P7    "); Serial.println(P7);
    Serial.print("#define BME280_PRECAL_P8    "); Serial.println(P8);
    Serial.print("#define BME280_PRECAL_P9    "); Serial.println(P9);
    Serial.println();
    Serial.print("#define BME280_PRECAL_H1    "); Serial.print(H1);  Serial.println("U");
    Serial.print("#define BME280_PRECAL_H2    "); Serial.println(H2);
    Serial.print("#define BME280_PRECAL_H3    "); Serial.print(H3);  Serial.println("U");
    Serial.print("#define BME280_PRECAL_H4    "); Serial.println(H4);
    Serial.print("#define BME280_PRECAL_H5    "); Serial.println(H5);
    Serial.print("#define BME280_PRECAL_H6    "); Serial.println(H6);

    Serial.println();
    Serial.println("========================================================");
    Serial.println("  END OF COPY BLOCK");
    Serial.println("========================================================");
    Serial.println();
    Serial.println("Next steps:");
    Serial.println("  1. Paste the block above into smart_home.h");
    Serial.println("  2. Re-flash ESP32 with AT firmware");
    Serial.println("  3. Reconnect BME280 to DE10-Nano JP1 pins");
    Serial.println("  4. Run `make` on DE10-Nano, then ./smart_home_monitor");
}

void loop() { /* nothing */ }
