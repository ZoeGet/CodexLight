#include "storage.h"

#include <Preferences.h>

namespace {

constexpr const char* WIFI_NAMESPACE = "wifi";
constexpr const char* SSID_KEY = "ssid";
constexpr const char* PASSWORD_KEY = "password";

}  // namespace

namespace WifiStorage {

bool load(WifiCredentials& credentials) {
  Preferences preferences;
  preferences.begin(WIFI_NAMESPACE, true);
  credentials.ssid = preferences.getString(SSID_KEY, "");
  credentials.password = preferences.getString(PASSWORD_KEY, "");
  preferences.end();
  credentials.ssid.trim();
  return credentials.ssid.length() > 0;
}

void save(const String& ssid, const String& password) {
  Preferences preferences;
  preferences.begin(WIFI_NAMESPACE, false);
  preferences.putString(SSID_KEY, ssid);
  preferences.putString(PASSWORD_KEY, password);
  preferences.end();
}

void clear() {
  Preferences preferences;
  preferences.begin(WIFI_NAMESPACE, false);
  preferences.clear();
  preferences.end();
}

}  // namespace WifiStorage
