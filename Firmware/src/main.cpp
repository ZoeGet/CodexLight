#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "config.h"
#include "config_portal.h"
#include "led.h"

namespace {

using namespace CodexLightConfig;

enum class TransportMode {
  Auto,
  Wired,
  Wireless,
};

enum class Transport {
  None,
  Wired,
  Wireless,
};

enum class LightState {
  Green,
  Red,
  Yellow,
};

enum class LedFrame {
  Off,
  Green,
  Red,
  Yellow,
};

LedController leds;
ConfigPortal configPortal;
Preferences preferences;
WiFiUDP udp;

TransportMode transportMode = TransportMode::Auto;
Transport activeTransport = Transport::None;
LightState wiredState = LightState::Green;
LightState wirelessState = LightState::Green;

String serialBuffer;
bool udpStarted = false;
unsigned long lastWiredPacketMs = 0;
unsigned long lastWirelessPacketMs = 0;
unsigned long lastHelloMs = 0;
bool linkWasConnected = false;
bool connectionAnimationActive = false;
unsigned long connectionAnimationStartedMs = 0;
LedFrame displayedFrame = LedFrame::Off;

void debugPrint(const String& message) {
  if (!DEBUG_SERIAL) {
    return;
  }
  Serial.print("[CodexLight ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

const char* modeName(TransportMode mode) {
  switch (mode) {
    case TransportMode::Auto:
      return "AUTO";
    case TransportMode::Wired:
      return "WIRED";
    case TransportMode::Wireless:
      return "WIRELESS";
  }
  return "AUTO";
}

const char* transportName(Transport transport) {
  switch (transport) {
    case Transport::Wired:
      return "WIRED";
    case Transport::Wireless:
      return "WIRELESS";
    case Transport::None:
      return "NONE";
  }
  return "NONE";
}

TransportMode parseMode(String value) {
  value.trim();
  value.toUpperCase();
  if (value == "WIRED") {
    return TransportMode::Wired;
  }
  if (value == "WIRELESS") {
    return TransportMode::Wireless;
  }
  return TransportMode::Auto;
}

bool parseState(String command, LightState& state) {
  command.trim();
  command.toUpperCase();
  if (command.endsWith(" GREEN")) {
    command = "GREEN";
  } else if (command.endsWith(" RED")) {
    command = "RED";
  } else if (command.endsWith(" YELLOW")) {
    command = "YELLOW";
  }

  if (command == "GREEN") {
    state = LightState::Green;
    return true;
  }
  if (command == "RED") {
    state = LightState::Red;
    return true;
  }
  if (command == "YELLOW") {
    state = LightState::Yellow;
    return true;
  }
  return false;
}

void showFrame(LedFrame frame) {
  if (frame == displayedFrame) {
    return;
  }

  switch (frame) {
    case LedFrame::Off:
      leds.allOff();
      break;
    case LedFrame::Green:
      leds.showGreen();
      break;
    case LedFrame::Red:
      leds.showRed();
      break;
    case LedFrame::Yellow:
      leds.showYellow();
      break;
  }
  displayedFrame = frame;
}

void showState(LightState state) {
  switch (state) {
    case LightState::Green:
      showFrame(LedFrame::Green);
      break;
    case LightState::Red:
      showFrame(LedFrame::Red);
      break;
    case LightState::Yellow:
      showFrame(LedFrame::Yellow);
      break;
  }
}

bool packetIsFresh(unsigned long packetMs, unsigned long now) {
  return packetMs != 0 && now - packetMs <= LINK_TIMEOUT_MS;
}

Transport selectActiveTransport(unsigned long now) {
  const bool wiredFresh = packetIsFresh(lastWiredPacketMs, now);
  const bool wirelessFresh = packetIsFresh(lastWirelessPacketMs, now);

  if (transportMode == TransportMode::Wired) {
    return wiredFresh ? Transport::Wired : Transport::None;
  }
  if (transportMode == TransportMode::Wireless) {
    return wirelessFresh ? Transport::Wireless : Transport::None;
  }
  if (wiredFresh) {
    return Transport::Wired;
  }
  if (wirelessFresh) {
    return Transport::Wireless;
  }
  return Transport::None;
}

void saveMode() {
  preferences.begin("codexlight", false);
  preferences.putString("transport", modeName(transportMode));
  preferences.end();
}

void loadMode() {
  preferences.begin("codexlight", false);
  const String saved = preferences.getString("transport", DEFAULT_TRANSPORT_MODE);
  preferences.end();
  transportMode = parseMode(saved);
}

void printStatus() {
  Serial.print("STATUS mode=");
  Serial.print(modeName(transportMode));
  Serial.print(" active=");
  Serial.print(transportName(activeTransport));
  Serial.print(" wifi=");
  Serial.print(configPortal.wifiConnected() ? "CONNECTED" : "DISCONNECTED");
  if (configPortal.wifiConnected()) {
    Serial.print(" ip=");
    Serial.print(WiFi.localIP());
  }
  Serial.println();
}

void handleControlCommand(String command) {
  command.trim();
  String upper = command;
  upper.toUpperCase();

  if (upper.startsWith("MODE ")) {
    transportMode = parseMode(upper.substring(5));
    saveMode();
    lastWiredPacketMs = 0;
    lastWirelessPacketMs = 0;
    Serial.println(String("MODE_OK ") + modeName(transportMode));
    return;
  }
  if (upper == "STATUS") {
    printStatus();
    return;
  }
  if (upper == "WIFI_CONFIG") {
    configPortal.start();
    Serial.println(String("WIFI_PORTAL ") + configPortal.apSsid() + " 192.168.4.1");
    return;
  }
  if (upper == "CLEAR_WIFI") {
    configPortal.resetSettings();
    Serial.println(String("WIFI_CLEARED ") + configPortal.apSsid() + " 192.168.4.1");
    return;
  }

  LightState state;
  if (upper == "PING") {
    lastWiredPacketMs = millis();
    Serial.println("PONG");
    return;
  }
  if (parseState(upper, state)) {
    wiredState = state;
    lastWiredPacketMs = millis();
  }
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (serialBuffer.length() > 0) {
        handleControlCommand(serialBuffer);
        serialBuffer = "";
      }
      continue;
    }
    if (serialBuffer.length() < 96) {
      serialBuffer += ch;
    } else {
      serialBuffer = "";
    }
  }
}

void sendHello() {
  if (!udpStarted) {
    return;
  }
  const String message = "CODEXLIGHT/1 HELLO mac=" + WiFi.macAddress() +
                         " mode=" + modeName(transportMode);
  udp.beginPacket(IPAddress(255, 255, 255, 255), UDP_PORT);
  udp.print(message);
  udp.endPacket();
}

void maintainUdp() {
  if (!configPortal.wifiConnected()) {
    if (udpStarted) {
      udp.stop();
      udpStarted = false;
    }
    return;
  }

  if (!udpStarted) {
    udpStarted = udp.begin(UDP_PORT) == 1;
    if (udpStarted) {
      debugPrint("UDP listening on " + String(UDP_PORT) + " at " + WiFi.localIP().toString());
      sendHello();
    }
  }

  if (!udpStarted) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastHelloMs >= 2000UL) {
    lastHelloMs = now;
    sendHello();
  }

