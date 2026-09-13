#pragma once

#include <Arduino.h>

struct CalPoint {
  float resistance;
  float angle;
};

// Three-point calibration (port hard-over, midships, starboard hard-over),
// expressed in the sender's own resistance rather than voltage - so it stays
// valid regardless of excitation voltage, divider resistors, or their
// tolerance. Linear interpolation between port<->mid and mid<->stbd corrects
// for the sender's offset/gain and for any asymmetry in the mechanical
// linkage. Works with either wiring polarity (resistance rising or falling
// with angle), as long as the three points are entered in physical
// port->mid->stbd order.
class Calibration {
public:
  void begin();

  void setPoint(uint8_t index, float resistance, float angle);  // 0=port, 1=mid, 2=stbd
  float resistanceToAngle(float resistanceOhms) const;

  void save();
  void printStatus(Stream &out) const;

private:
  // Defaults assume the sender's native 0-190R / 0-100deg scale is used
  // as-is; run the CAL PORT/MID/STBD procedure to match your boat's actual
  // rudder limits, which will usually only span part of that native range.
  CalPoint _points[3] = {
      {0.0f, 0.0f},
      {95.0f, 50.0f},
      {190.0f, 100.0f},
  };

  void load();
};
