#pragma once

#include <Arduino.h>

// Thin wrapper around the Nextion-compatible "Usart HMI" instruction set used
// by TJC displays (e.g. TJC8048X243): ASCII commands of the form
// `component.attribute=value`, each terminated by three 0xFF bytes.
class HMIDisplay {
public:
  void begin(HardwareSerial &serial, uint32_t baud, int8_t rxPin, int8_t txPin);

  void sendCommand(const String &cmd);
  void setText(const char *component, const String &text);
  void setValue(const char *component, int32_t value);

  // High-level helper: pushes the angle to the numeric, gauge and text
  // components in one call. Pass nullptr for any component you don't use.
  void updateRudderAngle(float angleDegrees, const char *numberComponent,
                         const char *gaugeComponent, const char *textComponent);

  void showFault(const char *textComponent, const char *message);

private:
  HardwareSerial *_serial = nullptr;
  void endCommand();
};
