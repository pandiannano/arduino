# Rudder Angle Indicator (ESP32-C3 + ADS1115 + TJC HMI)

Firmware for a boat rudder angle indicator:

- **MCU:** ESP32-C3
- **Sensor:** 360-degree rotary Hall-effect sensor, linear 0-5V analog output
  (only the portion of its range corresponding to your rudder's actual travel
  is used, e.g. roughly -40 to +40 degrees).
- **ADC:** ADS1115, 16-bit, I2C
- **Display:** TJC8048X243 (TJC "X2" series) HMI touchscreen, driven over UART
  with the Nextion-compatible "Usart HMI" instruction set. (This session
  could not fetch `wiki.tjc1688.com` — it's blocked by the sandbox's network
  policy — so double-check baud rate / component naming against your own
  copy of the datasheet and TJC project.)

Library dependency: **Adafruit ADS1X15** (install via Arduino Library
Manager: search "Adafruit ADS1X15").

## Wiring

### Sensor -> ADS1115

The ADS1115's inputs must stay within `VDD + 0.3V`. Feeding it a full 0-5V
signal means the ADS1115 itself has to be powered at 5V, and its I2C lines
then need a level shifter down to the ESP32-C3's 3.3V logic.

**Recommended instead:** power the ADS1115 from the ESP32-C3's 3.3V rail, and
scale the sensor's 0-5V output down with a resistor divider, e.g.:

```
Sensor OUT --[10k]--+--[20k]-- GND
                     |
                   ADS1115 AIN0   (also add a 100nF cap AIN0->GND for extra HF filtering)
```

That divider ratio is 20k/(10k+20k) = 0.667, giving a max ADC input of
5V * 0.667 = 3.33V. Set `SENSOR_DIVIDER_RATIO` in `Config.h` to match whatever
resistors you actually use — the firmware un-scales every reading by this
factor so all angles/thresholds in the code are expressed in real sensor
volts (0-5V), not ADC volts.

If you'd rather feed the sensor's 0-5V straight into the ADS1115, power the
ADS1115 from 5V, set `SENSOR_DIVIDER_RATIO = 1.0f`, change `ADS1115_GAIN` to
`GAIN_TWOTHIRDS` (±6.144V FS) in `Config.h`, and add an I2C level shifter
between the ADS1115 (5V logic) and the ESP32-C3 (3.3V logic).

**Analog low-pass filter (recommended, hardware):** put a simple RC filter
right at the sensor output, before the divider/ADC — e.g. 1k series resistor
into a 1uF capacitor to ground gives a ~160Hz cutoff, which is plenty for a
sensor that physically can't move faster than a rudder does, and knocks down
high-frequency EMI (bilge pump motors, solenoids, etc.) before it ever
reaches the ADC.

### ADS1115 -> ESP32-C3 (I2C)

| ADS1115 | ESP32-C3          |
|---------|-------------------|
| VDD     | 3.3V              |
| GND     | GND               |
| SCL     | GPIO9 (`I2C_SCL_PIN`) |
| SDA     | GPIO8 (`I2C_SDA_PIN`) |
| ADDR    | GND (-> address 0x48) |

Add 4.7k pull-ups on SDA/SCL if your ADS1115 breakout doesn't already have
them.

### ESP32-C3 -> TJC8048X243 (UART)

| ESP32-C3            | HMI |
|----------------------|-----|
| GPIO5 (`HMI_TX_PIN`) | RX  |
| GPIO4 (`HMI_RX_PIN`) | TX  |
| GND                  | GND |

The HMI needs its own 5V supply (check its current draw in the datasheet —
these displays typically pull several hundred mA, too much for the ESP32's
onboard regulator). Its UART is commonly 3.3V-logic tolerant on RX, but
verify against your datasheet; add a level shifter or a resistor divider on
the ESP32->HMI TX line if in doubt.

Set `HMI_BAUD` in `Config.h` to whatever baud you configure in the TJC
project (115200 is used as the default here).

## HMI project setup

In the TJC USART HMI Designer, create three widgets on your page and name
them to match `Config.h` (or edit `Config.h` to match names you prefer):

- `n0` - a numeric/variable component. The firmware writes `angle*10` (an
  integer, tenths of a degree) so you can show one decimal place without
  doing float math on the display itself.
- `z0` - a rotating needle/gauge widget, if your project has one. The
  firmware writes a plain 0-360 degree value (negative angles are wrapped:
  `-10` becomes `350`, matching how most gauge widgets expect their pointer
  angle).
