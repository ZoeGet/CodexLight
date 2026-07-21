# CodexLight Usage and Implementation Guide

English | [简体中文](使用与实现说明.md) | [Documentation Index](README.md) | [Project Home](../README.en.md)

This document combines the operating guide and technical implementation reference for firmware flashing, USB Wi-Fi provisioning, wired/wireless operation, troubleshooting, and maintenance.

## Hardware Connections

| LED | Data pin | State |
| --- | --- | --- |
| Yellow WS2812B | GPIO5 | Disconnected or waiting for approval |
| Green WS2812B | GPIO6 | Connection animation or task completed |
| Red WS2812B | GPIO7 | Codex is working |

Each WS2812B uses an independent DIN. Connect VCC to a stable 5 V supply and share ground with the ESP32-C3.

## Build and Upload

```powershell
cd C:\path\to\CodexLight\Firmware
pio run
pio run -t upload --upload-port COM4
pio device monitor --port COM4 --baud 115200
```

Close PlatformIO Monitor before starting the Bridge because both use the same serial port.

## USB Wi-Fi Provisioning

The current firmware does not open a SoftAP provisioning portal. Provisioning is done over USB serial.

Tray workflow:

1. Connect the device over USB.
2. Start `Bridge\start_codex_light_tray.vbs`.
3. Right-click the tray icon and choose `Configure WiFi`.
4. Enter SSID/password and save.

Command-line workflow:

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

Firmware command:

```text
WIFI_SET <ssid><TAB><password>
```

Credentials are saved only after `WiFi.status() == WL_CONNECTED`. Failed attempts are not persisted.

## Transport Modes

| Mode | Behavior |
| --- | --- |
| `WIRED` | USB serial heartbeat only |
| `WIRELESS` | UDP heartbeat only |
| `AUTO` | USB and UDP accepted; USB heartbeat has priority when fresh |

Mode commands:

```text
MODE WIRED
MODE WIRELESS
MODE AUTO
STATUS
```

The mode is stored under Preferences namespace `codexlight`, key `transport`.

## Windows Tray

Use the hidden launcher:

```text
Bridge\start_codex_light_tray.vbs
```

The tray hosts these user actions:

- `Configure WiFi`: USB provisioning.
- `Connection mode`: switches between AUTO, WIRED, and WIRELESS.
- `Open log folder`: opens `Bridge/logs`.
- `Restart monitor`: restarts `codex_light_monitor.py`.
- `Exit`: stops the tray app.

The legacy batch file remains available for explicit mode startup:

```powershell
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

## Architecture

```text
Codex Desktop logs
        |
        v
Bridge/codex_light_monitor.py
        |
        +-- USB CDC serial: GREEN / RED / YELLOW / MODE / WIFI_SET
        +-- LAN UDP: CODEXLIGHT/1 <STATE>
                         |
                         v
                  ESP32-C3 firmware
                         |
                         +-- Preferences: wifi ssid/password, transport mode
                         +-- GPIO5 yellow WS2812B
                         +-- GPIO6 green WS2812B
                         +-- GPIO7 red WS2812B
```

## Bridge Implementation

The Bridge reads:

- `~/.codex/sessions/**/*.jsonl`
- `~/.codex/logs_2.sqlite`

It sends only state colors to the device.

| Codex event | Output |
| --- | --- |
| `task_started`, messages, reasoning, tool calls, tool outputs | `RED` |
| Approval, permission, or explicit user input needed | `YELLOW` |
| `task_complete` or `turn_aborted` | `GREEN` |

The tray Wi-Fi setup path pauses the monitor, runs a one-shot `--wifi-config` child process, writes logs to `Bridge/logs/wifi_setup.*.log`, then restarts the monitor.

## Firmware Implementation

Main files:

- `Firmware/src/main.cpp`: serial command parser, UDP heartbeat, transport selection, LED state machine.
- `Firmware/src/config_portal.cpp`: STA Wi-Fi connection and USB provisioning wrapper. The name is retained for compatibility with older code.
- `Firmware/src/storage.cpp`: Preferences storage for Wi-Fi credentials.
- `Firmware/src/led.cpp`: three independent NeoPixel outputs.
- `Firmware/include/config.h`: GPIO, brightness, timeouts, UDP port, default mode.

Startup flow:

```text
setup
  -> read saved Wi-Fi credentials
  -> if present, try STA connection for 15 seconds
  -> if connected, start normal runtime
  -> if missing or failed, turn Wi-Fi off and wait for USB WIFI_SET
loop
  -> handle serial commands
  -> maintain UDP when Wi-Fi is connected
  -> select active transport
  -> render LED state
```

## Protocol

Serial commands:

```text
GREEN
RED
YELLOW
PING
STATUS
MODE WIRED
MODE WIRELESS
MODE AUTO
WIFI_CONFIG
WIFI_SET <ssid><TAB><password>
CLEAR_WIFI
```

UDP states and discovery:

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
CODEXLIGHT/1 HELLO mac=<MAC> mode=<MODE>
```

## Troubleshooting

- Wi-Fi setup fails: check `Bridge/logs/wifi_setup.out.log` and `.err.log`, close PlatformIO Monitor, verify 2.4 GHz Wi-Fi.
- Wireless does not update: confirm same LAN, allow UDP 4210 through firewall, delete `Bridge/config.local.json` to rediscover.
- Yellow keeps blinking: Bridge is not running, mode is wrong, serial is occupied, or UDP discovery cannot reach the device.
- Colors are wrong: verify `NEO_GRB`, DIN orientation, GPIO continuity, and 5 V power.

## Security and Local State

- Wi-Fi credentials are stored in ESP32 NVS and are not printed over serial.
- UDP is unauthenticated and unencrypted; use only on trusted LANs.
- Do not commit `Bridge/config.local.json`, `Bridge/logs/`, Wi-Fi passwords, or local device IPs.

## Verification

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
cd Firmware
pio run
```

## License

The project is distributed under the repository [MIT License](../LICENSE).
