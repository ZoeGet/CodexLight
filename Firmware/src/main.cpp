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
unsigned long lastUdpPacketMs = 0;
unsigned long udpPacketCount = 0;
String lastUdpText;
bool linkWasConnected = false;
bool connectionAnimationActive = false;
unsigned long connectionAnimationStartedMs = 0;
LedFrame displayedFrame = LedFrame::Off;
unsigned long lastLedRefreshMs = 0;
bool ledsInitialized = false;

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

const char* stateName(LightState state) {
  switch (state) {
    case LightState::Green:
      return "GREEN";
    case LightState::Red:
      return "RED";
    case LightState::Yellow:
      return "YELLOW";
  }
  return "GREEN";
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

void showFrame(LedFrame frame, unsigned long now) {
  const bool frameChanged = frame != displayedFrame;
  const bool refreshDue = now - lastLedRefreshMs >= LED_REFRESH_INTERVAL_MS;
  if (!frameChanged && !refreshDue) {
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
  lastLedRefreshMs = now;
}

void showState(LightState state, unsigned long now) {
  switch (state) {
    case LightState::Green:
      showFrame(LedFrame::Green, now);
      break;
    case LightState::Red:
      showFrame(LedFrame::Red, now);
      break;
    case LightState::Yellow:
      showFrame(LedFrame::Yellow, now);
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

void saveMode(TransportMode mode) {
  transportMode = mode;
  saveMode();
  lastWiredPacketMs = 0;
  lastWirelessPacketMs = 0;
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
  Serial.print(" sta_status=");
  Serial.print(configPortal.wifiStatusName());
  Serial.print(" target_ssid=");
  if (configPortal.configuredSsid().length() > 0) {
    Serial.print(configPortal.configuredSsid());
  } else {
    Serial.print("NONE");
  }
  Serial.print(" disconnect_reason=");
  Serial.print(configPortal.lastDisconnectReason());
  Serial.print(" network=");
  Serial.print(configPortal.stateName());
  Serial.print(" radio=");
  switch (WiFi.getMode()) {
    case WIFI_MODE_NULL:
      Serial.print("OFF");
      break;
    case WIFI_MODE_STA:
      Serial.print("STA");
      break;
    case WIFI_MODE_AP:
      Serial.print("AP");
      break;
    case WIFI_MODE_APSTA:
      Serial.print("AP_STA");
      break;
    default:
      Serial.print("UNKNOWN");
      break;
  }
  if (configPortal.portalActive()) {
    Serial.print(" ap=");
    Serial.print(WiFi.softAPSSID());
    Serial.print(" ap_ip=");
    Serial.print(WiFi.softAPIP());
    Serial.print(" ap_clients=");
    Serial.print(WiFi.softAPgetStationNum());
  }
  if (configPortal.wifiConnected()) {
    Serial.print(" ip=");
    Serial.print(WiFi.localIP());
  }
  Serial.print(" udp_packets=");
  Serial.print(udpPacketCount);
  Serial.print(" last_udp_ms=");
  Serial.print(lastUdpPacketMs);
  if (lastUdpText.length() > 0) {
    Serial.print(" last_udp=");
    Serial.print(lastUdpText);
  }
  Serial.println();
}

void handleControlCommand(String command) {
  command.trim();
  String upper = command;
  upper.toUpperCase();

  if (upper.startsWith("MODE ")) {
    saveMode(parseMode(upper.substring(5)));
    Serial.println(String("MODE_OK ") + modeName(transportMode));
    return;
  }
  if (upper == "STATUS") {
    printStatus();
    return;
  }
  if (upper == "WIFI_CONFIG") {
    Serial.println("WIFI_USB_PROVISIONING READY FORMAT=WIFI_SET <ssid><TAB><password>");
    return;
  }
  if (upper == "CLEAR_WIFI") {
    configPortal.resetSettings();
    Serial.println("WIFI_CLEARED USB_PROVISIONING");
    return;
  }
  if (upper.startsWith("WIFI_SET ")) {
    const int separator = command.indexOf('\t', 9);
    if (separator < 0) {
      Serial.println("WIFI_SET_ERROR FORMAT");
      return;
    }
    String ssid = command.substring(9, separator);
    const String password = command.substring(separator + 1);
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64 ||
        (password.length() > 0 && password.length() < 8)) {
      Serial.println("WIFI_SET_ERROR INVALID_CREDENTIALS");
      return;
    }
    configPortal.configure(ssid, password);
    if (configPortal.wifiConnected()) {
      saveMode(TransportMode::Wireless);
      Serial.println(String("WIFI_SET_OK ") + ssid + " " + WiFi.localIP().toString());
    } else {
      Serial.println(String("WIFI_SET_ERROR CONNECT_FAILED ") + ssid);
    }
    return;
  }
  if (upper == "LED_TEST RED") {
    showFrame(LedFrame::Red, millis());
    Serial.println("LED_TEST_OK RED");
    return;
  }
  if (upper == "LED_TEST GREEN") {
    showFrame(LedFrame::Green, millis());
    Serial.println("LED_TEST_OK GREEN");
    return;
  }
  if (upper == "LED_TEST YELLOW") {
    showFrame(LedFrame::Yellow, millis());
    Serial.println("LED_TEST_OK YELLOW");
    return;
  }
  if (upper == "LED_TEST OFF") {
    showFrame(LedFrame::Off, millis());
    Serial.println("LED_TEST_OK OFF");
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
    Serial.println(String("STATE_OK ") + stateName(state));
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
    if (serialBuffer.length() < 128) {
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

void sendUdpAck(IPAddress remoteIp, uint16_t remotePort) {
  if (!udpStarted) {
    return;
  }
  const String message = "CODEXLIGHT/1 ACK mac=" + WiFi.macAddress() +
                         " mode=" + modeName(transportMode) +
                         " active=" + transportName(activeTransport) +
                         " state=" + stateName(activeTransport == Transport::Wired ? wiredState : wirelessState);
  udp.beginPacket(remoteIp, remotePort);
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

  const IPAddress remoteIp = udp.remoteIP();
  const uint16_t remotePort = udp.remotePort();

  char packet[128];
  const int length = udp.read(packet, sizeof(packet) - 1);
  if (length <= 0) {
    return;
  }
  packet[length] = '\0';

  String command(packet);
  lastUdpPacketMs = now;
  udpPacketCount += 1;
  lastUdpText = command;
  lastUdpText.trim();
  LightState state;
  String upper = command;
  upper.trim();
  upper.toUpperCase();
  if (upper == "PING" || upper == "CODEXLIGHT/1 PING") {
    lastWirelessPacketMs = now;
    activeTransport = selectActiveTransport(now);
    sendUdpAck(remoteIp, remotePort);
  } else if (parseState(upper, state)) {
    wirelessState = state;
    lastWirelessPacketMs = now;
    activeTransport = selectActiveTransport(now);
    sendUdpAck(remoteIp, remotePort);
  }
}

void showNoWifiConfig(unsigned long now) {
  const bool redPhase = (now / 1000UL) % 2 == 0;
  showFrame(redPhase ? LedFrame::Red : LedFrame::Yellow, now);
}

void showWifiReconnect(unsigned long now) {
  const unsigned long phase = now % 2000UL;
  if (phase < 200UL || (phase >= 400UL && phase < 600UL)) {
    showFrame(LedFrame::Red, now);
  } else {
    showFrame(LedFrame::Off, now);
  }
}

void showWaitingForComputer(unsigned long now) {
  const bool on = (now / DISCONNECTED_BLINK_HALF_PERIOD_MS) % 2 == 0;
  showFrame(on ? LedFrame::Yellow : LedFrame::Off, now);
}

void updateLeds() {
  if (!ledsInitialized) {
    return;
  }

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
    if (configPortal.wifiConnected()) {
      showWaitingForComputer(now);
    } else if (configPortal.configuredSsid().length() > 0) {
      showWifiReconnect(now);
    } else {
      showNoWifiConfig(now);
    }
    return;
  }

  if (connectionAnimationActive) {
    if (now - connectionAnimationStartedMs < CONNECTED_ANIMATION_MS) {
      const bool on = ((now - connectionAnimationStartedMs) /
                       CONNECTED_BLINK_HALF_PERIOD_MS) % 2 == 0;
      if (on) {
        showFrame(LedFrame::Green, now);
      } else {
        showFrame(LedFrame::Off, now);
      }
      return;
    }
    connectionAnimationActive = false;
  }

  showState(activeTransport == Transport::Wired ? wiredState : wirelessState, now);
}

void maintainLedController() {
  if (ledsInitialized) {
    return;
  }

  leds.begin();
  ledsInitialized = true;
  displayedFrame = LedFrame::Off;
  lastLedRefreshMs = 0;
  debugPrint("LED controller enabled after Wi-Fi provisioning");
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println("CODEXLIGHT READY");

  maintainLedController();
  showFrame(LedFrame::Yellow, millis());

  configPortal.begin();
  Serial.println("WIFI_PROVISIONING USB_SERIAL");
  Serial.flush();
  configPortal.autoConnect();
  if (configPortal.wifiConnected()) {
    Serial.println(String("WIFI_CONNECTED ") + WiFi.SSID() + " " + WiFi.localIP().toString());
    maintainUdp();
  } else {
    Serial.println("WIFI_USB_PROVISIONING READY FORMAT=WIFI_SET <ssid><TAB><password>");
  }

  loadMode();
  debugPrint(String("Boot mode=") + modeName(transportMode));
  debugPrint(String("Config AP=") + configPortal.apSsid());
  printStatus();
}

void loop() {
  configPortal.loop();
  handleSerialInput();
  maintainLedController();
  maintainUdp();
  updateLeds();
  delay(2);
}
