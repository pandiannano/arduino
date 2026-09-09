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

  // Gauge component: .val is the pointer angle in degrees, 0-359,
  // increasing in whatever direction the widget was drawn. Wrap into that
  // range rather than clamp, since the gauge is circular.
  long gaugeVal = lround(angleDeg + HMI_GAUGE_ZERO_OFFSET_DEG);
  gaugeVal = ((gaugeVal % 360) + 360) % 360;
  snprintf(buf, sizeof(buf), "%s.val=%ld", HMI_GAUGE_COMPONENT, gaugeVal);
  sendCommand(buf);

  snprintf(buf, sizeof(buf), "%s.val=%d", HMI_FAULT_COMPONENT, fault ? 1 : 0);
  sendCommand(buf);

  _lastSent = angleDeg;
  _lastFault = fault;
  _lastUpdateMs = now;
}
