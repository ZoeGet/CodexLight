#include "config_portal.h"

#include <WiFi.h>

#include "config.h"
#include "storage.h"

using namespace CodexLightConfig;

namespace {

void logNetwork(const String& message) {
  if (DEBUG_SERIAL) {
    Serial.println(String("[WiFi] ") + message);
  }
}

}  // namespace

void ConfigPortal::begin() {
  if (initialized_) {
    return;
  }

  buildApSsid();
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        logNetwork("Event STA_CONNECTED");
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        lastDisconnectReason_ = info.wifi_sta_disconnected.reason;
        logNetwork("Event STA_DISCONNECTED reason=" + String(lastDisconnectReason_));
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        currentSsid_ = WiFi.SSID();
        logNetwork("Event STA_GOT_IP " + WiFi.localIP().toString());
        break;
      default:
        break;
    }
  });
  initialized_ = true;
}

bool ConfigPortal::autoConnect() {
  begin();

  WifiCredentials credentials;
  if (!loadCredentials(credentials)) {
    logNetwork("No saved Wi-Fi credentials; waiting for USB provisioning");
    WiFi.disconnect(false, true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  currentSsid_ = credentials.ssid;
  if (connectTo(credentials.ssid, credentials.password)) {
    return true;
  }

  WifiStorage::clear();
  currentSsid_ = "";
  logNetwork("Invalid saved Wi-Fi credentials cleared; waiting for USB provisioning");
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_OFF);
  return false;
}

bool ConfigPortal::start() {
  begin();
  logNetwork("AP provisioning disabled; use USB serial WIFI_SET");
  return false;
}

void ConfigPortal::loop() {}

void ConfigPortal::resetSettings() {
  begin();
  WifiStorage::clear();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  currentSsid_ = "";
  lastDisconnectReason_ = 0;
  logNetwork("Saved Wi-Fi credentials cleared");
}

void ConfigPortal::configure(const String& ssid, const String& password) {
  begin();
  currentSsid_ = ssid;
  if (connectTo(ssid, password)) {
    WifiStorage::save(ssid, password);
  } else {
    currentSsid_ = "";
    WiFi.disconnect(false, true);
    WiFi.mode(WIFI_OFF);
  }
}

bool ConfigPortal::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool ConfigPortal::portalActive() const {
  return false;
}

const String& ConfigPortal::apSsid() const {
  return apSsid_;
}

const String& ConfigPortal::configuredSsid() const {
  static String ssid;
  ssid = currentSsid_;
  if (ssid.length() == 0) {
    WifiCredentials credentials;
    if (WifiStorage::load(credentials)) {
      ssid = credentials.ssid;
    }
  }
  return ssid;
}

const char* ConfigPortal::stateName() const {
  if (wifiConnected()) {
    return "CONNECTED";
  }
  return "USB_PROVISIONING";
}

const char* ConfigPortal::wifiStatusName() const {
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

uint8_t ConfigPortal::lastDisconnectReason() const {
  return lastDisconnectReason_;
}

void ConfigPortal::buildApSsid() {
  apSsid_ = "USB_SERIAL";
}

bool ConfigPortal::connectTo(const String& ssid, const String& password) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  lastDisconnectReason_ = 0;
  logNetwork("Connecting to " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentSsid_ = WiFi.SSID();
    logNetwork("Connected to " + WiFi.SSID() + " at " + WiFi.localIP().toString());
    return true;
  }

  logNetwork("Connection to " + ssid + " failed; status=" + String(wifiStatusName()) +
             " reason=" + String(lastDisconnectReason_));
  WiFi.disconnect(false, false);
  return false;
}

bool ConfigPortal::loadCredentials(WifiCredentials& credentials) {
  return WifiStorage::load(credentials);
}
