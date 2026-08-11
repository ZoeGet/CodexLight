# CodexLight Bridge

English | [简体中文](README.md) | [Project Home](../README.en.md)

The Bridge runs on the Windows computer hosting Codex Desktop. It reads local Codex session logs, maps activity to `GREEN`, `RED`, or `YELLOW`, and sends the state to the ESP32-C3 over USB serial, LAN UDP, or both.

## Features

- Monitors `~/.codex/sessions/**/*.jsonl` and `~/.codex/logs_2.sqlite`.
- Supports USB serial, UDP, and mixed AUTO mode.
- Provides a Windows tray menu for Wi-Fi setup, mode switching, logs, monitor restart, and exit.
- Configures device Wi-Fi over USB serial. The firmware no longer uses an ESP32 AP portal.
- Remembers the most recently discovered UDP device for wireless startup.
- When no computer USB is connected, the tray skips serial setup and continues UDP using the device's saved wireless mode.

## State Rules

| Codex event | Output |
| --- | --- |
| `task_started`, reasoning, messages, tool calls, and tool outputs | `RED` |
| A tool call requires approval, permission, or user input | `YELLOW` |
| `task_complete` or `turn_aborted` | `GREEN` |

The Bridge supports both `function_call` / `function_call_output` and `custom_tool_call` / `custom_tool_call_output`. When a tool call contains `require_escalated`, `sandbox_permissions`, `request_user_input`, or another approval/permission marker, the state remains `YELLOW` until the matching tool output arrives.

The Bridge sends only color states. It never sends Codex message text, tool output, API keys, or login tokens to the device.

## Dependency

```powershell
python -m pip install pyserial
```

## Tray Startup

Recommended hidden launcher. It defaults to `WIRELESS` mode:

```text
Bridge\start_codex_light_tray.vbs
```

The legacy batch launcher is also available, but it may briefly show a console window:

```text
Bridge\start_codex_light_tray.bat
```

Tray menu:

- `Configure WiFi`: write router SSID/password over USB.
- `Connection mode`: switch between `Auto (wired + wireless)`, `Wired only`, and `Wireless only`.
- `Open log folder`: open `Bridge/logs`.
- `Restart monitor`: restart the monitor process.
- `Exit`: quit.

## Wi-Fi Provisioning

The tray `Configure WiFi` action pauses the monitor process, opens serial, and sends:

```text
WIFI_SET <ssid><TAB><password>
```

On success, the device replies:

```text
WIFI_SET_OK <ssid> <ip>
```

Failure logs are written to:

```text
Bridge\logs\wifi_setup.out.log
Bridge\logs\wifi_setup.err.log
```

Command-line provisioning:

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

## Run Modes

| Mode | Bridge behavior |
| --- | --- |
| `WIRED` | Opens serial and sends states only over USB |
| `WIRELESS` | Sends states over UDP; if USB exists, serial saves `MODE WIRELESS` and is released; if USB is absent, the saved firmware mode is used |
| `AUTO` | Enables serial and UDP; firmware prefers a fresh serial heartbeat |

Manual commands:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
python Bridge\codex_light_monitor.py --udp --udp-port 4210
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
```

## Common Options

| Option | Description |
| --- | --- |
| `--serial COM4` | Use a specific serial port |
| `--serial auto` | Auto-select a common ESP32/USB serial device |
| `--baud 115200` | Serial baud rate |
| `--udp` | Enable UDP state output and discovery |
| `--udp-port 4210` | UDP port |
| `--firmware-mode AUTO` | Persist firmware mode over serial |
| `--serial-setup-only` | Use serial only for mode setup, then release it |
| `--reset-on-connect` | Pulse ESP32 reset lines after opening serial; used by wireless tray startup to initialize the device |
| `--wifi-ssid` / `--wifi-password` | One-shot USB Wi-Fi provisioning |
| `--wifi-config path.json` | Read `{ "ssid": "...", "password": "..." }` from JSON |

## Logs

- `Bridge/logs/codex_light_monitor.out.log`
- `Bridge/logs/codex_light_monitor.err.log`
- `Bridge/logs/wifi_setup.out.log`
- `Bridge/logs/wifi_setup.err.log`

## Verification

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
```

## License

The Bridge is covered by the repository's [MIT License](../LICENSE).
