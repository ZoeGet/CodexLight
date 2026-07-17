#ifndef CODEXLIGHT_CONFIG_H
#define CODEXLIGHT_CONFIG_H

#include <Arduino.h>

namespace CodexLightConfig {

constexpr uint8_t RED_LED_PIN = 7;
constexpr uint8_t GREEN_LED_PIN = 6;
constexpr uint8_t YELLOW_LED_PIN = 5;

constexpr uint8_t LEDS_PER_CHANNEL = 1;
constexpr uint8_t DEFAULT_BRIGHTNESS = 25;

constexpr unsigned long SERIAL_BAUD = 115200;
constexpr uint16_t UDP_PORT = 4210;
constexpr unsigned long LINK_TIMEOUT_MS = 6000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr unsigned long DISCONNECTED_BLINK_HALF_PERIOD_MS = 500;
constexpr unsigned long CONNECTED_ANIMATION_MS = 2000;
constexpr unsigned long CONNECTED_BLINK_HALF_PERIOD_MS = 250;
constexpr unsigned long LED_REFRESH_INTERVAL_MS = 1000;

// AP provisioning hotspot settings. The actual SSID is
// CONFIG_AP_SSID_PREFIX plus the last bytes of the ESP32 MAC address, for
// example CodexLight-A1B2.
constexpr const char* CONFIG_AP_SSID_PREFIX = "CodexLight";
constexpr const char* CONFIG_AP_PASSWORD = "123456789";

// Change this to "WIRED" or "WIRELESS" to choose a fixed startup mode.
// "AUTO" accepts both transports and prefers a recent wired heartbeat.
constexpr const char* DEFAULT_TRANSPORT_MODE = "WIRED";

// Enables firmware diagnostics on the USB serial port. Open the VS Code /
// PlatformIO serial monitor at SERIAL_BAUD to inspect transport, Wi-Fi, UDP,
// configuration portal, and MAC status while debugging hardware setup.
constexpr bool DEBUG_SERIAL = true;

constexpr uint8_t RED_COLOR_R = 255;
constexpr uint8_t RED_COLOR_G = 0;
constexpr uint8_t RED_COLOR_B = 0;

constexpr uint8_t GREEN_COLOR_R = 0;
constexpr uint8_t GREEN_COLOR_G = 255;
constexpr uint8_t GREEN_COLOR_B = 0;

constexpr uint8_t YELLOW_COLOR_R = 255;
constexpr uint8_t YELLOW_COLOR_G = 255;
constexpr uint8_t YELLOW_COLOR_B = 0;

}  // namespace CodexLightConfig

#endif  // CODEXLIGHT_CONFIG_H
