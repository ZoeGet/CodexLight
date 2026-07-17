#include "config_portal.h"

#include <WiFi.h>

#include "config.h"

using namespace CodexLightConfig;

void ConfigPortal::begin() {
  if (initialized_) {
    return;
  }

  buildApSsid();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  std::vector<const char*> menu = {"wifi", "exit"};
  manager_.setMenu(menu);
  manager_.setDebugOutput(false);
  manager_.setConfigPortalBlocking(false);
  manager_.setHostname(apSsid_);
  manager_.setConfigPortalTimeout(0);
  manager_.setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000UL);
  manager_.setAPStaticIPConfig(
      IPAddress(192, 168, 4, 1),
      IPAddress(192, 168, 4, 1),
      IPAddress(255, 255, 255, 0));

  manager_.autoConnect(apSsid_.c_str(), CONFIG_AP_PASSWORD);
  initialized_ = true;
}

void ConfigPortal::process() {
  if (initialized_) {
    manager_.process();
  }
}

void ConfigPortal::start() {
  begin();
  manager_.startConfigPortal(apSsid_.c_str(), CONFIG_AP_PASSWORD);
}

void ConfigPortal::resetSettings() {
  begin();
  manager_.resetSettings();
  WiFi.disconnect(true, true);
  manager_.startConfigPortal(apSsid_.c_str(), CONFIG_AP_PASSWORD);
}

bool ConfigPortal::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

const String& ConfigPortal::apSsid() const {
  return apSsid_;
}

void ConfigPortal::buildApSsid() {
  const String mac = WiFi.macAddress();
  apSsid_ = String(CONFIG_AP_SSID_PREFIX) + "-" + mac.substring(mac.length() - 5);
  apSsid_.replace(":", "");
}
