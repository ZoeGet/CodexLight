# CodexLight Bridge

English | [简体中文](README.md) | [Project Home](../README.en.md)

The Bridge runs on the Windows computer hosting Codex Desktop. It continuously reads local Codex session logs, maps activity to `GREEN`, `RED`, or `YELLOW`, and sends the state to the ESP32-C3 over USB serial, UDP, or both.

## State Rules

| Codex event | Output |
| --- | --- |
| `task_started`, reasoning, messages, tool calls, and tool outputs | `RED` |
| A tool call requires approval, permission, or user input | `YELLOW` |
| `task_complete` or `turn_aborted` | `GREEN` |

An active task does not turn green because of an ordinary `item/completed` event or a short period without logs. After detecting an approval request, the Bridge latches `YELLOW` against parallel tool calls, unrelated tool outputs, and SQLite diagnostics until the matching approval call returns. After `task_complete` or `turn_aborted`, the Bridge latches `GREEN` and ignores trailing log or diagnostic events from the completed turn until the next `task_started` or `user_message`.

## Requirements

- Windows 10/11
- Python 3.9+
- Codex Desktop
- `pyserial` for wired mode

```powershell
python -m pip install pyserial
```

## Run Modes

Run these commands from the repository root.

### Wired Serial

```powershell
python Bridge\codex_light_monitor.py --serial COM4 --baud 115200
```

Automatic selection of common ESP32 serial ports:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

After opening the port, the Bridge waits two seconds. When only serial is enabled, it retries `MODE WIRED` until the ESP32 replies with `MODE_OK WIRED`, ensuring that the firmware has switched modes and accepts wired states. It then repeats the current state every two seconds as a heartbeat.

### Wireless UDP

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

The computer and ESP32 must be on the same LAN. The Bridge initially broadcasts to `255.255.255.255:4210`. After receiving an ESP32 `HELLO`, it records the device MAC and IP and prefers unicast.

Discovery state is stored in:

```text
Bridge/config.local.json
```

The file is ignored by Git. Delete it to force rediscovery.

### Enable Serial and UDP

```powershell
python Bridge\codex_light_monitor.py --serial COM4 --baud 115200 --udp --udp-port 4210
```

When serial and UDP are enabled together, the Bridge automatically sends `MODE AUTO`. The firmware accepts both heartbeat sources and prefers a valid wired connection.

### Windows Tray

Double-click:

```text
Bridge\start_codex_light_tray.bat
```

Double-clicking selects `AUTO`, enabling automatic serial and UDP while firmware prefers a valid wired connection. A mode can also be selected explicitly from PowerShell:

```powershell
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

| Mode | Bridge arguments | Behavior |
| --- | --- | --- |
| `auto` | Serial + UDP | Default; firmware prefers a fresh wired connection |
| `wired` | Serial only | Bridge sends and confirms `MODE WIRED` |
| `wireless` | UDP states; serial setup only | With USB available, saves `MODE WIRELESS` and immediately releases serial; otherwise uses the saved firmware mode |

Serial port, baud rate, and UDP port are configured at the top of the batch file through `SERIAL_PORT`, `SERIAL_BAUD`, and `UDP_PORT`. The tray tooltip and status row display the selected mode. The `Connection mode` submenu switches directly between `Auto`, `Wired only`, and `Wireless only`, restarting the Bridge automatically. In wireless mode, an available USB connection is used only to complete the `MODE WIRELESS` handshake; serial is then closed and state traffic continues over UDP. The tray menu can also open `Bridge/logs`, restart the Bridge, or exit.

## Common Options

| Option | Default | Description |
| --- | --- | --- |
| `--serial COM4` | disabled | Use a specific serial port |
| `--serial auto` | disabled | Select a common ESP32/USB serial device automatically |
| `--baud` | `115200` | Serial baud rate |
| `--firmware-mode` | inferred | Negotiate `AUTO`, `WIRED`, or `WIRELESS` over serial |
| `--serial-setup-only` | disabled | Use serial only to save mode, then release it after acknowledgement |
| `--udp` | disabled | Enable UDP state output and discovery |
| `--udp-host` | `255.255.255.255` | UDP target before discovery |
| `--udp-port` | `4210` | UDP port |
| `--udp-interval` | `2.0` | Shared serial and UDP heartbeat interval |
| `--device-mac` | empty | Accept discovery only from this ESP32 MAC |
| `--sessions-root` | `~/.codex/sessions` | Codex JSONL session directory |
| `--sqlite` | `~/.codex/logs_2.sqlite` | Codex diagnostic log database |
| `--poll` | `0.5` | Log polling interval |
| `--max-age-days` | `2` | Age window for session files |
| `--quiet-timeout` | `20` | Green fallback when no task is active |
| `--from-start` | disabled | Process existing JSONL content on startup |
| `--repeat` | disabled | Print repeated states to the console |

Show all options:

```powershell
python Bridge\codex_light_monitor.py --help
```

`--complete-grace` is retained for compatibility; `task_complete` now controls the green state.

## Protocol

### Serial

One ASCII state per line:

```text
GREEN
RED
YELLOW
```

### UDP

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
```

ESP32 discovery packet:

```text
CODEXLIGHT/1 HELLO mac=AA:BB:CC:DD:EE:FF mode=WIRELESS
```

## Verification

Syntax check without creating `__pycache__`:

```powershell
python -B -c "import ast,pathlib; ast.parse(pathlib.Path('Bridge/codex_light_monitor.py').read_text(encoding='utf-8')); print('OK')"
```

Example foreground output:

```text
2026-07-17 13:19:18 SERIAL connected COM4
2026-07-17 13:19:20 GREEN  startup
2026-07-17 13:20:07 RED    reasoning
2026-07-17 13:20:45 YELLOW approval_pending
2026-07-17 13:21:02 GREEN  task_complete
```

## Troubleshooting

- `SERIAL connect failed`: close PlatformIO Monitor or any other program using the COM port.
- `SERIAL no matching serial port`: specify a port explicitly with `--serial COM4`.
- Wireless state does not update: check LAN membership, firewall access to UDP 4210, and `WIRELESS`/`AUTO` firmware mode.
- Discovery selects the wrong unit: pass `--device-mac AA:BB:CC:DD:EE:FF`.
- State behavior looks outdated: stop old Bridge processes and restart the latest script.

## Security

UDP traffic is neither encrypted nor authenticated. Use it only on a trusted LAN.

## License

The Bridge is covered by the repository's [MIT License](../LICENSE).
