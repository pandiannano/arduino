#include "Calibration.h"
#include <Preferences.h>

static const char *NVS_NAMESPACE = "rudder";
static const char *NVS_KEY = "points";

void Calibration::begin() {
  load();
}

void Calibration::setPoint(uint8_t index, float voltage, float angle) {
  if (index > 2) return;
  _points[index].voltage = voltage;
  _points[index].angle = angle;
}

static float interpolate(float v, const CalPoint &a, const CalPoint &b) {
  float dv = b.voltage - a.voltage;
  if (fabsf(dv) < 0.0001f) return a.angle;  // Degenerate calibration; avoid divide-by-zero.
  float t = (v - a.voltage) / dv;
  return a.angle + t * (b.angle - a.angle);
}

float Calibration::voltageToAngle(float voltage) const {
  const CalPoint &port = _points[0];
  const CalPoint &mid = _points[1];
  const CalPoint &stbd = _points[2];

  bool increasing = (stbd.voltage >= port.voltage);
  bool towardStbd = increasing ? (voltage >= mid.voltage) : (voltage <= mid.voltage);

  float angle = towardStbd ? interpolate(voltage, mid, stbd) : interpolate(voltage, port, mid);

  // Clamp: extrapolating past a physical hard-over stop isn't meaningful.
  float minAngle = min(port.angle, stbd.angle);
  float maxAngle = max(port.angle, stbd.angle);
  return constrain(angle, minAngle, maxAngle);
}

void Calibration::save() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBytes(NVS_KEY, _points, sizeof(_points));
  prefs.end();
}

void Calibration::load() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  if (prefs.isKey(NVS_KEY) && prefs.getBytesLength(NVS_KEY) == sizeof(_points)) {
    prefs.getBytes(NVS_KEY, _points, sizeof(_points));
  }
  prefs.end();
}

void Calibration::printStatus(Stream &out) const {
  static const char *names[3] = {"PORT", "MID", "STBD"};
  out.println(F("Rudder calibration:"));
  for (uint8_t i = 0; i < 3; i++) {
    out.printf("  %s: %.3f V -> %.1f deg\n", names[i], _points[i].voltage, _points[i].angle);
  }
}
