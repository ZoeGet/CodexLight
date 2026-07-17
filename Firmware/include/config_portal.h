#ifndef CODEXLIGHT_CONFIG_PORTAL_H
#define CODEXLIGHT_CONFIG_PORTAL_H

#include <Arduino.h>
#include <WiFiManager.h>

class ConfigPortal {
 public:
  void begin();
  void process();
  void start();
  void resetSettings();
  bool wifiConnected() const;
  const String& apSsid() const;

 private:
  WiFiManager manager_;
  String apSsid_;
  bool initialized_ = false;

  void buildApSsid();
};

#endif
