#include <Arduino.h>
#include <cstring>

#include <Preferences.h>

#include "config.h"
#include "config_portal.h"
#include "led.h"

#include <WiFi.h>
#include <WiFiUdp.h>

namespace {

using namespace CodexLightConfig;

LedController leds;
Preferences preferences;
String serialBuffer;
unsigned long lastWirelessPacketMs = 0;
String controlToken;
bool pairingMode = false;
unsigned long pairingModeUntilMs = 0;

WiFiUDP udp;
ConfigPortal configPortal;
bool udpStarted = false;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastHelloMs = 0;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
unsigned long wifiConnectStartedMs = 0;

enum class LightState {
  Red,
  Green,
  Yellow,
  Unknown,
};

const char* lightStateName(LightState state) {
  switch (state) {
    case LightState::Red:
      return "RED";
    case LightState::Green:
      return "GREEN";
    case LightState::Yellow:
      return "YELLOW";
    case LightState::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

void debugPrint(const String& message) {
  if (!DEBUG_SERIAL) {
    return;
  }

  Serial.print("[CodexLight ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

String maskToken(const String& token) {
  if (token.length() == 0) {
    return "none";
  }
  if (token.length() <= 8) {
    return "<short-token>";
  }
  return token.substring(0, 8) + "..." + token.substring(token.length() - 4) +
         " (len=" + String(token.length()) + ")";
}

String sanitizeCommand(String command) {
  const int tokenStart = command.indexOf("token=");
  if (tokenStart < 0) {
    return command;
  }

  int tokenEnd = command.indexOf(' ', tokenStart);
  if (tokenEnd < 0) {
    tokenEnd = command.length();
  }

  const String token = command.substring(tokenStart + 6, tokenEnd);
  return command.substring(0, tokenStart + 6) + maskToken(token) + command.substring(tokenEnd);
}

const char* wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAILABLE";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
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

String localIpString() {
  return WiFi.localIP().toString();
}

void startWifiConnect() {
  debugPrint("Starting WiFiManager autoConnect AP=" + configPortal.apSsid());
  const bool connected = configPortal.autoConnect();
  wifiConnectStartedMs = millis();
  lastWifiAttemptMs = millis();
  debugPrint(String("WiFiManager autoConnect result=") + (connected ? "connected" : "not_connected"));
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("Wi-Fi connected ssid=" + WiFi.SSID() + " ip=" + localIpString() + " mac=" + WiFi.macAddress() +
               " rssi=" + String(WiFi.RSSI()) + " dBm");
    leds.showGreen();
  } else {
    leds.showYellow();
  }
}

void saveControlToken(const String& token) {
  preferences.begin("codexlight", false);
  preferences.putString("token", token);
  preferences.end();
  controlToken = token;
  debugPrint("Saved UDP token " + maskToken(controlToken));
}

void loadControlToken() {
  preferences.begin("codexlight", true);
  controlToken = preferences.getString("token", "");
  preferences.end();
  debugPrint("Loaded UDP token " + maskToken(controlToken));
}

void clearControlToken() {
  preferences.begin("codexlight", false);
  preferences.remove("token");
  preferences.end();
  controlToken = "";
  debugPrint("Cleared UDP token from NVS");
}

void enterPairingMode(unsigned long durationMs) {
  pairingMode = true;
  pairingModeUntilMs = millis() + durationMs;
  leds.showYellow();
  debugPrint("Entered pairing mode for " + String(durationMs / 1000UL) + " seconds");
}

void maintainPairingMode() {
  if (pairingMode && static_cast<long>(millis() - pairingModeUntilMs) >= 0) {
    pairingMode = false;
    debugPrint("Pairing mode expired");
  }
}

String getValueAfter(String command, const String& key) {
  String upperCommand = command;
  String upperKey = key;
  upperCommand.toUpperCase();
  upperKey.toUpperCase();

  const int keyStart = upperCommand.indexOf(upperKey);
  if (keyStart < 0) {
    return "";
  }

  const int valueStart = keyStart + key.length();
  int valueEnd = command.indexOf(' ', valueStart);
  if (valueEnd < 0) {
    valueEnd = command.length();
  }
  return command.substring(valueStart, valueEnd);
}

String normalizeCommand(String command) {
  command.trim();
  command.toUpperCase();

  constexpr const char* prefix = "CODEXLIGHT/1 ";
  if (command.startsWith(prefix)) {
    command = command.substring(strlen(prefix));
    command.trim();
  }

  return command;
}

LightState parseState(String command) {
  command = normalizeCommand(command);

  if (command == "RED") {
    return LightState::Red;
  }
  if (command == "GREEN") {
    return LightState::Green;
  }
  if (command == "YELLOW") {
    return LightState::Yellow;
  }
  return LightState::Unknown;
}

LightState parseAuthenticatedState(String command, bool requireAuth) {
  command.trim();

  String upperCommand = command;
  upperCommand.toUpperCase();

  constexpr const char* prefix = "CODEXLIGHT/1 ";
  if (!upperCommand.startsWith(prefix)) {
    return requireAuth ? LightState::Unknown : parseState(command);
  }

  command = command.substring(strlen(prefix));
  command.trim();
  upperCommand = command;
  upperCommand.toUpperCase();

  if (controlToken.length() == 0) {
    return requireAuth ? LightState::Unknown : parseState(command);
  }

  const String receivedToken = getValueAfter(command, "token=");
  if (receivedToken != controlToken) {
    debugPrint(
        "Rejected UDP command: token mismatch received=" + maskToken(receivedToken) +
        " expected=" + maskToken(controlToken));
    return LightState::Unknown;
  }

  if (upperCommand.endsWith(" GREEN")) {
    return LightState::Green;
  }
  if (upperCommand.endsWith(" RED")) {
    return LightState::Red;
  }
  if (upperCommand.endsWith(" YELLOW")) {
    return LightState::Yellow;
  }
  return LightState::Unknown;
}

bool handlePairCommand(String command) {
  command.trim();
  String upperCommand = command;
  upperCommand.toUpperCase();

  if (upperCommand == "PAIR") {
    debugPrint("Serial command PAIR received");
    enterPairingMode(60000UL);
    return true;
  }

  if (upperCommand == "CLEAR_TOKEN") {
    debugPrint("Serial command CLEAR_TOKEN received");
    clearControlToken();
    enterPairingMode(120000UL);
    return true;
  }

  if (upperCommand == "CLEAR_WIFI") {
    debugPrint("Serial command CLEAR_WIFI received");
    configPortal.resetSettings();
    WiFi.disconnect(false, true);
    leds.showYellow();
    return true;
  }

  if (upperCommand == "WIFI_RECONNECT") {
    debugPrint("Serial command WIFI_RECONNECT received");
    WiFi.disconnect(false, true);
    startWifiConnect();
    return true;
  }

  constexpr const char* pairPrefix = "CODEXLIGHT/1 PAIR_SET ";
  if (!upperCommand.startsWith(pairPrefix)) {
    return false;
  }

  if (!pairingMode && controlToken.length() > 0) {
    debugPrint("Ignored PAIR_SET: device already has a token and is not in pairing mode");
    return true;
  }

  const String newToken = getValueAfter(command, "token=");
  if (newToken.length() < 16) {
    debugPrint("Ignored PAIR_SET: token is too short, len=" + String(newToken.length()));
    return true;
  }

  saveControlToken(newToken);
  pairingMode = false;
  leds.showGreen();

  if (udpStarted) {
    const String response = "CODEXLIGHT/1 PAIR_OK mac=" + WiFi.macAddress();
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print(response);
    udp.endPacket();
    debugPrint("Sent PAIR_OK to " + udp.remoteIP().toString() + ":" + String(udp.remotePort()) +
               " mac=" + WiFi.macAddress());
  }

  return true;
}

void applyState(LightState state) {
  if (state != LightState::Unknown) {
    debugPrint(String("Apply state ") + lightStateName(state));
  }

  switch (state) {
    case LightState::Red:
      leds.showRed();
      break;
    case LightState::Green:
      leds.showGreen();
      break;
    case LightState::Yellow:
      leds.showYellow();
      break;
    case LightState::Unknown:
      break;
  }
}

void handleCommand(String command, bool requireAuth) {
  if (handlePairCommand(command)) {
    return;
  }

  const LightState state = parseAuthenticatedState(command, requireAuth);
  if (state != LightState::Unknown) {
    applyState(state);
  } else {
    debugPrint(String("Ignored command requireAuth=") + (requireAuth ? "true" : "false") +
               " data=" + sanitizeCommand(command));
  }
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (serialBuffer.length() > 0) {
        debugPrint("Serial RX " + sanitizeCommand(serialBuffer));
        handleCommand(serialBuffer, false);
        serialBuffer = "";
      }
      continue;
    }

    if (serialBuffer.length() < 64) {
      serialBuffer += ch;
    } else {
      serialBuffer = "";
      debugPrint("Serial RX buffer overflow; dropped command");
    }
  }
}

void sendHello() {
  if (!udpStarted) {
    return;
  }

  const String type = pairingMode ? "PAIR_HELLO" : "HELLO";
  const String message = "CODEXLIGHT/1 " + type + " mac=" + WiFi.macAddress();
  udp.beginPacket(IPAddress(255, 255, 255, 255), UDP_PORT);
  udp.print(message);
  udp.endPacket();
  debugPrint("Sent UDP " + type + " mac=" + WiFi.macAddress() + " ip=" + localIpString());
}

void maintainWifi() {
  const wl_status_t status = WiFi.status();
  if (status != lastWifiStatus) {
    lastWifiStatus = status;
    debugPrint(String("Wi-Fi status ") + wifiStatusName(status));
    if (status == WL_CONNECTED) {
      debugPrint("Wi-Fi connected ssid=" + WiFi.SSID() + " ip=" + localIpString() + " mac=" + WiFi.macAddress() +
                 " rssi=" + String(WiFi.RSSI()) + " dBm");
      leds.showGreen();
    }
  }

  if (status == WL_CONNECTED) {
    if (!udpStarted) {
      udpStarted = udp.begin(UDP_PORT) == 1;
      if (udpStarted) {
        lastWirelessPacketMs = millis();
        debugPrint("UDP listener started on port " + String(UDP_PORT));
        sendHello();
      } else {
        debugPrint("UDP listener failed to start on port " + String(UDP_PORT));
      }
    }

    const unsigned long now = millis();
    const unsigned long helloIntervalMs = pairingMode ? 1000UL : 5000UL;
    if (now - lastHelloMs >= helloIntervalMs) {
      lastHelloMs = now;
      sendHello();
    }
    return;
  }

  if (udpStarted) {
    debugPrint("UDP listener stopped because Wi-Fi is not connected");
  }
  udpStarted = false;
  lastHelloMs = 0;

  const unsigned long now = millis();
  if (now - lastWifiAttemptMs < 5000UL) {
    return;
  }

  lastWifiAttemptMs = now;
  debugPrint("Wi-Fi disconnected; use serial WIFI_RECONNECT or reset to reopen WiFiManager portal if needed");
}

void handleUdpInput() {
  if (!udpStarted) {
    return;
  }

  const int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  debugPrint("UDP packet available size=" + String(packetSize) + " from=" +
             udp.remoteIP().toString() + ":" + String(udp.remotePort()));

  char packet[80];
  const int len = udp.read(packet, sizeof(packet) - 1);
  if (len <= 0) {
    debugPrint("UDP read returned no payload");
    return;
  }

  packet[len] = '\0';
  lastWirelessPacketMs = millis();
  debugPrint("UDP RX " + sanitizeCommand(String(packet)));
  handleCommand(String(packet), true);
}

void handleWirelessTimeout() {
  if (!udpStarted || lastWirelessPacketMs == 0) {
    return;
  }

  if (millis() - lastWirelessPacketMs > WIRELESS_TIMEOUT_MS) {
    leds.showYellow();
    debugPrint("Wireless heartbeat timeout; showing YELLOW");
    lastWirelessPacketMs = millis();
  }
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  debugPrint("Booting CodexLight firmware");
  debugPrint("Serial baud " + String(SERIAL_BAUD));
  leds.begin();
  loadControlToken();
  configPortal.begin();
  leds.showGreen();

  if (controlToken.length() == 0) {
    enterPairingMode(120000UL);
  }

  debugPrint("WiFiManager AP " + configPortal.apSsid() + " password=" + String(CONFIG_AP_PASSWORD));
  startWifiConnect();
}

void loop() {
  maintainPairingMode();
  handleSerialInput();
  maintainWifi();
  handleUdpInput();
  handleWirelessTimeout();
}
