#include "Filters.h"

// ---------------- MovingAverageFilter ----------------

MovingAverageFilter::MovingAverageFilter(uint8_t windowSize)
    : _size(windowSize), _index(0), _count(0), _sum(0.0f) {
  _buffer = new float[_size];
  for (uint8_t i = 0; i < _size; i++) _buffer[i] = 0.0f;
}

MovingAverageFilter::~MovingAverageFilter() {
  delete[] _buffer;
}

float MovingAverageFilter::update(float newSample) {
  _sum -= _buffer[_index];
  _buffer[_index] = newSample;
  _sum += newSample;
  _index = (_index + 1) % _size;
  if (_count < _size) _count++;
  return _sum / _count;
}

void MovingAverageFilter::reset() {
  for (uint8_t i = 0; i < _size; i++) _buffer[i] = 0.0f;
  _sum = 0.0f;
  _index = 0;
  _count = 0;
}

// ---------------- MedianFilter ----------------

MedianFilter::MedianFilter(uint8_t windowSize)
    : _size(windowSize), _index(0), _count(0) {
  _buffer = new float[_size];
  _sorted = new float[_size];
}

MedianFilter::~MedianFilter() {
  delete[] _buffer;
  delete[] _sorted;
}

float MedianFilter::update(float newSample) {
  _buffer[_index] = newSample;
  _index = (_index + 1) % _size;
  if (_count < _size) _count++;

  for (uint8_t i = 0; i < _count; i++) _sorted[i] = _buffer[i];

  // Insertion sort - window is small (<=15 typical), cost is negligible.
  for (uint8_t i = 1; i < _count; i++) {
    float key = _sorted[i];
    int8_t j = i - 1;
    while (j >= 0 && _sorted[j] > key) {
      _sorted[j + 1] = _sorted[j];
      j--;
    }
    _sorted[j + 1] = key;
  }

  return _sorted[_count / 2];
}

void MedianFilter::reset() {
  _index = 0;
  _count = 0;
}

// ---------------- EMAFilter ----------------

EMAFilter::EMAFilter(float alpha) : _alpha(alpha), _value(0.0f), _initialized(false) {}

float EMAFilter::update(float newSample) {
  if (!_initialized) {
    _value = newSample;
    _initialized = true;
  } else {
    _value += _alpha * (newSample - _value);
  }
  return _value;
}

void EMAFilter::reset(float initialValue) {
  _value = initialValue;
  _initialized = true;
}

void EMAFilter::setAlpha(float alpha) {
  _alpha = alpha;
}
