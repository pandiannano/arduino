#include "AngleFilter.h"
#include <string.h>

void AngleFilter::begin() {
  _medianIndex = 0;
  _medianFilled = false;
  _maIndex = 0;
  _maCount = 0;
  _maCosSum = 0.0f;
  _maSinSum = 0.0f;
  _emaInit = false;
  _fault = false;
}

int32_t AngleFilter::medianFilter(int32_t newCode) {
  _medianBuf[_medianIndex] = newCode;
  _medianIndex = (_medianIndex + 1) % MEDIAN_WINDOW;
  if (_medianIndex == 0) {
    _medianFilled = true;
  }

  int32_t sorted[MEDIAN_WINDOW];
  uint8_t count = _medianFilled ? MEDIAN_WINDOW : _medianIndex;
  if (count == 0) {
    return newCode; // first-ever sample
  }
  memcpy(sorted, _medianBuf, count * sizeof(int32_t));

  // insertion sort — MEDIAN_WINDOW is small (typically 5), so O(n^2) is fine
  for (uint8_t i = 1; i < count; i++) {
    int32_t key = sorted[i];
    int8_t j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  return sorted[count / 2];
}

float AngleFilter::codeToAngle(int32_t code) const {
  float angle = CAL_ANGLE_A + (float)(code - CAL_CODE_AT_A) *
                (CAL_ANGLE_B - CAL_ANGLE_A) / (float)(CAL_CODE_AT_B - CAL_CODE_AT_A);
  return angle - ANGLE_ZERO_TRIM_DEG;
}

float AngleFilter::update(int32_t rawCode) {
  // Sensor/wiring fault: reading pinned near either ADC rail means the
  // input isn't a valid mid-scale Hall voltage (broken wire, unseated
  // connector, shorted input).
  const int32_t FS = 8388607L; // 2^23 - 1, full scale for a 24-bit signed code
  _fault = (rawCode < (-FS + ADC_FULLSCALE_FAULT_MARGIN)) ||
           (rawCode > (FS - ADC_FULLSCALE_FAULT_MARGIN));

  int32_t medianCode = medianFilter(rawCode);
  float angleDeg = codeToAngle(medianCode);
  float rad = radians(angleDeg);
  float c = cosf(rad);
  float s = sinf(rad);

  // circular moving average: running sum, O(1) per sample
  if (_maCount == MOVING_AVERAGE_WINDOW) {
    _maCosSum -= _maCosBuf[_maIndex];
    _maSinSum -= _maSinBuf[_maIndex];
  } else {
    _maCount++;
  }
  _maCosBuf[_maIndex] = c;
  _maSinBuf[_maIndex] = s;
  _maCosSum += c;
  _maSinSum += s;
  _maIndex = (_maIndex + 1) % MOVING_AVERAGE_WINDOW;

  float maCos = _maCosSum / _maCount;
  float maSin = _maSinSum / _maCount;

  // circular EMA on top of the moving average, for extra smoothing /
  // response-time shaping
  if (!_emaInit) {
    _emaCos = maCos;
    _emaSin = maSin;
    _emaInit = true;
  } else {
    _emaCos += EMA_ALPHA * (maCos - _emaCos);
    _emaSin += EMA_ALPHA * (maSin - _emaSin);
  }

  float filteredAngle = degrees(atan2f(_emaSin, _emaCos));

  if (filteredAngle < ANGLE_MIN_DEG - 5.0f || filteredAngle > ANGLE_MAX_DEG + 5.0f) {
    _fault = true;
  }

  return filteredAngle;
}
