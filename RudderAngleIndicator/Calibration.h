#pragma once

#include <Arduino.h>

struct CalPoint {
  float voltage;
  float angle;
};

// Three-point calibration (port hard-over, midships, starboard hard-over).
// Linear interpolation between port<->mid and mid<->stbd corrects for the
// sensor's offset/gain and for any asymmetry in the mechanical linkage.
// Works with either wiring polarity (voltage rising or falling with angle),
// as long as the three points are entered in physical port->mid->stbd order.
class Calibration {
public:
  void begin();

  void setPoint(uint8_t index, float voltage, float angle);  // 0=port, 1=mid, 2=stbd
  float voltageToAngle(float voltage) const;

  void save();
  void printStatus(Stream &out) const;

private:
  CalPoint _points[3] = {
      {0.5f, -35.0f},
      {2.5f, 0.0f},
      {4.5f, 35.0f},
  };

  void load();
};
