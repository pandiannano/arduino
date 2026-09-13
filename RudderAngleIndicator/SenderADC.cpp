#include "SenderADC.h"
#include "Config.h"

SenderADC::SenderADC()
    : _nodeOversample(OVERSAMPLE_COUNT),
      _supplyOversample(OVERSAMPLE_COUNT),
      _spikeReject(MEDIAN_WINDOW),
      _slowMean(MEAN_WINDOW),
      _smoother(EMA_ALPHA) {}

void SenderADC::begin(Adafruit_ADS1115 &ads) {
  _ads = &ads;
}

float SenderADC::readChannelVolts(uint8_t channel, MovingAverageFilter &oversample) {
  // Stage 1: several raw back-to-back ADC reads averaged (ADC-level noise filter).
  float acc = 0.0f;
  for (uint8_t i = 0; i < OVERSAMPLE_COUNT; i++) {
    acc += _ads->readADC_SingleEnded(channel);
  }
  float oversampled = acc / OVERSAMPLE_COUNT;

  // Stage 2: rolling average of that value across cycles.
  float avgCounts = oversample.update(oversampled);

  return _ads->computeVolts((int16_t)lroundf(avgCounts));
}

bool SenderADC::read(float &resistanceOhmsOut) {
  // Both channels go through identical divider networks, so their exact
  // attenuation ratio and the excitation voltage itself both cancel out of
  // the resistance formula below - this measurement is ratiometric and
  // self-corrects for supply drift, ripple, and divider tolerance.
  float vNode = readChannelVolts(ADS1115_CHANNEL_NODE, _nodeOversample);
  float vSupply = readChannelVolts(ADS1115_CHANNEL_SUPPLY, _supplyOversample);

  if (vSupply < SUPPLY_FAULT_MIN_V) {
    return false;  // Excitation supply missing/disconnected.
  }

  float denom = vSupply - vNode;
  if (denom < FAULT_DENOM_EPSILON_V) {
    return false;  // Sender open-circuit: node has been pulled up to the supply.
  }

  float rawResistance = R_FIXED_OHMS * vNode / denom;

  // Stage 3: median filter rejects transient spikes (wiper contact noise, EMI).
  float median = _spikeReject.update(rawResistance);
  // Stage 4: moving average ("mean value averaging") over recent history.
  float windowedMean = _slowMean.update(median);
  // Stage 5: exponential low-pass for the final, steady output.
  float smoothed = _smoother.update(windowedMean);

  if (smoothed < SENDER_FAULT_LOW_OHMS || smoothed > SENDER_FAULT_HIGH_OHMS) {
    return false;
  }

  resistanceOhmsOut = smoothed;
  return true;
}
