# CodexLight Implementation Status

[Documentation Index](README.md) | [Project Home](../README.en.md) | [中文实现说明](当前实现说明.md)

This document is the technical handoff for the current CodexLight implementation. The root READMEs remain the primary user-facing source of truth.

## Architecture

```text
Codex Desktop logs
        |
        v
Bridge/codex_light_monitor.py
        |
        +-- USB CDC serial: GREEN / RED / YELLOW
        +-- LAN UDP: CODEXLIGHT/1 <STATE>
                         |
                         v
                  ESP32-C3 firmware
                         |
                         +-- GPIO5 yellow WS2812B
                         +-- GPIO6 green WS2812B
                         +-- GPIO7 red WS2812B
```

## Desktop Bridge

Entry point: `Bridge/codex_light_monitor.py`

Primary inputs:

- `~/.codex/sessions/**/*.jsonl`
- `~/.codex/logs_2.sqlite`

State mapping:

| Event | State |
| --- | --- |
| `task_started` | `RED` |
| `agent_message`, `agent_reasoning`, `token_count` | `RED` |
| `function_call`, `function_call_output` | `RED` |
| Tool payload contains approval, permission, or user-input markers | `YELLOW` |
| `task_complete` | `GREEN` |
| `turn_aborted` | `GREEN` |

An active turn does not become green because of `item/completed` or a quiet timeout. `quiet_timeout` is a fallback only when no active turn exists.

When serial opens, the Bridge waits two seconds, sends `MODE WIRED` for serial-only operation or `MODE AUTO` when UDP is also enabled, and sends the current state. Serial and UDP states repeat every two seconds as firmware heartbeats.

UDP begins with broadcast discovery. After receiving a matching `HELLO`, the Bridge records the MAC and IP in the Git-ignored `Bridge/config.local.json` and prefers unicast.

## Firmware

Entry point: `Firmware/src/main.cpp`

Modules:

- `src/config_portal.cpp`: non-blocking WiFiManager provisioning
- `src/led.cpp`: three independent Adafruit NeoPixel outputs
- `include/config.h`: user-facing constants

Each loop iteration:

1. Parses USB serial commands.
2. Calls WiFiManager `process()`.
3. Maintains UDP listening and `HELLO` discovery packets.
4. Selects an active transport from the configured mode and fresh heartbeats.
5. Updates the connection animation or actual LED state.

There is no infinite Wi-Fi wait and no `while (!Serial)`, so standalone and wired operation remain available without a network.

## Transport Modes

- `WIRED`: considers `lastWiredPacketMs` only.
- `WIRELESS`: considers `lastWirelessPacketMs` only.
- `AUTO`: accepts both and prefers a fresh wired heartbeat.

The mode is stored in NVS using Preferences namespace `codexlight`, key `transport`. The source default is `WIRED`. A normal firmware upload does not erase NVS.

## Heartbeat and LED Timing

- Bridge heartbeat interval: 2 seconds
- Firmware link timeout: 6 seconds
- Disconnected blink half-period: 500 ms
- Connection animation duration: 2 seconds
- Connection animation half-period: 250 ms

The LED driver uses `NEO_GRB + NEO_KHZ800`, matching the reference project. Every state transition writes all three independent LED outputs so inactive LEDs are explicitly cleared.

The firmware caches the last rendered LED frame. WS2812B data is sent only when the state or blink phase changes. A `task_complete` green frame therefore remains latched until a later `RED`, `YELLOW`, or disconnected frame replaces it.

## Wi-Fi Provisioning

WiFiManager opens `CodexLight-XXXX` at `192.168.4.1` when no usable credentials exist. The current AP password is `123456789`.

- `WIFI_CONFIG` calls `startConfigPortal()`.
- `CLEAR_WIFI` resets saved settings, disconnects, and reopens the portal.

The ESP32-C3 ultimately operates as a Wi-Fi station on the user's 2.4 GHz LAN. The phone-facing AP is a provisioning mechanism, not the permanent desktop-to-device network.

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
CLEAR_WIFI
```

UDP states:

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
CODEXLIGHT/1 PING
```

Discovery:

```text
CODEXLIGHT/1 HELLO mac=<MAC> mode=<MODE>
```

## Removed Legacy Behavior

The current implementation no longer uses:

- Hard-coded `wifi_secrets.h` credentials
- A custom blocking configuration page
- UDP token pairing or `PAIR_SET`
- FastLED global multi-controller refresh
- Ordinary `item/completed` logs as a full task-completion signal

## Verification

```powershell
python -B -c "import ast,pathlib; ast.parse(pathlib.Path('Bridge/codex_light_monitor.py').read_text(encoding='utf-8')); print('OK')"

cd Firmware
pio run -j 1
```

The latest NeoPixel firmware build completed successfully. It used approximately 65.5% flash and 12.3% RAM; exact values may change with dependency versions.

## Maintenance Rules

- Keep root and subdirectory documentation synchronized in Chinese and English.
- Do not commit `Bridge/config.local.json`, Wi-Fi credentials, or device-specific IP addresses.
- Preserve non-blocking serial operation when modifying provisioning.
- Treat `task_complete` and `turn_aborted` as the definitive green transitions.
- Rebuild firmware after changing GPIOs, pixel order, or provisioning constants.

## License

The project is distributed under the repository's [MIT License](../LICENSE). Third-party dependencies retain their own licenses.
