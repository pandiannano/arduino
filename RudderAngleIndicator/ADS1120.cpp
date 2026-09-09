#include "ADS1120.h"

// ADS1120 command bytes (datasheet Table 12)
static const uint8_t CMD_RESET  = 0x06;
static const uint8_t CMD_START  = 0x08;
static const uint8_t CMD_RDATA  = 0x10;
static const uint8_t CMD_WREG0  = 0x40; // WREG, starting register 0, encoded below

ADS1120::ADS1120(uint8_t csPin, uint8_t drdyPin)
  : _cs(csPin), _drdy(drdyPin), _spi(nullptr),
    _spiSettings(1000000, MSBFIRST, SPI_MODE1) {}

void ADS1120::begin(SPIClass &spiBus) {
  _spi = &spiBus;
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  pinMode(_drdy, INPUT);
}

void ADS1120::sendCommand(uint8_t cmd) {
  _spi->beginTransaction(_spiSettings);
  digitalWrite(_cs, LOW);
  _spi->transfer(cmd);
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
}

void ADS1120::writeRegister(uint8_t reg, uint8_t value) {
  _spi->beginTransaction(_spiSettings);
  digitalWrite(_cs, LOW);
  _spi->transfer(CMD_WREG0 | (reg << 2)); // write 1 register starting at `reg`
  _spi->transfer(value);
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
}

void ADS1120::start() {
  digitalWrite(_cs, HIGH);
  delayMicroseconds(50);
  sendCommand(CMD_RESET);
  delay(1); // datasheet: allow 50us+ after reset before further commands; 1ms is generous

  // CONFIG0: MUX=1000 (AIN0 vs AVSS, single-ended), GAIN=000 (x1),
  // PGA_BYPASS=1. The PGA is bypassed because our input swings close to
  // AVDD (0-5 V) — the PGA's own input range does not cover that.
  writeRegister(0, 0x81);

  // CONFIG1: DR=000 (20 SPS, Normal mode — the slowest/most-filtered rate),
  // MODE=00 (Normal), CM=1 (continuous conversion, so the ADC free-runs and
  // we just poll DRDY), TS=0, BCS=0.
  writeRegister(1, 0x04);

  // CONFIG2: VREF=10 (use analog supply AVDD-AVSS as the reference —
  // ratiometric measurement, see Config.h), 50/60=11 (simultaneous 50 Hz
  // and 60 Hz rejection; only guaranteed at 20 SPS Normal mode, which is
  // why DR is fixed to the slowest setting above), PSW=0, IDAC=000.
  writeRegister(2, 0xB0);

  // CONFIG3: IDACs unused, default DRDY behaviour.
  writeRegister(3, 0x00);

  sendCommand(CMD_START); // begin free-running continuous conversions
}

bool ADS1120::dataReady() {
  return digitalRead(_drdy) == LOW;
}

int32_t ADS1120::readData() {
  uint8_t b0, b1, b2;
  _spi->beginTransaction(_spiSettings);
  digitalWrite(_cs, LOW);
  _spi->transfer(CMD_RDATA);
  b0 = _spi->transfer(0xFF);
  b1 = _spi->transfer(0xFF);
  b2 = _spi->transfer(0xFF);
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();

  int32_t value = ((int32_t)b0 << 16) | ((int32_t)b1 << 8) | b2;
  if (value & 0x00800000L) {
    value |= 0xFF000000L; // sign-extend 24-bit two's complement to 32-bit
  }
  return value;
}
