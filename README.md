# 🔬 Smart Home Monitor — BME280 Calibration Tool

Part of the **IoT Smart Home Monitor** project built on a DE10-Nano (Intel Cyclone V SoC) for the *Electronics for Embedded Systems* course at Politecnico di Torino (A.Y. 2024–2025).

This tool extracts the factory-programmed calibration coefficients from a Bosch BME280 sensor chip using an ESP32. The resulting `#define` block is pasted into the C supervisor's `smart_home.h`, enabling accurate temperature, pressure, and humidity compensation without needing direct HPS I²C access.

---

## 📋 Table of Contents

- [Why This Tool Exists](#why-this-tool-exists)
- [Contents](#contents)
- [Hardware Setup](#hardware-setup)
- [Option A: ESP-IDF Project](#option-a-esp-idf-project)
- [Option B: Arduino Sketch](#option-b-arduino-sketch)
- [Expected Output](#expected-output)
- [How to Use the Output](#how-to-use-the-output)
- [How It Works](#how-it-works)

---

## 🤔 Why This Tool Exists

In this project the BME280's SDA and SCL lines are wired directly to **FPGA GPIO pins** (JP1), not to the HPS I²C bus. The FPGA fabric reads raw ADC values from the sensor, but the **26 factory calibration coefficients** — needed by the Bosch compensation formula in the C supervisor — are only accessible over I²C.

This tool solves the problem: wire the BME280 temporarily to an ESP32, run this firmware, and copy the printed coefficients into `smart_home.h`. This only needs to be done **once per BME280 chip**. 🎉

---

## 📁 Contents

```
tools/
├── read_bme280_calib/
│   └── read_bme280_calib_esp32.ino    # 🔧 Arduino IDE sketch (ESP32 board)
└── read_bme280_calib_espidf/
    └── main/
        └── main.c                     # ✅ ESP-IDF native project (recommended)
```

Two implementations are provided — use whichever matches your toolchain.

---

## 🔌 Hardware Setup

Wire the BME280 to the ESP32 as follows:

| BME280 Pin | ESP32 Pin | Notes |
|------------|-----------|-------|
| VCC | 3.3 V | ⚠️ Do **not** use 5V |
| GND | GND | |
| SDA | GPIO 21 | Default ESP-IDF I²C SDA |
| SCL | GPIO 22 | Default ESP-IDF I²C SCL |
| SDO / ADDR | GND | Sets I²C address to **0x76** |

> 💡 If your BME280 module (e.g., Waveshare) already has pull-up resistors on SDA/SCL, no external resistors are needed.

---

## 🛠️ Option A: ESP-IDF Project (`read_bme280_calib_espidf/`)

### Prerequisites

- ESP-IDF v5.x installed and activated (`idf.py` in PATH)
- ESP32 board connected via USB

### ▶️ Build & Flash

```bash
cd tools/read_bme280_calib_espidf/
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

> Replace `/dev/ttyUSB0` with your serial port (`COM3`, etc. on Windows).

---

## 🔧 Option B: Arduino Sketch (`read_bme280_calib/`)

### Prerequisites

- Arduino IDE 2.x with **ESP32 board package** installed
- No additional libraries required (uses ESP-IDF I²C via Arduino framework)

### ▶️ Flash

1. Open `read_bme280_calib_esp32.ino` in Arduino IDE.
2. Select your ESP32 board and port.
3. Click **Upload** ⬆️.
4. Open **Serial Monitor** at **115200 baud**.

---

## 📺 Expected Output

After flashing and booting, the serial monitor will print:

```
BME280 Calibration Coefficient Reader
======================================
✅ Found BME280 at I2C address 0x76

--- Copy these lines into smart_home.h ---

#define BME280_PRECAL_T1    28267U
#define BME280_PRECAL_T2    26539
#define BME280_PRECAL_T3    50
#define BME280_PRECAL_P1    36797U
#define BME280_PRECAL_P2    -10607
#define BME280_PRECAL_P3    3024
#define BME280_PRECAL_P4    7447
#define BME280_PRECAL_P5    -15
#define BME280_PRECAL_P6    -7
#define BME280_PRECAL_P7    9900
#define BME280_PRECAL_P8    -10230
#define BME280_PRECAL_P9    4285
#define BME280_PRECAL_H1    75U
#define BME280_PRECAL_H2    370
#define BME280_PRECAL_H3    0U
#define BME280_PRECAL_H4    312
#define BME280_PRECAL_H5    50
#define BME280_PRECAL_H6    30

--- End of calibration data ---
```

---

## 📋 How to Use the Output

1. 📋 Copy the entire `#define` block from the serial monitor.
2. 📂 Open `c code/smart_home.h`.
3. ✏️ Replace the existing `BME280_PRECAL_*` definitions with your chip's values.
4. 🔨 Recompile: `make` in `c code/`.
5. 📤 Redeploy to the DE10-Nano.

> ⚠️ **Important:** Calibration coefficients are unique per chip. If you replace the BME280 with a different physical chip, you must repeat this procedure.

---

## ⚙️ How It Works

The BME280 stores 26 factory-trimmed coefficients in internal non-volatile memory (registers `0x88`–`0xA1` and `0xE1`–`0xE7`). These are used by the Bosch compensation formula (datasheet Annex 4.2.3) to convert raw 20-bit ADC readings into calibrated physical values (°C, Pa, %RH).

This firmware:
1. 🔌 Initialises the ESP32 I²C peripheral
2. 🔍 Probes for BME280 at address `0x76` (SDO=GND) or `0x77` (SDO=VCC)
3. 📖 Reads all calibration register ranges in two I²C transactions
4. 🔢 Assembles the 16-bit signed/unsigned coefficients per the datasheet register map
5. 🖨️ Prints them as ready-to-paste C preprocessor defines
