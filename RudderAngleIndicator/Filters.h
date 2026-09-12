#pragma once

#include <Arduino.h>

// Simple moving average over a fixed-size window (a running "mean value average").
class MovingAverageFilter {
public:
  explicit MovingAverageFilter(uint8_t windowSize);
  ~MovingAverageFilter();

  float update(float newSample);
  void reset();

private:
  float *_buffer;
  uint8_t _size;
  uint8_t _index;
  uint8_t _count;
  float _sum;
};

// Median filter: rejects single-sample outliers (spikes) that a mean filter
// would only dilute. Window size should be odd.
class MedianFilter {
public:
  explicit MedianFilter(uint8_t windowSize);
  ~MedianFilter();

  float update(float newSample);
  void reset();

private:
  float *_buffer;
  float *_sorted;
  uint8_t _size;
  uint8_t _index;
  uint8_t _count;
};

// First-order exponential moving average (IIR low-pass): y += alpha*(x - y).
class EMAFilter {
public:
  explicit EMAFilter(float alpha);

  float update(float newSample);
  void reset(float initialValue);
  void setAlpha(float alpha);

private:
  float _alpha;
  float _value;
  bool _initialized;
};
