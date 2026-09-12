// =====================================================================
// Rudder Angle Indicator
//
// ESP32-C3 + ADS1115 (I2C ADC) + 0-5V Hall-effect rudder angle sensor
// + TJC8048X243 HMI (Nextion-compatible instruction set, over UART)
//
// Filter pipeline (see README.md for the full rationale):
//   1. Oversampling: several raw ADC reads averaged per sample (ADC noise).
//   2. ADS1115 internal sinc decimation filter (set via data rate).
//   3. Median filter: rejects transient spikes/outliers.
//   4. Moving average: "mean value averaging" over recent history.
//   5. Exponential moving average: final smoothing for a steady readout.
//
// Requires: Adafruit_ADS1X15 library (Library Manager: "Adafruit ADS1X15").
// =====================================================================

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#include "Config.h"
#include "Filters.h"
#include "Calibration.h"
#include "HMI.h"

Adafruit_ADS1115 ads;
Calibration calibration;
HMIDisplay hmi;

MovingAverageFilter oversampleAvg(OVERSAMPLE_COUNT);  // averages the raw back-to-back ADC reads
MedianFilter spikeReject(MEDIAN_WINDOW);              // rejects single-sample outliers
MovingAverageFilter slowMean(MEAN_WINDOW);             // moving-average / "mean value averaging"
EMAFilter smoother(EMA_ALPHA);                         // final low-pass smoothing

float displayedAngle = 0.0f;
uint32_t lastHmiUpdate = 0;
uint32_t lastSampleTime = 0;

bool readFilteredVoltage(float &voltageOut);
void handleSerialCommands();

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  if (!ads.begin(ADS1115_I2C_ADDRESS, &Wire)) {
    Serial.println(F("ERROR: ADS1115 not found - check wiring/I2C address."));
  }
  ads.setGain(ADS1115_GAIN);
  ads.setDataRate(ADS1115_DATA_RATE);

  calibration.begin();
  hmi.begin(Serial1, HMI_BAUD, HMI_RX_PIN, HMI_TX_PIN);

  Serial.println(F("Rudder angle indicator ready."));
  Serial.println(F("Serial commands: CAL PORT <deg> | CAL MID <deg> | CAL STBD <deg> | CAL SAVE | CAL SHOW"));
}

void loop() {
  handleSerialCommands();

  uint32_t now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) return;
  lastSampleTime = now;

  float voltage;
  if (!readFilteredVoltage(voltage)) {
    hmi.showFault(HMI_TEXT_COMPONENT, "SENSOR FAULT");
    return;
  }

  float rawAngle = calibration.voltageToAngle(voltage);

  // Deadband + slew limiting: stops the needle/readout from twitching on
  // residual quantization noise, without adding lag to a real helm movement.
  float delta = rawAngle - displayedAngle;
  if (fabsf(delta) < ANGLE_DEADBAND_DEG) {
    delta = 0.0f;
  } else {
    delta = constrain(delta, -MAX_SLEW_DEG_PER_UPDATE, MAX_SLEW_DEG_PER_UPDATE);
  }
  displayedAngle += delta;

  bool changed = fabsf(delta) > 0.0f;
  bool heartbeatDue = (now - lastHmiUpdate) >= HMI_HEARTBEAT_MS;
  bool minIntervalElapsed = (now - lastHmiUpdate) >= HMI_MIN_UPDATE_INTERVAL_MS;

  if (minIntervalElapsed && (changed || heartbeatDue)) {
    hmi.updateRudderAngle(displayedAngle, HMI_NUMBER_COMPONENT, HMI_GAUGE_COMPONENT, HMI_TEXT_COMPONENT);
    lastHmiUpdate = now;
  }
}

// Runs the full filter pipeline once and returns the resulting sensor-side
// voltage (i.e. already corrected for SENSOR_DIVIDER_RATIO). Returns false
// if the reading looks like a sensor/wiring fault.
bool readFilteredVoltage(float &voltageOut) {
  float acc = 0.0f;
  for (uint8_t i = 0; i < OVERSAMPLE_COUNT; i++) {
    acc += ads.readADC_SingleEnded(ADS1115_CHANNEL);
  }
  float oversampled = acc / OVERSAMPLE_COUNT;

  float meanStage = oversampleAvg.update(oversampled);
  float median = spikeReject.update(meanStage);
  float windowedMean = slowMean.update(median);
  float smoothed = smoother.update(windowedMean);

  float voltage = ads.computeVolts((int16_t)lroundf(smoothed)) / SENSOR_DIVIDER_RATIO;

  if (voltage < SENSOR_FAULT_LOW_V || voltage > SENSOR_FAULT_HIGH_V) {
    return false;
  }

  voltageOut = voltage;
  return true;
}

// Serial commands for field calibration, e.g.:
//   CAL PORT -35.0    (turn rudder to port hard-over, then send)
//   CAL MID 0
//   CAL STBD 35.0
//   CAL SHOW          (print the current table)
//   CAL SAVE          (persist to flash / NVS)
void handleSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("CAL SHOW")) {
    calibration.printStatus(Serial);
    return;
  }
  if (line.equalsIgnoreCase("CAL SAVE")) {
    calibration.save();
    Serial.println(F("Calibration saved to flash."));
    return;
  }

  int firstSpace = line.indexOf(' ');
  int secondSpace = line.indexOf(' ', firstSpace + 1);
  if (!line.startsWith("CAL ") || firstSpace < 0 || secondSpace < 0) {
    Serial.println(F("Unknown command."));
    return;
  }

  String which = line.substring(firstSpace + 1, secondSpace);
  float angle = line.substring(secondSpace + 1).toFloat();

  float voltage;
  if (!readFilteredVoltage(voltage)) {
    Serial.println(F("Cannot calibrate: sensor fault reading."));
    return;
  }

  uint8_t index;
  if (which.equalsIgnoreCase("PORT")) index = 0;
  else if (which.equalsIgnoreCase("MID")) index = 1;
  else if (which.equalsIgnoreCase("STBD")) index = 2;
  else {
    Serial.println(F("Unknown point, use PORT, MID or STBD."));
    return;
  }

  calibration.setPoint(index, voltage, angle);
  Serial.printf("Captured %s: %.3f V -> %.1f deg (send CAL SAVE to persist)\n", which.c_str(), voltage, angle);
}
