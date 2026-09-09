#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Wiring — EDIT THESE to match your actual PCB / breadboard layout.
// Any free GPIO on the ESP32-C3 works; avoid the strapping pins (2, 8, 9)
// and the pins used by USB (18/19 on some boards).
// ---------------------------------------------------------------------------

// ADS1120 (SPI mode 1, CPOL=0/CPHA=1, max ~4 MHz — we use 1 MHz for margin)
#define PIN_SPI_SCK   4
#define PIN_SPI_MISO  5
#define PIN_SPI_MOSI  6
#define PIN_ADS_CS    7
#define PIN_ADS_DRDY  10   // ADS1120 DRDY, active LOW when a new conversion is ready

// HMI display, driven on the ESP32-C3's second UART (Serial1).
// Serial (UART0 / USB-CDC) is left free for debug prints on the console.
#define PIN_HMI_RX    20
#define PIN_HMI_TX    21
#define HMI_BAUD      9600   // must match the baud rate configured in the HMI project

// ---------------------------------------------------------------------------
// ADS1120 analog front end
// ---------------------------------------------------------------------------
// The Hall sensor is wired AIN0 -> signal, AVSS -> sensor ground, and the
// ADS1120's own AVDD is fed from the SAME 5 V rail that powers the Hall
// sensor. The ADC's reference is then configured to be that same AVDD
// supply (ratiometric measurement) instead of the internal 2.048 V
// reference. This is deliberate noise/drift rejection: any sag or ripple on
// the shared 5 V rail shifts the sensor's output and the ADC's reference by
// the same fraction, so it cancels out of the ratio instead of showing up
// as angle error. If you instead give the ADS1120 its own regulated
// reference, change ADS1120_VREF_SOURCE in ADS1120.cpp.

// ---------------------------------------------------------------------------
// Sensor calibration
// ---------------------------------------------------------------------------
// Two-point calibration: raw ADS1120 code (24-bit signed) at two known,
// physically-measured rudder angles. Build with CALIBRATION_MODE = 1 first,
// swing the rudder to two known angles (e.g. full port / full starboard, or
// use a protractor on the bench), and note the printed raw codes.
#define CALIBRATION_MODE 0   // 1 = print raw ADC codes over USB serial instead of driving the HMI

#define CAL_CODE_AT_A     0        // raw code measured at CAL_ANGLE_A
#define CAL_ANGLE_A       -35.0f   // degrees, e.g. full port

#define CAL_CODE_AT_B     0        // raw code measured at CAL_ANGLE_B
#define CAL_ANGLE_B       35.0f    // degrees, e.g. full starboard

// Mechanical zero trim, applied after the two-point calibration above.
// Bump this until the indicator reads exactly 0.0 with the rudder amidships.
#define ANGLE_ZERO_TRIM_DEG   0.0f

// Physically-plausible display range. Used only to flag an out-of-range
// fault (broken linkage, sensor unseated, bad calibration) — the filters
// themselves work over the full 0-360 degree circle so they stay correct
// even if your mechanical zero sits close to the sensor's own 0/360 wrap.
#define ANGLE_MIN_DEG   -45.0f
#define ANGLE_MAX_DEG    45.0f

// ---------------------------------------------------------------------------
// Filter tuning
// ---------------------------------------------------------------------------
// 1) Median filter over the last N raw ADC codes — kills single-sample
//    spikes (ESD, switching noise, slip-ring/brush contact glitches) before
//    they ever reach the averaging stages. Must be odd.
#define MEDIAN_WINDOW  5

// 2) Mean value averaging — circular buffer of the last N median-filtered
//    samples, averaged as vectors (see AngleFilter.cpp) so the average stays
//    correct even across the 0/360 wrap.
#define MOVING_AVERAGE_WINDOW  10

// 3) Exponential moving average applied on top, for final smoothing /
//    response-time shaping. 0 < alpha <= 1; smaller = smoother but slower.
#define EMA_ALPHA  0.15f

// Sensor fault detection: ADS1120 code is a 24-bit signed value, so full
// scale is +/-8388607. If the reading sits within this many counts of
// either rail, treat it as an open/shorted sensor lead rather than a real
// angle.
#define ADC_FULLSCALE_FAULT_MARGIN  20000L

// ---------------------------------------------------------------------------
// HMI update pacing
// ---------------------------------------------------------------------------
// Rate-limit writes to the display: the ADS1120 itself is sampled at ~20 SPS
// (see ADS1120.cpp), but the HMI does not need — and should not receive —
// an update every single sample. Coalescing updates avoids flooding the UART
// link and avoids visible flicker/redraw stutter on the HMI.
#define HMI_MIN_UPDATE_INTERVAL_MS  100   // 10 Hz max
#define HMI_MIN_UPDATE_DELTA_DEG    0.1f  // ignore changes smaller than this

// Component names as configured in the HMI project. The command syntax
// below assumes a Nextion-compatible UART protocol (text command + 0xFF
// 0xFF 0xFF terminator), which is what the great majority of "UART HMI"
// touch displays use (Nextion itself, and most Nextion-compatible clones).
// If your specific display uses a different protocol (e.g. DWIN's binary
// VP-address frames), replace the body of HMI::sendAngle() — the rest of
// the firmware (ADC + filtering) does not need to change.
#define HMI_TEXT_COMPONENT   "t0"    // text field showing e.g. "12.3"
#define HMI_VALUE_COMPONENT  "va0"   // numeric variable, e.g. driving a rotated needle
#define HMI_FAULT_COMPONENT  "va1"   // 0 = ok, 1 = fault

// Debug printing on the USB serial console.
#define DEBUG_SERIAL  1
