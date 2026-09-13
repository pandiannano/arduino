#include "HMI.h"

void HMIDisplay::begin(HardwareSerial &serial, uint32_t baud, int8_t rxPin, int8_t txPin) {
  _serial = &serial;
  _serial->begin(baud, SERIAL_8N1, rxPin, txPin);
  delay(200);  // Let the HMI finish its own boot sequence before the first command.
}

void HMIDisplay::endCommand() {
  // TJC/Nextion instructions are terminated by three 0xFF bytes.
  _serial->write((uint8_t)0xFF);
  _serial->write((uint8_t)0xFF);
  _serial->write((uint8_t)0xFF);
}

void HMIDisplay::sendCommand(const String &cmd) {
  if (!_serial) return;
  _serial->print(cmd);
  endCommand();
}

void HMIDisplay::setText(const char *component, const String &text) {
  sendCommand(String(component) + ".txt=\"" + text + "\"");
}

void HMIDisplay::setValue(const char *component, int32_t value) {
  sendCommand(String(component) + ".val=" + String(value));
}

void HMIDisplay::updateRudderAngle(float angleDegrees, const char *numberComponent,
                                   const char *gaugeComponent, const char *textComponent) {
  if (numberComponent) {
    // Sent as tenths of a degree so the HMI can show one decimal without doing float math itself.
    setValue(numberComponent, lroundf(angleDegrees * 10.0f));
  }

  if (gaugeComponent) {
    int32_t gaugeAngle = lroundf(angleDegrees);
    if (gaugeAngle < 0) gaugeAngle += 360;  // Most rotating-needle widgets expect 0-360, not negative degrees.
    setValue(gaugeComponent, gaugeAngle);
  }

  if (textComponent) {
    char buf[16];
    dtostrf(angleDegrees, 0, 1, buf);
    const char *side = angleDegrees < -0.5f ? " P" : (angleDegrees > 0.5f ? " S" : "");
    setText(textComponent, String(buf) + side);
  }
}

void HMIDisplay::showFault(const char *textComponent, const char *message) {
  setText(textComponent, message);
}
