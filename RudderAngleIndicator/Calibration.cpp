#include "Calibration.h"
#include <Preferences.h>

static const char *NVS_NAMESPACE = "rudder";
static const char *NVS_KEY = "points";

void Calibration::begin() {
  load();
}

void Calibration::setPoint(uint8_t index, float resistance, float angle) {
  if (index > 2) return;
  _points[index].resistance = resistance;
  _points[index].angle = angle;
}

static float interpolate(float r, const CalPoint &a, const CalPoint &b) {
  float dr = b.resistance - a.resistance;
  if (fabsf(dr) < 0.0001f) return a.angle;  // Degenerate calibration; avoid divide-by-zero.
  float t = (r - a.resistance) / dr;
  return a.angle + t * (b.angle - a.angle);
}

float Calibration::resistanceToAngle(float resistanceOhms) const {
  const CalPoint &port = _points[0];
  const CalPoint &mid = _points[1];
  const CalPoint &stbd = _points[2];

  bool increasing = (stbd.resistance >= port.resistance);
  bool towardStbd = increasing ? (resistanceOhms >= mid.resistance) : (resistanceOhms <= mid.resistance);

  float angle = towardStbd ? interpolate(resistanceOhms, mid, stbd) : interpolate(resistanceOhms, port, mid);

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
    out.printf("  %s: %.2f ohm -> %.1f deg\n", names[i], _points[i].resistance, _points[i].angle);
  }
}
