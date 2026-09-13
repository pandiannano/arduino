# Rudder Angle Indicator (ESP32-C3 + ADS1115 + TJC HMI)

Firmware for a boat rudder angle indicator:

- **MCU:** ESP32-C3
- **Sensor:** resistive rudder sender, a variable resistor of 0-190 ohm
  linear over 0-100 degrees of rotation, excited from a 5V or 12V supply
  (both are supported; see the two divider options below).
- **ADC:** ADS1115, 16-bit, I2C, two channels used.
- **Display:** TJC8048X243 (TJC "X2" series) HMI touchscreen, driven over UART
  with the Nextion-compatible "Usart HMI" instruction set. (This session
  could not fetch `wiki.tjc1688.com` — it's blocked by the sandbox's network
  policy — so double-check baud rate / component naming against your own
  copy of the datasheet and TJC project.)

Library dependency: **Adafruit ADS1X15** (install via Arduino Library
Manager: search "Adafruit ADS1X15").

## Why two ADC channels

A 0-190 ohm sender isn't a voltage source — it has to be turned into a
voltage with a resistor divider before an ADC can read it:

```
Vexc --[ R_FIXED ]-- node --[ sender 0-190R ]-- GND
```

`R_FIXED` is a resistor you add, from the excitation supply (Vexc, 5V or
12V) down to the sender. As the sender's resistance changes with rudder
angle, the voltage at `node` changes too:

```
Vnode = Vexc * R_sender / (R_FIXED + R_sender)
   =>  R_sender = R_FIXED * Vnode / (Vexc - Vnode)
```

The problem: that formula needs to know `Vexc` accurately, and on a boat
`Vexc` is never rock-solid — it sags under engine-start load, drifts with
battery charge state, has alternator ripple, etc. Any error in `Vexc`
becomes a direct error in the computed angle.

The fix used here: read `Vexc` itself on a **second** ADC channel, through
an **identical** divider network (see the two wiring options below), every
sample cycle, right alongside the sender node. Both channels then see the
exact same attenuation, so it cancels out algebraically and the sender
resistance formula only needs the *ratio* of the two readings:

```
R_sender = R_FIXED * Vnode_adc / (Vsupply_adc - Vnode_adc)
```

This is a **ratiometric** measurement — it self-corrects for supply
drift/ripple and for the divider resistors' own tolerance, as long as both
dividers are built the same way. It's the reason this design uses two
channels instead of one.

## Wiring

### Sender -> ADS1115 (choose one)

The ADS1115's inputs must never exceed `VDD + 0.3V`, and under a sender
**open-circuit fault** the node voltage rises all the way to `Vexc` — so
the divider must be sized to survive that fault case safely, not just the
normal 0-190 ohm range.

**Option A — 5V excitation (simpler):**

```
5V --[ R_FIXED = 190R ]-- NODE --[ sender ]-- GND      -> ADS1115 AIN0
5V --[ 10k ]-- SUPPLY_TAP --[ 10k ]-- GND               -> ADS1115 AIN1
```

Max node voltage under fault = 5V, safely under `GAIN_ONE`'s ±4.096V. The
supply tap divides 5V down to 2.5V with the same style of divider so both
channels are on comparable footing. Power the ADS1115 from the ESP32-C3's
3.3V rail — wait, 5V into a 3.3V-powered ADS1115 still violates
`VDD+0.3V`, so for Option A power the **ADS1115 from 5V** and add an I2C
level shifter (3.3V<->5V) between it and the ESP32-C3. If you'd rather
avoid a level shifter, use Option B instead (it keeps everything at 3.3V).

**Option B — 12V excitation, or 5V without a level shifter (recommended):**

Add matching dividers on *both* the sender node and the supply tap so
everything stays under ~3.3V even at 12V, and run the ADS1115 from the
ESP32-C3's 3.3V rail directly (no level shifter needed):

```
12V --[ R_FIXED = 190R ]-- NODE --[ sender ]-- GND
NODE --[ 10k ]-- ADC_NODE --[ 3.3k ]-- GND              -> ADS1115 AIN0

12V --[ 10k ]-- ADC_SUPPLY --[ 3.3k ]-- GND             -> ADS1115 AIN1
```

