/*******************************************************************************
 * read_bme280_calib.ino
 *
 * Reads the factory calibration coefficients from a BME280 sensor connected
 * to an Arduino over I2C and prints them in a format you can paste directly
 * into smart_home.h.
 *
 * Wiring (Arduino Uno / Nano):
 *   BME280 VCC  -> 3.3V  (use 3.3V, NOT 5V — BME280 is 3.3V device)
 *   BME280 GND  -> GND
 *   BME280 SDA  -> A4
 *   BME280 SCL  -> A5
 *   BME280 SDO/ADDR -> GND  (sets I2C address to 0x76, same as DE10-Nano project)
 *
 * If your BME280 module already has pull-up resistors (like the Waveshare one),
 * no external resistors are needed.
 *
 * How to use:
 *   1. Upload this sketch to your Arduino.
 *   2. Open Serial Monitor at 115200 baud.
 *   3. Copy the printed #define block.
 *   4. Paste it into smart_home.h, replacing the existing BME280_PRECAL_* lines.
 *   5. Recompile the DE10-Nano supervisor with `make`.
 *******************************************************************************/

#include <Wire.h>

#define BME280_ADDR  0x76   /* SDO -> GND */

/* BME280 register addresses for calibration data */
#define REG_CALIB_T_P   0x88   /* 26 bytes: T1..T3, P1..P9, unused, H1 */
#define REG_CALIB_H2_H6 0xE1   /* 7  bytes: H2..H6 */
#define REG_CHIP_ID     0xD0

/* ---- helpers ---- */
static uint8_t read_reg(uint8_t reg)
{
    Wire.beginTransmission(BME280_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)BME280_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

static void read_block(uint8_t reg, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(BME280_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)BME280_ADDR, len);
    for (uint8_t i = 0; i < len && Wire.available(); i++)
        buf[i] = Wire.read();
}

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    delay(100);

    Serial.println("\n===== BME280 Calibration Reader =====");

    /* Verify chip ID */
    uint8_t chip_id = read_reg(REG_CHIP_ID);
    if (chip_id != 0x60) {
        Serial.print("ERROR: Unexpected chip ID 0x");
        Serial.print(chip_id, HEX);
        Serial.println(" (expected 0x60 for BME280). Check wiring / address.");
        return;
    }
    Serial.println("Chip ID OK (0x60 = BME280)");

    /* Read calibration blocks */
    uint8_t calib[26];
    uint8_t hcal[7];
    read_block(REG_CALIB_T_P, calib, 26);
    delay(5);
    read_block(REG_CALIB_H2_H6, hcal, 7);

    /* Decode temperature coefficients */
    uint16_t T1 = (uint16_t)((calib[1]  << 8) | calib[0]);
    int16_t  T2 = (int16_t) ((calib[3]  << 8) | calib[2]);
    int16_t  T3 = (int16_t) ((calib[5]  << 8) | calib[4]);

    /* Decode pressure coefficients */
    uint16_t P1 = (uint16_t)((calib[7]  << 8) | calib[6]);
    int16_t  P2 = (int16_t) ((calib[9]  << 8) | calib[8]);
    int16_t  P3 = (int16_t) ((calib[11] << 8) | calib[10]);
    int16_t  P4 = (int16_t) ((calib[13] << 8) | calib[12]);
    int16_t  P5 = (int16_t) ((calib[15] << 8) | calib[14]);
    int16_t  P6 = (int16_t) ((calib[17] << 8) | calib[16]);
    int16_t  P7 = (int16_t) ((calib[19] << 8) | calib[18]);
    int16_t  P8 = (int16_t) ((calib[21] << 8) | calib[20]);
    int16_t  P9 = (int16_t) ((calib[23] << 8) | calib[22]);

    /* Decode humidity coefficients */
    uint8_t  H1 = calib[25];
    int16_t  H2 = (int16_t)((hcal[1] << 8) | hcal[0]);
    uint8_t  H3 = hcal[2];
    int16_t  H4 = (int16_t)((hcal[3] << 4) | (hcal[4] & 0x0F));
    int16_t  H5 = (int16_t)((hcal[5] << 4) | ((hcal[4] >> 4) & 0x0F));
    int8_t   H6 = (int8_t)hcal[6];

    /* Print the ready-to-paste block */
    Serial.println("\n--- COPY BELOW INTO smart_home.h ---\n");

    Serial.print("#define BME280_PRECAL_T1    "); Serial.print(T1); Serial.println("U");
    Serial.print("#define BME280_PRECAL_T2    "); Serial.println(T2);
    Serial.print("#define BME280_PRECAL_T3    "); Serial.println(T3);
    Serial.println();
    Serial.print("#define BME280_PRECAL_P1    "); Serial.print(P1); Serial.println("U");
    Serial.print("#define BME280_PRECAL_P2    "); Serial.println(P2);
    Serial.print("#define BME280_PRECAL_P3    "); Serial.println(P3);
    Serial.print("#define BME280_PRECAL_P4    "); Serial.println(P4);
    Serial.print("#define BME280_PRECAL_P5    "); Serial.println(P5);
    Serial.print("#define BME280_PRECAL_P6    "); Serial.println(P6);
    Serial.print("#define BME280_PRECAL_P7    "); Serial.println(P7);
    Serial.print("#define BME280_PRECAL_P8    "); Serial.println(P8);
    Serial.print("#define BME280_PRECAL_P9    "); Serial.println(P9);
    Serial.println();
    Serial.print("#define BME280_PRECAL_H1    "); Serial.print(H1); Serial.println("U");
    Serial.print("#define BME280_PRECAL_H2    "); Serial.println(H2);
    Serial.print("#define BME280_PRECAL_H3    "); Serial.print(H3); Serial.println("U");
    Serial.print("#define BME280_PRECAL_H4    "); Serial.println(H4);
    Serial.print("#define BME280_PRECAL_H5    "); Serial.println(H5);
    Serial.print("#define BME280_PRECAL_H6    "); Serial.println(H6);

    Serial.println("\n--- END OF COPY BLOCK ---");
    Serial.println("\nDone. Paste the block above into smart_home.h, then run `make` on the DE10-Nano.");
}

void loop() { /* nothing — run once in setup() */ }
