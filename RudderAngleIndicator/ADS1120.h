#pragma once
#include <Arduino.h>
#include <SPI.h>

// Minimal register-level driver for the TI ADS1120 24-bit delta-sigma ADC,
// configured here for a single-ended 0-5 V input read ratiometrically
// against its own AVDD supply, running continuous conversions with the
// slowest normal-mode data rate so its internal sinc filter gives
// simultaneous 50/60 Hz mains rejection. See Config.h for the wiring
// rationale.
class ADS1120 {
public:
  ADS1120(uint8_t csPin, uint8_t drdyPin);

  // Call once in setup(), after SPI.begin().
  void begin(SPIClass &spiBus);

  // Full reset + register configuration + start continuous conversions.
  void start();

  // True once a new conversion result is latched (DRDY asserted low).
  bool dataReady();

  // Reads and clears the current conversion result. Only call after
  // dataReady() returns true. Returns a sign-extended 24-bit value.
  int32_t readData();

private:
  uint8_t _cs;
  uint8_t _drdy;
  SPIClass *_spi;
  SPISettings _spiSettings;

  void sendCommand(uint8_t cmd);
  void writeRegister(uint8_t reg, uint8_t value);
};