  const int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  char packet[128];
  const int length = udp.read(packet, sizeof(packet) - 1);
  if (length <= 0) {
    return;
  }
  packet[length] = '\0';

  String command(packet);
  LightState state;
  String upper = command;
  upper.trim();
  upper.toUpperCase();
  if (upper == "PING" || upper == "CODEXLIGHT/1 PING") {
    lastWirelessPacketMs = now;
  } else if (parseState(upper, state)) {
    wirelessState = state;
    lastWirelessPacketMs = now;
  }
}

void updateLeds() {
  const unsigned long now = millis();
  activeTransport = selectActiveTransport(now);
  const bool connected = activeTransport != Transport::None;

  if (connected && !linkWasConnected) {
    connectionAnimationActive = true;
    connectionAnimationStartedMs = now;
    debugPrint(String("Computer link connected via ") + transportName(activeTransport));
  } else if (!connected && linkWasConnected) {
    connectionAnimationActive = false;
    debugPrint("Computer link disconnected");
  }
  linkWasConnected = connected;

  if (!connected) {
    const bool on = (now / DISCONNECTED_BLINK_HALF_PERIOD_MS) % 2 == 0;
    if (on) {
      showFrame(LedFrame::Yellow);
    } else {
      showFrame(LedFrame::Off);
    }
    return;
  }

  if (connectionAnimationActive) {
    if (now - connectionAnimationStartedMs < CONNECTED_ANIMATION_MS) {
      const bool on = ((now - connectionAnimationStartedMs) /
                       CONNECTED_BLINK_HALF_PERIOD_MS) % 2 == 0;
      if (on) {
        showFrame(LedFrame::Green);
      } else {
        showFrame(LedFrame::Off);
      }
      return;
    }
    connectionAnimationActive = false;
  }

  showState(activeTransport == Transport::Wired ? wiredState : wirelessState);
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println("CODEXLIGHT READY");

  leds.begin();
  loadMode();
  configPortal.begin();
  debugPrint(String("Boot mode=") + modeName(transportMode));
  debugPrint(String("Config AP=") + configPortal.apSsid() + " password=" + CONFIG_AP_PASSWORD);
  printStatus();
}

void loop() {
  handleSerialInput();
  configPortal.process();
  maintainUdp();
  updateLeds();
  delay(2);
}
