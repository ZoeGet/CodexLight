#ifndef CODEXLIGHT_STORAGE_H
#define CODEXLIGHT_STORAGE_H

#include <Arduino.h>

struct WifiCredentials {
  String ssid;
  String password;
};

namespace WifiStorage {

bool load(WifiCredentials& credentials);
void save(const String& ssid, const String& password);
void clear();

}  // namespace WifiStorage

#endif  // CODEXLIGHT_STORAGE_H
