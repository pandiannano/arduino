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
* TJC X2, 4.3" 800x480 UART touch display ([wiki.tjc1688.com](http://wiki.tjc1688.com/product/X2_serial_compare.html)) —
  TJC's "USART HMI" line, protocol-compatible with Nextion

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

HMI display setup (TJC X2, 4.3" 800x480)
-----------------------------------------
`HMI::sendAngle()` uses the standard TJC/Nextion wire protocol (ASCII
command + `0xFF 0xFF 0xFF` terminator, `page.object.attribute=value`
addressing). In your TJC project (built with their USART HMI editor),
lay out three components matching `Config.h`:

* `t0` — a Text component, showing the angle as e.g. `"12.3"`.
* `z0` — a **Gauge** component for the needle. Its `.val` (set by the
  firmware every update) is the pointer angle in degrees, 0-359 — no
  rotated-image lookup table needed, the widget draws the needle itself.
  Draw the pointer resting wherever you want "0 rudder" to read, then tune
  `HMI_GAUGE_ZERO_OFFSET_DEG` in `Config.h` until an amidships rudder
  centres the needle.
* `va1` — a hidden numeric variable used as the fault flag (0 = ok, 1 =
  fault); bind a warning icon/text's visibility to it in the editor.

Set the display's UART baud in the TJC project to match `HMI_BAUD` in
`Config.h` (default 9600 here — TJC boards are commonly shipped at 9600
but can be set much higher; either works, just keep both ends matched).

If you end up using a different widget than Gauge for the needle (e.g. a
picture rotated through a discrete frame set), or a non-Nextion-protocol
display entirely, only `HMI.cpp` needs to change — the ADC driver and
filter chain are independent of it.

Note: I wasn't able to fetch wiki.tjc1688.com directly (blocked by this
environment's network policy), so the protocol/Gauge details above are
from general TJC/Nextion documentation rather than that specific page —
worth a quick cross-check against your local TJC editor's component
reference.
