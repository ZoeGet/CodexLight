#ifndef CODEXLIGHT_CONFIG_PORTAL_H
#define CODEXLIGHT_CONFIG_PORTAL_H

#include <Arduino.h>
#include <WiFiManager.h>

class ConfigPortal {
 public:
  void begin();
  bool autoConnect();
  void resetSettings();
  const String& apSsid() const;

 private:
  WiFiManager manager_;
  String apSsid_;

  void buildApSsid();
};

#endif  // CODEXLIGHT_CONFIG_PORTAL_H
