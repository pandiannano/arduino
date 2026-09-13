#pragma once

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

#include "Filters.h"

// Reads the resistive rudder sender (0-190 ohm) via a ratiometric two-channel
// ADS1115 measurement and runs it through the full noise-filtering pipeline.
//
// Circuit assumed: Vexc --[R_FIXED]-- node --[sender]-- GND, with the node
// read on one ADS1115 channel and Vexc itself (through an identical divider)
// read on another. See README.md for the wiring and the math.
class SenderADC {
public:
  SenderADC();

  void begin(Adafruit_ADS1115 &ads);

  // Runs one full read+filter cycle and returns the filtered sender
  // resistance in ohms. Returns false (leaving resistanceOhmsOut untouched)
  // if the reading indicates a wiring/sensor fault.
  bool read(float &resistanceOhmsOut);

private:
  Adafruit_ADS1115 *_ads = nullptr;

  MovingAverageFilter _nodeOversample;
  MovingAverageFilter _supplyOversample;
  MedianFilter _spikeReject;
  MovingAverageFilter _slowMean;
  EMAFilter _smoother;

  float readChannelVolts(uint8_t channel, MovingAverageFilter &oversample);
};
