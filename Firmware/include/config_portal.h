#ifndef CODEXLIGHT_CONFIG_PORTAL_H
#define CODEXLIGHT_CONFIG_PORTAL_H

#include <Arduino.h>

struct WifiCredentials;

class ConfigPortal {
 public:
  void begin();
  bool autoConnect();
  bool start();
  void loop();
  void resetSettings();
  void configure(const String& ssid, const String& password);

  bool wifiConnected() const;
  bool portalActive() const;
  const String& apSsid() const;
  const String& configuredSsid() const;
  const char* stateName() const;
  const char* wifiStatusName() const;
  uint8_t lastDisconnectReason() const;

 private:
  String apSsid_;
  String currentSsid_;
  bool initialized_ = false;
  unsigned long lastReconnectAttemptMs_ = 0;
  uint8_t lastDisconnectReason_ = 0;

  void buildApSsid();
  void startSavedConnect(const String& ssid, const String& password);
  bool connectTo(const String& ssid, const String& password);
  bool loadCredentials(WifiCredentials& credentials);
};

#endif  // CODEXLIGHT_CONFIG_PORTAL_H