- `t0` - a text field. The firmware writes a human-readable string like
  `"12.3 S"` (S = starboard, P = port) or `"SENSOR FAULT"`.

All communication uses the standard Nextion-style instruction format:
`component.attribute=value` followed by three `0xFF` bytes, e.g.
`t0.txt="12.3 S"` + `FF FF FF`.

## The filter pipeline

Every requirement you asked for maps to one stage in `readFilteredVoltage()`
(`RudderAngleIndicator.ino`):

1. **Analog noise filter (hardware):** the RC low-pass described above, plus
   the resistor-divider itself, which also reduces the impedance the ADC
   sees.
2. **ADC-level noise filter — oversampling:** `OVERSAMPLE_COUNT` raw ADC
   reads are taken back-to-back and averaged every sample cycle. This
   directly attacks noise at the ADC's own sample rate.
3. **ADS1115 internal filter:** the ADS1115 has a built-in sinc decimation
   filter whose bandwidth is set by its data rate (`ADS1115_DATA_RATE`).
   Lower SPS = narrower filter = quieter but slower conversions. 64 SPS is a
   good default for a rudder (nothing on a boat moves the helm at more than
   a few Hz).
4. **Software filter — median:** `MedianFilter` (`MEDIAN_WINDOW` samples)
   rejects single-sample outliers/spikes (e.g. a solenoid switching nearby)
   that survive averaging, without smearing them across neighboring
   readings the way a mean filter would.
5. **Mean value averaging:** `MovingAverageFilter` (`MEAN_WINDOW` samples)
   is the classic moving-average smoother over recent history.
6. **Software filter — EMA:** `EMAFilter` (`EMA_ALPHA`) is a first-order
   low-pass IIR filter that gives the final, steady output. Lower `alpha` =
   smoother but slower to track real movement.

On top of the filter chain, `loop()` applies:

- **Fault detection:** voltages outside `SENSOR_FAULT_LOW_V` /
  `SENSOR_FAULT_HIGH_V` are treated as a broken/shorted sensor and shown as
  `SENSOR FAULT` on the HMI instead of a bogus angle.
- **Deadband + slew limiting:** `ANGLE_DEADBAND_DEG` ignores changes too
  small to be real movement, and `MAX_SLEW_DEG_PER_UPDATE` caps how fast the
  displayed angle can move per update, so the readout is steady but never
  laggy for an actual helm movement.
- **Rate-limited HMI updates:** the display is only updated when the angle
  actually changes (subject to `HMI_MIN_UPDATE_INTERVAL_MS`), plus a
  `HMI_HEARTBEAT_MS` periodic resend so the display self-heals after a
  UART glitch or its own reboot.

All of the above are tunable constants in `Config.h` — start with the
defaults, watch the behavior on the HMI, and tighten/loosen as needed for
your boat's actual rudder speed and electrical noise environment.

## Calibration

Rather than hard-coding a single fixed voltage-to-angle formula, the
firmware stores a 3-point calibration table (port hard-over, midships,
starboard hard-over) in flash (ESP32 NVS via `Preferences`), and linearly
interpolates between points. This corrects for the sensor's own offset/gain
and for any asymmetry in your mechanical linkage — turn the wheel to each
reference position and capture it live over the Serial monitor (115200
baud):

```
CAL PORT -35.0     <- rudder physically at 35 deg to port
CAL MID 0          <- rudder physically centered
CAL STBD 35.0      <- rudder physically at 35 deg to starboard
CAL SHOW           <- prints the captured table
CAL SAVE           <- persists it to flash (survives reboot/power loss)
```

Use whatever angle convention you prefer (the example uses negative =
port, positive = starboard); the code doesn't assume anything about sign
beyond "port -> mid -> stbd" being entered in that physical order, and
works with either wiring polarity (sensor voltage rising or falling with
angle).

## Files

- `RudderAngleIndicator.ino` - setup/loop, filter pipeline, serial calibration commands.
- `Config.h` - all pins, calibration limits, and filter tuning constants.
- `Filters.h/.cpp` - `MovingAverageFilter`, `MedianFilter`, `EMAFilter`.
- `Calibration.h/.cpp` - 3-point calibration table with NVS persistence.
- `HMI.h/.cpp` - minimal Nextion/TJC "Usart HMI" protocol wrapper.
