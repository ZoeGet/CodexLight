# CodexLight Usage Guide

English | [简体中文](USAGE.md) | [Project Home](README.en.md)

This guide covers firmware upload, USB Wi-Fi provisioning, wired/wireless/AUTO modes, Windows tray operation, serial commands, UDP protocol, troubleshooting, and verification.

## Prerequisites

Desktop requirements:

- Windows 10/11
- Python 3.9+
- Codex Desktop
- `pyserial`

```powershell
python -m pip install pyserial
```

Firmware development requires PlatformIO Core or the VS Code PlatformIO IDE. PlatformIO installs `Adafruit NeoPixel` automatically.

## Build and Upload

```powershell
cd C:\path\to\CodexLight\Firmware
pio run
pio run -t upload --upload-port COM4
```

Replace `COM4` with the actual ESP32-C3 port. Open the serial monitor:

```powershell
pio device monitor --port COM4 --baud 115200
```

When no Wi-Fi is configured, expected output is similar to:

```text
CODEXLIGHT READY
WIFI_PROVISIONING USB_SERIAL
[WiFi] No saved Wi-Fi credentials; waiting for USB provisioning
WIFI_USB_PROVISIONING READY FORMAT=WIFI_SET <ssid><TAB><password>
STATUS mode=AUTO active=NONE wifi=DISCONNECTED ... network=USB_PROVISIONING radio=OFF
```

After Wi-Fi is configured, expected output is similar to:

```text
CODEXLIGHT READY
WIFI_PROVISIONING USB_SERIAL
[WiFi] Connecting to YourWifi
WIFI_CONNECTED YourWifi 192.168.x.x
STATUS mode=AUTO active=NONE wifi=CONNECTED ... radio=STA ip=192.168.x.x
```

PlatformIO Monitor and the Bridge cannot use the same COM port at the same time. Close the monitor before starting the Bridge.

## USB Wi-Fi Provisioning

The current firmware no longer uses a phone AP portal. Use the tray menu:

1. Connect CodexLight to the computer over USB.
2. Start the tray by double-clicking `Bridge\start_codex_light_tray.vbs`.
3. Right-click the tray icon and choose `Configure WiFi`.
4. Enter the router SSID and password.
5. Click `Save`.

On success, the tray shows `WiFi saved and connected.` The ESP32 stores the credentials in NVS and reconnects automatically on later boots.

Command-line provisioning is also available:

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

Successful output:

```text
DEVICE WIFI_SET_OK YourWifi 192.168.x.x
```

Failure details are written to:

```text
Bridge\logs\wifi_setup.out.log
Bridge\logs\wifi_setup.err.log
```

The firmware saves credentials only after a successful connection. Wrong passwords, missing SSIDs, or timeouts do not overwrite the saved configuration.

## Windows Tray

Recommended launcher:

```text
Bridge\start_codex_light_tray.vbs
```

This starts the tray without leaving a PowerShell window open. Do not close the hosting PowerShell process directly; right-click the tray icon and choose `Exit`.

The legacy batch launcher is still available but may briefly show a console window:

```text
Bridge\start_codex_light_tray.bat
```

Startup mode can be selected explicitly:

```powershell
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

The tray menu provides:

- `Configure WiFi`: configure Wi-Fi over USB.
- `Connection mode`: switch between `Auto`, `Wired only`, and `Wireless only`.
- `Open log folder`: open Bridge logs.
- `Restart monitor`: restart the Bridge monitor process.
- `Exit`: quit the tray app.

## Wired, Wireless, and AUTO Modes

| Mode | Behavior | Recommended use |
| --- | --- | --- |
| `AUTO` | Bridge sends over USB and UDP; firmware prefers a fresh USB heartbeat and can fall back to UDP after USB expires | Daily use |
| `WIRED` | USB serial only | Debugging, firmware validation, unstable Wi-Fi |
| `WIRELESS` | Wi-Fi UDP only; USB is used only to save mode or configure Wi-Fi | Cable-free placement |

Switch modes from the tray icon under `Connection mode`. The Bridge restarts its internal monitor and sends one of:

```text
MODE AUTO
MODE WIRED
MODE WIRELESS
```

The selected mode is stored in ESP32 NVS and survives normal firmware uploads.

## Manual Bridge Commands

Wired:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

Wireless:

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

Both transports:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
```

Wireless mode requires the computer and ESP32 to be on the same LAN, with router AP/client isolation disabled. Windows Firewall must allow Python to use UDP port `4210`.

## LED Behavior

- No valid desktop heartbeat: GPIO5 yellow blinks.
- First desktop connection: GPIO6 green blinks for two seconds.
- Codex reasoning, responding, or running tools: GPIO7 red stays on.
- Waiting for approval, permission, or user input: GPIO5 yellow stays on.
- `task_complete` or `turn_aborted`: GPIO6 green stays on.
- No heartbeat on the active transport for six seconds: returns to blinking yellow.

## Serial Commands

| Command | Effect |
| --- | --- |
| `GREEN` | Set wired state to green and refresh heartbeat |
| `RED` | Set wired state to red and refresh heartbeat |
| `YELLOW` | Set wired state to yellow and refresh heartbeat |
| `PING` | Refresh wired heartbeat and reply with `PONG` |
| `STATUS` | Print mode, active transport, Wi-Fi state, IP, and diagnostics |
| `MODE WIRED` | Use USB only and persist mode |
| `MODE WIRELESS` | Use UDP only and persist mode |
| `MODE AUTO` | Accept USB and UDP and persist mode |
| `WIFI_CONFIG` | Print the USB provisioning hint |
| `WIFI_SET <ssid><TAB><password>` | Configure Wi-Fi over USB; save only after connection succeeds |
| `CLEAR_WIFI` | Clear saved Wi-Fi and wait for provisioning |

## UDP Protocol

Bridge sends:

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
```

The ESP32 broadcasts discovery every two seconds:

```text
CODEXLIGHT/1 HELLO mac=AA:BB:CC:DD:EE:FF mode=AUTO
```

The default UDP port is `4210`.

## Troubleshooting

### Wi-Fi setup fails

- Confirm USB is connected and PlatformIO Monitor is not using the COM port.
- Use the detailed failure text shown by the tray.
- Check `Bridge/logs/wifi_setup.out.log` and `Bridge/logs/wifi_setup.err.log`.
- ESP32-C3 supports 2.4 GHz Wi-Fi only.
- Use the latest tray version if SSID/password contains spaces or special characters; it passes credentials through a temporary JSON file.

### Wireless mode does not respond

- Confirm serial output shows `WIFI_CONNECTED <ssid> <ip>`.
- Put the computer and device on the same LAN.
- Allow Python through Windows Firewall for UDP `4210`.
- Delete `Bridge/config.local.json` to force rediscovery.
- Switch to `AUTO` and plug in USB to verify the device receives states.

### Yellow keeps blinking

- The Bridge is not running, or the wrong connection mode is selected.
- The COM port is held by PlatformIO Monitor.
- In wireless mode, the computer and device are not on the same LAN.
- Run `STATUS` and inspect `mode`, `active`, `wifi`, `network`, and `radio`.

### Full factory reset

```powershell
cd Firmware
pio run -t erase --upload-port COM4
pio run -t upload --upload-port COM4
```

## Development Verification

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
cd Firmware
pio run
```

For architecture details, see [Docs/USAGE_AND_IMPLEMENTATION.md](Docs/USAGE_AND_IMPLEMENTATION.md).
