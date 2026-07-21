#include "config_portal.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include "config.h"
#include "storage.h"

using namespace CodexLightConfig;

namespace {

void logNetwork(const String& message) {
  if (DEBUG_SERIAL) {
    Serial.println(String("[WiFi] ") + message);
  }
}

const char* encryptionName(wifi_auth_mode_t type) {
  switch (type) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2_WPA3_PSK";
    default:
      return "UNKNOWN";
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
  startSavedConnect(credentials.ssid, credentials.password);
  return WiFi.status() == WL_CONNECTED;
}

bool ConfigPortal::start() {
  begin();
  logNetwork("AP provisioning disabled; use USB serial WIFI_SET");
  return false;
}

void ConfigPortal::loop() {
  if (WiFi.status() == WL_CONNECTED) {
    currentSsid_ = WiFi.SSID();
    return;
  }

  WifiCredentials credentials;
  if (!loadCredentials(credentials)) {
    return;
  }

  currentSsid_ = credentials.ssid;
  const unsigned long now = millis();
  if (now - lastReconnectAttemptMs_ < WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }

  startSavedConnect(credentials.ssid, credentials.password);
}

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
    lastReconnectAttemptMs_ = 0;
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

void ConfigPortal::startSavedConnect(const String& ssid, const String& password) {
  lastReconnectAttemptMs_ = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER_QDBM);
  lastDisconnectReason_ = 0;
  logNetwork("Starting saved Wi-Fi " + ssid + " tx_power_qdbm=" + String(WIFI_MAX_TX_POWER_QDBM));
  WiFi.begin(ssid.c_str(), password.c_str());
}

bool ConfigPortal::connectTo(const String& ssid, const String& password) {
  WiFi.mode(WIFI_OFF);
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER_QDBM);
  WiFi.disconnect(true, true);
  delay(300);
  lastDisconnectReason_ = 0;
  logNetwork("Connecting to " + ssid + " password_len=" + String(password.length()) +
             " tx_power_qdbm=" + String(WIFI_MAX_TX_POWER_QDBM));

  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentSsid_ = WiFi.SSID();
    logNetwork("Connected to " + WiFi.SSID() + " at " + WiFi.localIP().toString());
    return true;
  }

  logNetwork("Normal connect failed; status=" + String(wifiStatusName()) +
             " reason=" + String(lastDisconnectReason_));
  WiFi.disconnect(false, false);
  delay(300);

  int bestNetwork = -1;
  int bestRssi = -1000;
  logNetwork("Scanning after normal connect failure");
  const int networkCount = WiFi.scanNetworks(false, true);
  for (int i = 0; i < networkCount; ++i) {
    if (WiFi.SSID(i) != ssid) {
      continue;
    }
    logNetwork("Found target ssid=" + WiFi.SSID(i) + " channel=" + String(WiFi.channel(i)) +
               " rssi=" + String(WiFi.RSSI(i)) + " auth=" + encryptionName(WiFi.encryptionType(i)) +
               " bssid=" + WiFi.BSSIDstr(i));
    if (WiFi.RSSI(i) > bestRssi) {
      bestRssi = WiFi.RSSI(i);
      bestNetwork = i;
    }
  }

  if (bestNetwork >= 0) {
    const int32_t channel = WiFi.channel(bestNetwork);
    WiFi.scanDelete();
    logNetwork("Retrying with target channel=" + String(channel));
    WiFi.begin(ssid.c_str(), password.c_str(), channel);
  } else {
    WiFi.scanDelete();
    logNetwork("Target SSID not found in scan; trying normal connect anyway");
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  started = millis();
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
