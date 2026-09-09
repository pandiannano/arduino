#pragma once
#include <Arduino.h>

// UART driver for the HMI display. Assumes a Nextion-compatible protocol:
// ASCII command text followed by a 0xFF 0xFF 0xFF terminator. This covers
// the great majority of hobbyist "UART HMI" touchscreens (genuine Nextion
// and most Nextion-compatible clones). If your display uses a different
// wire protocol (e.g. DWIN's binary VP-address frames), only sendAngle()
// needs to change — nothing else in the firmware depends on it.
class HMI {
public:
  void begin(HardwareSerial &serial, unsigned long baud);

  // Rate-limited, deadbanded update — see Config.h for the thresholds.
  // Safe to call on every filter sample; most calls will be no-ops.
  void sendAngle(float angleDeg, bool fault);

private:
  HardwareSerial *_serial = nullptr;
  float _lastSent = NAN;
  bool _lastFault = false;
  unsigned long _lastUpdateMs = 0;

  void sendCommand(const char *cmd);
};
