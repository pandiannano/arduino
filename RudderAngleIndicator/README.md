Rudder Angle Indicator
======================

ESP32-C3 firmware for a rudder angle indicator: a 0-5 V Hall effect
angle sensor, read through an ADS1120 24-bit SPI ADC, filtered in several
stages, and displayed on a UART touch HMI.

Hardware
--------
* ESP32-C3
* Hall effect angle sensor, ratiometric 0-5 V output over 0-360 deg
* TI ADS1120 (SPI ADC)
* UART "HMI" touch display (Nextion-compatible protocol assumed)

Wire the Hall sensor's supply and the ADS1120's AVDD from the *same* 5 V
rail — see the comment block in `Config.h` for why (it's the first line of
noise/drift rejection, before any filtering runs).

Files
-----
* `Config.h` — all pins, calibration constants and filter tuning. Start here.
* `ADS1120.h/.cpp` — register-level SPI driver for the ADC.
* `AngleFilter.h/.cpp` — the filter chain (see below).
* `HMI.h/.cpp` — UART display driver.
* `RudderAngleIndicator.ino` — wires it all together.

The filter chain
-----------------
Five distinct noise-reduction stages, front to back:

1. **ADC-level noise rejection.** The ADS1120 is configured for its
   slowest Normal-mode data rate (20 SPS) with simultaneous 50/60 Hz
   rejection enabled. This engages the ADC's internal sinc digital filter,
   which is the most effective place to remove mains hum — it happens
   before the signal is even a discrete sample.
2. **Ratiometric reference.** The ADC's reference is tied to the same
   supply as the Hall sensor, so supply ripple/drift cancels out of the
   measurement ratio instead of appearing as angle noise.
3. **Median filter** (`MEDIAN_WINDOW`, default 5) on raw ADC codes —
   removes single-sample spikes (slip-ring/brush glitches, ESD, switching
   transients) that a linear filter would otherwise smear across several
   output samples.
4. **Mean value averaging** (`MOVING_AVERAGE_WINDOW`, default 10) — a
   circular-buffer moving average of the median-filtered angle.
5. **Exponential moving average** (`EMA_ALPHA`, default 0.15) on top, for
   final smoothing and to shape the indicator's response time.

Stages 4 and 5 both average **unit vectors** (cos/sin of the angle), not
the raw degree values — a plain arithmetic mean of angles breaks at the
0/360 wrap point (359 deg and 1 deg would average to 180 deg instead of
~0 deg). Vector averaging is correct unconditionally, so the indicator
stays accurate even if your mechanical zero point ends up near the
sensor's own 0/360 rollover.

Calibration
-----------
1. Set `CALIBRATION_MODE 1` in `Config.h`, flash, open the serial monitor.
2. Move the rudder to two known, separated angles and note the raw code
   printed at each (average a few seconds of readings by eye).
3. Enter those codes/angles as `CAL_CODE_AT_A`/`CAL_ANGLE_A` and
   `CAL_CODE_AT_B`/`CAL_ANGLE_B`.
4. Set `CALIBRATION_MODE 0`, flash again.
5. With the rudder amidships, adjust `ANGLE_ZERO_TRIM_DEG` until the
   indicator reads exactly 0.0.

Fault detection
----------------
If the raw ADC code sits near either rail (broken wire, unseated
connector) or the filtered angle lands well outside `ANGLE_MIN_DEG`/
`ANGLE_MAX_DEG`, `AngleFilter::isFault()` returns true and the HMI driver
sends a fault flag to the display instead of a plausible-looking but wrong
angle.

Adapting the HMI protocol
--------------------------
`HMI::sendAngle()` assumes a Nextion-compatible UART protocol (ASCII
command + `0xFF 0xFF 0xFF` terminator), which covers most hobbyist UART
touch displays. If your specific display uses a different protocol (e.g.
DWIN's binary VP-address frames), only `HMI.cpp` needs to change — the ADC
driver and filter chain are independent of it.
