#pragma once
#include <Arduino.h>
#include "Config.h"

// Turns a stream of raw ADS1120 codes into a clean rudder angle:
//   raw code -> median filter -> calibration (code -> degrees)
//            -> circular moving average -> circular EMA -> final angle
//
// The averaging stages work on unit vectors (cos/sin of the angle) rather
// than on the degree values directly. A plain arithmetic mean breaks down
// near a wrap point (e.g. averaging 359 deg and 1 deg naively gives 180 deg,
// not the correct ~0 deg) — vector averaging gives the correct circular mean
// unconditionally, so the filter stays correct even if the mechanical zero
// ends up close to the sensor's own 0/360 rollover point.
class AngleFilter {
public:
  void begin();

  // Feed one new raw ADS1120 code; returns the filtered angle in degrees.
  float update(int32_t rawCode);

  bool isFault() const { return _fault; }

private:
  // --- stage 1: median filter on raw codes ---
  int32_t _medianBuf[MEDIAN_WINDOW];
  uint8_t _medianIndex = 0;
  bool _medianFilled = false;
  int32_t medianFilter(int32_t newCode);

  // --- stage 2: calibration ---
  float codeToAngle(int32_t code) const;

  // --- stage 3: circular moving average ---
  float _maCosBuf[MOVING_AVERAGE_WINDOW];
  float _maSinBuf[MOVING_AVERAGE_WINDOW];
  uint8_t _maIndex = 0;
  uint8_t _maCount = 0;
  float _maCosSum = 0.0f;
  float _maSinSum = 0.0f;

  // --- stage 4: circular EMA ---
  float _emaCos = 0.0f;
  float _emaSin = 0.0f;
  bool _emaInit = false;

  bool _fault = false;
};
