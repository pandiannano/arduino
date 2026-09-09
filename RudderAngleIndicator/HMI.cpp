#include "HMI.h"
#include "Config.h"

void HMI::begin(HardwareSerial &serial, unsigned long baud) {
  _serial = &serial;
  _serial->begin(baud, SERIAL_8N1, PIN_HMI_RX, PIN_HMI_TX);
}

void HMI::sendCommand(const char *cmd) {
  _serial->print(cmd);
  _serial->write(0xFF);
  _serial->write(0xFF);
  _serial->write(0xFF);
}

void HMI::sendAngle(float angleDeg, bool fault) {
  unsigned long now = millis();
  bool faultChanged = (fault != _lastFault);
  bool dueForUpdate = (now - _lastUpdateMs) >= HMI_MIN_UPDATE_INTERVAL_MS;
  bool movedEnough = isnan(_lastSent) || fabsf(angleDeg - _lastSent) >= HMI_MIN_UPDATE_DELTA_DEG;

  if (!faultChanged && !(dueForUpdate && movedEnough)) {
    return;
  }

  char buf[48];

  snprintf(buf, sizeof(buf), "%s.txt=\"%.1f\"", HMI_TEXT_COMPONENT,
           fault ? 0.0f : angleDeg);
  sendCommand(buf);

  // Scale to an integer for a needle/gauge component: 0 at ANGLE_MIN_DEG,
  // 1000 at ANGLE_MAX_DEG. Adjust the scale/expression in the HMI project
  // to match how its rotation/pointer widget is actually driven.
  long scaled = lround((angleDeg - ANGLE_MIN_DEG) * 1000.0f /
                        (ANGLE_MAX_DEG - ANGLE_MIN_DEG));
  scaled = constrain(scaled, 0L, 1000L);
  snprintf(buf, sizeof(buf), "%s.val=%ld", HMI_VALUE_COMPONENT, scaled);
  sendCommand(buf);

  snprintf(buf, sizeof(buf), "%s.val=%d", HMI_FAULT_COMPONENT, fault ? 1 : 0);
  sendCommand(buf);

  _lastSent = angleDeg;
  _lastFault = fault;
  _lastUpdateMs = now;
}
