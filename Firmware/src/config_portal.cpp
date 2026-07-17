#include "config_portal.h"

#include <WiFi.h>

#include "config.h"

using namespace CodexLightConfig;

void ConfigPortal::begin() {
  buildApSsid();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  manager_.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SECONDS);
  manager_.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000UL);
  manager_.setAPStaticIPConfig(
      IPAddress(192, 168, 4, 1),
      IPAddress(192, 168, 4, 1),
      IPAddress(255, 255, 255, 0));
}

bool ConfigPortal::autoConnect() {
  return manager_.autoConnect(apSsid_.c_str(), CONFIG_AP_PASSWORD);
}

void ConfigPortal::resetSettings() {
  manager_.resetSettings();
}

const String& ConfigPortal::apSsid() const {
  return apSsid_;
}

void ConfigPortal::buildApSsid() {
  const String mac = WiFi.macAddress();
  apSsid_ = String(CONFIG_AP_SSID_PREFIX) + "-" + mac.substring(mac.length() - 5);
  apSsid_.replace(":", "");
}
