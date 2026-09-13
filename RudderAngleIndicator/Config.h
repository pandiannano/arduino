#pragma once

#include <Adafruit_ADS1X15.h>

// =====================================================================
// Rudder Angle Indicator - hardware & filter configuration
//
// Board:  ESP32-C3
// Sensor: resistive rudder sender, 0-190 ohm linear over 0-100 degrees,
//         wired into a voltage divider excited from 5V or 12V.
// ADC:    ADS1115 (16-bit, I2C), two channels: sender node + supply sense.
// HMI:    TJC8048X243 (Nextion-compatible instruction set) over UART
// =====================================================================

// ---------- I2C bus (ADS1115) ----------
static const int I2C_SDA_PIN = 8;   // ESP32-C3-DevKitM-1 default pins.
static const int I2C_SCL_PIN = 9;   // Check your specific board's silkscreen/pinout.

static const uint8_t ADS1115_I2C_ADDRESS = 0x48;  // ADDR pin tied to GND. 0x49/0x4A/0x4B for VDD/SDA/SCL.

// ---------- Sender wiring (resistive divider) ----------
// Circuit: Vexc --[R_FIXED]-- node --[sender 0-190R]-- GND
// The node is read on AIN0. A second tap reads the excitation supply
// itself (through an IDENTICAL divider network) on AIN1. Because both
// channels see the same attenuation, that attenuation cancels out of the
// resistance formula below - the measurement is ratiometric and doesn't
// care about the divider's exact ratio or about Vexc drifting/ripple.
// See README.md for the two recommended divider designs (5V vs 12V system).
static const uint8_t ADS1115_CHANNEL_NODE = 0;    // sender node (AIN0)
static const uint8_t ADS1115_CHANNEL_SUPPLY = 1;  // excitation supply sense (AIN1)

static const adsGain_t ADS1115_GAIN = GAIN_ONE;               // +-4.096V FS; sized for the divided (~<=3.3V) signals.
static const uint16_t ADS1115_DATA_RATE = RATE_ADS1115_64SPS; // Lower SPS = narrower internal sinc filter = quieter but slower.

// R_FIXED_OHMS is the known fixed resistor between Vexc and the sender node.
// Measure your actual resistor with a multimeter and enter that value here -
// the whole conversion's accuracy depends on how well this matches reality.
// 190 ohm (matching the sender's own full-scale value) centers the divider
// nicely and gives good sensitivity across the full 0-190R sender range.
static const float R_FIXED_OHMS = 190.0f;

// Sender's native electrical/mechanical range, from its datasheet.
static const float SENDER_MIN_OHMS = 0.0f;
static const float SENDER_MAX_OHMS = 190.0f;
static const float SENDER_FULL_SCALE_DEG = 100.0f;

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
// Each sample takes ~(2 channels x OVERSAMPLE_COUNT reads) ADS1115 conversions;
// at RATE_ADS1115_64SPS that's roughly 2*4*(1000/64) =~ 125ms, so this interval
// mostly just prevents busy-looping between conversions rather than pacing them.
static const uint32_t SAMPLE_INTERVAL_MS = 50;
static const uint32_t HMI_MIN_UPDATE_INTERVAL_MS = 100; // Don't flood the HMI's UART with updates.
static const uint32_t HMI_HEARTBEAT_MS = 1000;          // Resend even if unchanged, so the display self-heals after a glitch/reboot.

// ---------- Filter pipeline ----------
// Each stage targets a different noise source; see README.md for the full rationale.
static const uint8_t OVERSAMPLE_COUNT = 4;  // Raw ADC reads averaged together per sample (ADC-level noise filter),
                                             // reused as the window for a second rolling average across cycles.
static const uint8_t MEDIAN_WINDOW = 7;     // Odd window size; rejects transient spikes/outliers (wiper contact noise etc.).
static const uint8_t MEAN_WINDOW = 10;      // Moving-average ("mean value averaging") window.
static const float EMA_ALPHA = 0.15f;       // Final low-pass smoothing; smaller = smoother but slower to respond.

// ---------- Fault detection & display stability ----------
static const float SUPPLY_FAULT_MIN_V = 0.30f;    // Supply-sense channel below this: excitation supply missing/disconnected.
static const float FAULT_DENOM_EPSILON_V = 0.02f; // (Vsupply - Vnode) below this: sender open-circuit (node pulled to supply).

static const float SENDER_FAULT_LOW_OHMS = -5.0f;   // Comfortably below 0R; anything lower means a wiring/short fault.
static const float SENDER_FAULT_HIGH_OHMS = 210.0f; // Comfortably above 190R; allows resistor tolerance without false faults.

static const float ANGLE_DEADBAND_DEG = 0.15f;      // Ignore changes smaller than this (pure display jitter).
static const float MAX_SLEW_DEG_PER_UPDATE = 3.0f;  // Caps how fast the displayed angle can move per SAMPLE_INTERVAL_MS.
