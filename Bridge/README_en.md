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

The built `CodexLightTray.exe` does not require a separate Python installation. Local Python is needed only when running from source or rebuilding the EXE; source mode also requires:

```powershell
python -m pip install pyserial
```

## Tray Startup

For normal use, double-click the standalone executable:

```text
Bridge\CodexLightTray.exe
```

The EXE embeds a 64-bit Python runtime and pyserial. On first launch it extracts runtime files to `%LOCALAPPDATA%\CodexLight`; logs and local device configuration are stored there as well. Windows Firewall may ask for network access the first time UDP is used; allow private networks.

When running from source, use the hidden launcher. It defaults to `WIRELESS` mode:

```text
Bridge\Source\start_codex_light_tray.vbs
```

The legacy batch launcher is also available, but it may briefly show a console window:

```text
Bridge\Source\start_codex_light_tray.bat
```

Tray menu:

- `Configure WiFi`: write router SSID/password over USB.
- `Connection mode`: switch between `Auto (wired + wireless)`, `Wired only`, and `Wireless only`.
- `Open log folder`: open the active runtime log directory; `%LOCALAPPDATA%\CodexLight\logs` for the EXE and `Bridge\Source\logs` for source mode.
- `Restart monitor`: restart the monitor process.
- `Exit`: quit.

Rebuild the standalone EXE with:

```powershell
powershell -ExecutionPolicy Bypass -File Bridge\Source\build_tray_exe.ps1
```

The output is `Bridge\CodexLightTray.exe`. The build uses local 64-bit Python and the Windows .NET Framework C# compiler, and does not require network access.

## Wi-Fi Provisioning

The tray `Configure WiFi` action pauses the monitor process, opens serial, and sends:

```text
WIFI_SET <ssid><TAB><password>
```

On success, the device replies:

```text
WIFI_SET_OK <ssid> <ip>
```

EXE failure logs are written to:

```text
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.err.log
```

Source-mode failure logs are written under `Bridge\Source\logs`.

Command-line provisioning:

```powershell
python Bridge\Source\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

## Run Modes

| Mode | Bridge behavior |
| --- | --- |
| `WIRED` | Opens serial and sends states only over USB |
| `WIRELESS` | Sends states over UDP; if USB exists, serial saves `MODE WIRELESS` and is released; if USB is absent, the saved firmware mode is used |
| `AUTO` | Enables serial and UDP; firmware prefers a fresh serial heartbeat |

Manual commands:

```powershell
python Bridge\Source\codex_light_monitor.py --serial auto --baud 115200
python Bridge\Source\codex_light_monitor.py --udp --udp-port 4210
python Bridge\Source\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
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

- EXE build: `%LOCALAPPDATA%\CodexLight\logs`
- Source mode: `Bridge\Source\logs`
- Both contain `codex_light_monitor.out.log`, `codex_light_monitor.err.log`, `wifi_setup.out.log`, and `wifi_setup.err.log`.

## Verification

```powershell
python -B -m py_compile Bridge\Source\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\Source\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
```

## License

The Bridge is covered by the repository's [MIT License](../LICENSE).
