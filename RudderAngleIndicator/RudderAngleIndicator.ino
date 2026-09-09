// Rudder angle indicator
// ESP32-C3 + ADS1120 (SPI, 24-bit ADC) + 0-5V Hall effect angle sensor
// + UART HMI touch display.
//
// Wiring, calibration procedure and filter tuning are documented in
// Config.h. Run once with CALIBRATION_MODE set to 1 in Config.h to find
// CAL_CODE_AT_A / CAL_CODE_AT_B before normal use.

#include <SPI.h>
#include "Config.h"
#include "ADS1120.h"
#include "AngleFilter.h"
#include "HMI.h"

ADS1120 adc(PIN_ADS_CS, PIN_ADS_DRDY);
AngleFilter filter;
HMI hmi;

void setup() {
  Serial.begin(115200);

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_ADS_CS);
  adc.begin(SPI);
  adc.start();

  filter.begin();

#if !CALIBRATION_MODE
  hmi.begin(Serial1, HMI_BAUD);
#endif
}

void loop() {
  if (!adc.dataReady()) {
    return;
  }

  int32_t raw = adc.readData();

#if CALIBRATION_MODE
  // Print the raw code so it can be read off at known rudder angles and
  // copied into CAL_CODE_AT_A / CAL_CODE_AT_B in Config.h.
  Serial.println(raw);
#else
  float angle = filter.update(raw);
  bool fault = filter.isFault();
  hmi.sendAngle(angle, fault);

#if DEBUG_SERIAL
  Serial.print("raw=");
  Serial.print(raw);
  Serial.print("  angle=");
  Serial.print(angle, 2);
  Serial.println(fault ? "  FAULT" : "");
#endif
#endif
}
