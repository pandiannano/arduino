#pragma once

#include <Adafruit_ADS1X15.h>

// =====================================================================
// Rudder Angle Indicator - hardware & filter configuration
//
// Board: ESP32-C3
// Sensor: 360-degree rotary Hall sensor, 0-5V analog output
// ADC:    ADS1115 (16-bit, I2C)
// HMI:    TJC8048X243 (Nextion-compatible instruction set) over UART
// =====================================================================

// ---------- I2C bus (ADS1115) ----------
static const int I2C_SDA_PIN = 8;   // ESP32-C3-DevKitM-1 default pins.
static const int I2C_SCL_PIN = 9;   // Check your specific board's silkscreen/pinout.

static const uint8_t ADS1115_I2C_ADDRESS = 0x48;  // ADDR pin tied to GND. 0x49/0x4A/0x4B for VDD/SDA/SCL.
static const uint8_t ADS1115_CHANNEL = 0;         // Sensor wired to AIN0 (single-ended).

// IMPORTANT hardware note:
// The ADS1115's input pins must never exceed VDD+0.3V. Feeding a full 0-5V
// signal straight into it means the ADS1115 itself must run from 5V, and its
// I2C lines then need a level shifter down to the ESP32-C3's 3.3V logic.
// The simpler, recommended wiring is a resistor divider that scales the
// sensor's 0-5V down to a safe 0-3.3V range and runs the ADS1115 from the
// ESP32-C3's own 3.3V rail (no level shifter needed at all).
// SENSOR_DIVIDER_RATIO = Vadc / Vsensor for whatever divider you build, e.g.
// a 10k (top) / 20k (bottom) divider gives 20/(10+20) = 0.667.
static const float SENSOR_DIVIDER_RATIO = 0.66f;  // 1.0f if you feed 0-5V directly (5V-powered ADS1115 + level shifter).

static const adsGain_t ADS1115_GAIN = GAIN_ONE;              // +-4.096V full scale, sized for a ~3.3V divided input.
static const uint16_t ADS1115_DATA_RATE = RATE_ADS1115_64SPS; // Lower SPS = narrower internal sinc filter = quieter but slower.

// ---------- HMI (TJC8048X243, Nextion-compatible "Usart HMI" instruction set) ----------
static const int HMI_RX_PIN = 4;   // ESP32-C3 pin wired to the HMI's TX pin.
static const int HMI_TX_PIN = 5;   // ESP32-C3 pin wired to the HMI's RX pin.
static const uint32_t HMI_BAUD = 115200;  // Must match the baud set in the TJC project / on the display itself.

// Component names as laid out in the TJC USART HMI Designer project - rename
// these to match whatever you actually name the widgets on your GUI page.
static const char *HMI_NUMBER_COMPONENT = "n0";  // Numeric field, receives angle*10 (tenths of a degree).
static const char *HMI_GAUGE_COMPONENT  = "z0";  // Rotating needle/gauge widget, receives 0-360 (degrees).
static const char *HMI_TEXT_COMPONENT   = "t0";  // Text field, receives "12.3 S" / "SENSOR FAULT" etc.

// ---------- Sampling cadence ----------
static const uint32_t SAMPLE_INTERVAL_MS = 50;          // ~20 Hz filter update rate.
static const uint32_t HMI_MIN_UPDATE_INTERVAL_MS = 100; // Don't flood the HMI's UART with updates.
static const uint32_t HMI_HEARTBEAT_MS = 1000;          // Resend even if unchanged, so the display self-heals after a glitch/reboot.

// ---------- Filter pipeline ----------
// Each stage targets a different noise source; see README.md for the full rationale.
static const uint8_t OVERSAMPLE_COUNT = 4;  // Raw ADC reads averaged together per sample (ADC-level noise filter).
static const uint8_t MEDIAN_WINDOW = 7;     // Odd window size; rejects transient spikes/outliers.
static const uint8_t MEAN_WINDOW = 10;      // Moving-average ("mean value averaging") window.
static const float EMA_ALPHA = 0.15f;       // Final low-pass smoothing; smaller = smoother but slower to respond.

// ---------- Fault detection & display stability ----------
static const float SENSOR_FAULT_LOW_V  = 0.05f;  // Below this (real sensor-side volts): broken wire / short to ground.
static const float SENSOR_FAULT_HIGH_V = 4.95f;  // Above this: open circuit / short to +5V.

static const float ANGLE_DEADBAND_DEG = 0.15f;      // Ignore changes smaller than this (pure display jitter).
static const float MAX_SLEW_DEG_PER_UPDATE = 3.0f;  // Caps how fast the displayed angle can move per SAMPLE_INTERVAL_MS.