That 10k/3.3k divider attenuates by 3.3/(10+3.3) = 0.248, so even a 12V
fault-case node reads only ~2.98V at the ADC — safe under `GAIN_ONE`. The
same two-resistor divider (same part values) is used on the supply tap so
the two channels' attenuations match and cancel in the ratio calculation.
For a 5V system with Option B's dividers, everything just reads
proportionally lower — still fine, only using less of the ADC's range.

Either way: add a small ceramic capacitor (e.g. 100nF) from each ADC input
to GND for extra high-frequency filtering, and keep the sender wiring
short or shielded if it's routed anywhere near motors/solenoids/bilge pumps.

`R_FIXED_OHMS` in `Config.h` must match whatever fixed resistor you
actually install — measure it with a multimeter and enter the real value
for best accuracy (190Ω is a good nominal choice: it centers the divider
and gives even sensitivity across the sender's full 0-190Ω range).

### ADS1115 -> ESP32-C3 (I2C)

| ADS1115 | ESP32-C3          |
|---------|-------------------|
| VDD     | 3.3V (Option B) or 5V (Option A) |
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

Every requirement you asked for maps to a stage in `SenderADC::read()`
(`SenderADC.cpp`):

1. **Analog noise filter (hardware):** the divider resistors plus the
   optional 100nF caps on each ADC input, as described above. Nothing on a
   boat's rudder moves fast enough to need more than that.
2. **ADC-level noise filter — oversampling:** `OVERSAMPLE_COUNT` raw ADC
   reads are taken back-to-back and averaged, for *each* of the two
   channels, every sample cycle. This directly attacks noise at the ADC's
   own sample rate — including wiper/contact noise from the resistive
   sender itself.
3. **ADS1115 internal filter:** the ADS1115 has a built-in sinc decimation
   filter whose bandwidth is set by its data rate (`ADS1115_DATA_RATE`).
   Lower SPS = narrower filter = quieter but slower conversions. 64 SPS is a
   good default for a rudder (nothing on a boat moves the helm at more than
   a few Hz).
4. **Ratiometric conversion:** the two (already oversampled) channel
   voltages are combined into a sender resistance as described above — this
   is itself a correction, removing sensitivity to supply drift/ripple.
5. **Software filter — median:** `MedianFilter` (`MEDIAN_WINDOW` samples)
   rejects single-sample outliers/spikes (a flaky wiper contact, a nearby
   solenoid switching) that survive averaging, without smearing them across
   neighboring readings the way a mean filter would.
6. **Mean value averaging:** `MovingAverageFilter` (`MEAN_WINDOW` samples)
   is the classic moving-average smoother over recent history.
7. **Software filter — EMA:** `EMAFilter` (`EMA_ALPHA`) is a first-order
   low-pass IIR filter that gives the final, steady output. Lower `alpha` =
   smoother but slower to track real movement.

On top of the filter chain, `loop()` and `SenderADC::read()` apply:

- **Fault detection:** a supply-sense reading below `SUPPLY_FAULT_MIN_V`
  means the excitation supply itself looks disconnected; a node voltage
  too close to the supply voltage (`FAULT_DENOM_EPSILON_V`) means the
  sender has gone open-circuit; a computed resistance outside
  `SENDER_FAULT_LOW_OHMS`/`SENDER_FAULT_HIGH_OHMS` means a short or other
  out-of-range fault. Any of these show `SENSOR FAULT` on the HMI instead
  of a bogus angle.
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

Rather than assuming the sender's full 0-190Ω/0-100° native range maps
directly onto your rudder's actual travel, the firmware stores a 3-point
calibration table (port hard-over, midships, starboard hard-over) in flash
(ESP32 NVS via `Preferences`) as **resistance**-to-angle pairs, and linearly
interpolates between points. Because it's resistance-based rather than
voltage-based, it stays valid regardless of your excitation voltage,
divider resistor values, or their tolerance. Turn the wheel to each
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
works with either wiring polarity (sender resistance rising or falling
with angle).

## Files

- `RudderAngleIndicator.ino` - setup/loop and the serial calibration commands.
- `Config.h` - all pins, sender/divider constants, and filter tuning constants.
- `SenderADC.h/.cpp` - ratiometric two-channel resistance measurement + full filter pipeline.
- `Filters.h/.cpp` - `MovingAverageFilter`, `MedianFilter`, `EMAFilter`.
- `Calibration.h/.cpp` - 3-point (resistance -> angle) calibration table with NVS persistence.
- `HMI.h/.cpp` - minimal Nextion/TJC "Usart HMI" protocol wrapper.
