# CodexLight Usage and Implementation Guide

English | [简体中文](使用与实现说明.md) | [Documentation Index](README.md) | [Project Home](../README.en.md)

This document combines the operating guide and technical implementation reference for firmware flashing, USB Wi-Fi provisioning, wired/wireless operation, troubleshooting, and maintenance.

## Hardware Connections

| LED | Data pin | State |
| --- | --- | --- |
| Yellow WS2812B | GPIO5 | Disconnected, waiting, or diagnostics |
| Green WS2812B | GPIO6 | Connection animation or task completed |
| Red WS2812B | GPIO7 | Codex is working or Wi-Fi reconnect diagnostics |

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
2. Double-click `Bridge\dist\CodexLightTray.exe`. It requires no separate Python installation and defaults to `WIRELESS` mode.
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

Normal users double-click the standalone EXE, which defaults to `WIRELESS` mode:

```text
Bridge\dist\CodexLightTray.exe
```

The EXE embeds a 64-bit Python runtime and `pyserial` and extracts them to `%LOCALAPPDATA%\CodexLight` on first launch. Allow private-network access if Windows Firewall prompts. Source debugging can still use `Bridge\start_codex_light_tray.vbs`.

The tray hosts these user actions:

- `Configure WiFi`: USB provisioning.
- `Connection mode`: switches between AUTO, WIRED, and WIRELESS.
- `Open log folder`: opens the active log directory; `%LOCALAPPDATA%\CodexLight\logs` for the EXE and `Bridge/logs` for source mode.
- `Restart monitor`: restarts `codex_light_monitor.py`.
- `Exit`: stops the tray app.

The wireless tray path opens serial only for setup when USB exists, sends a reset pulse and `MODE WIRELESS`, then releases the port. If no USB serial port exists, it logs `SERIAL setup skipped; using saved firmware mode.` and continues over UDP.

## Architecture

```text
Codex Desktop logs
        |
        v
Bridge/codex_light_monitor.py
        |
        +-- USB CDC serial: GREEN / RED / YELLOW / MODE / WIFI_SET
        +-- LAN UDP: CODEXLIGHT/1 <STATE> and CODEXLIGHT/1 PING
                         |
                         v
                  ESP32-C3 firmware
                         |
                         +-- Preferences: wifi ssid/password, transport mode
                         +-- non-blocking saved Wi-Fi reconnect
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

Session parsing accepts both legacy `function_call` / `function_call_output` and current `custom_tool_call` / `custom_tool_call_output` events. Calls containing `require_escalated`, `sandbox_permissions`, `request_user_input`, or another approval/permission marker remain `YELLOW` until the matching output event resolves the wait.

The tray Wi-Fi setup path pauses the monitor, runs a one-shot `--wifi-config` child process, then restarts the monitor. The EXE writes `wifi_setup.*.log` under `%LOCALAPPDATA%\CodexLight\logs`; source mode writes it under `Bridge/logs`.

## Firmware Implementation

Main files:

- `Firmware/src/main.cpp`: serial command parser, UDP heartbeat, transport selection, LED state machine, diagnostics.
- `Firmware/src/config_portal.cpp`: STA Wi-Fi connection, USB provisioning wrapper, non-blocking saved Wi-Fi reconnect.
- `Firmware/src/storage.cpp`: Preferences storage for Wi-Fi credentials.
- `Firmware/src/led.cpp`: three independent NeoPixel outputs.
- `Firmware/include/config.h`: GPIO, brightness, timeouts, UDP port, default mode, and ESP32-C3 Wi-Fi transmit power.

Wi-Fi notes:

- USB serial is the only provisioning method.
- Credentials are stored only after the STA interface reaches `WL_CONNECTED`.
- Saved Wi-Fi starts non-blockingly at boot, so LED diagnostics and the main loop continue even when the AP is unavailable.
- Failed saved Wi-Fi connections keep credentials and retry every 10 seconds.
- ESP32-C3 uses the SDK default transmit power governed by PHY and country configuration.
- Default debug serial output is disabled for standalone wireless operation.

Startup flow:

```text
setup
  -> initialize LEDs and show startup yellow
  -> read saved Wi-Fi credentials
  -> if present, start STA connection without blocking the main loop
  -> load persisted transport mode
loop
  -> continue saved Wi-Fi reconnect attempts when needed
  -> handle USB serial commands when available
  -> maintain UDP when Wi-Fi is connected
  -> select active transport
  -> render LED state or standalone diagnostics
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
LED_TEST RED
LED_TEST GREEN
LED_TEST YELLOW
LED_TEST OFF
```

UDP states, pings, ACKs, and discovery:

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
CODEXLIGHT/1 PING
CODEXLIGHT/1 ACK mac=<MAC> mode=<MODE> active=<TRANSPORT> state=<STATE>
CODEXLIGHT/1 HELLO mac=<MAC> mode=<MODE>
```

## Troubleshooting

- Wi-Fi setup fails: for the EXE, check `%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log` and `.err.log`; for source mode, check `Bridge/logs`. Also close PlatformIO Monitor and verify 2.4 GHz Wi-Fi.
- Wireless does not update: confirm same LAN, allow UDP 4210 through firewall, and restart the tray to rediscover the device.
- Slow yellow blink: Wi-Fi is connected and the firmware is waiting for desktop heartbeats; start the tray.
- Red double-blink: saved Wi-Fi exists but connection is still failing or reconnecting.
- Colors are wrong: verify `NEO_GRB`, DIN orientation, GPIO continuity, and 5 V power.

## Security

- Wi-Fi credentials are stored in ESP32 NVS and are not printed over serial.
- UDP is unauthenticated and unencrypted; use only on trusted LANs.

## Verification

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -ExecutionPolicy Bypass -File Bridge\build_tray_exe.ps1
cd Firmware
pio run
```

## License

The project is distributed under the repository [MIT License](../LICENSE).
