# CodexLight Usage Guide

English | [简体中文](USAGE.md) | [Project Home](README.en.md)

This guide covers firmware upload, USB Wi-Fi provisioning, wired/wireless/AUTO modes, Windows tray operation, serial commands, UDP protocol, standalone power, troubleshooting, and verification.

## Prerequisites

Desktop requirements:

- Windows 10/11
- Codex Desktop

`Bridge\dist\CodexLightTray.exe` does not require Python. Python 3.9+ and `pyserial` are needed only when running the Bridge from source.

Firmware development requires PlatformIO Core or the VS Code PlatformIO IDE. PlatformIO installs `Adafruit NeoPixel` automatically.

## Build and Upload

```powershell
cd C:\path\to\CodexLight\Firmware
pio run
pio run -t upload --upload-port COM4
```

Replace `COM4` with the actual ESP32-C3 port. Open the serial monitor with:

```powershell
pio device monitor --port COM4 --baud 115200
```

PlatformIO Monitor, serial terminal apps, and the Bridge cannot use the same COM port at the same time. Close the monitor before starting the tray.

## USB Wi-Fi Provisioning

The current firmware no longer opens an ESP32 AP portal. Use the tray:

1. Connect CodexLight to the computer over USB.
2. Start the tray by double-clicking `Bridge\dist\CodexLightTray.exe`. It defaults to `WIRELESS` mode.
3. Right-click the tray icon and choose `Configure WiFi`.
4. Enter the 2.4 GHz router SSID and password.
5. Click `Save`.

On success, the tray shows `WiFi saved and connected.` The device saves credentials only after a successful connection. Wrong passwords, missing SSIDs, or timeouts do not overwrite the previous saved configuration.

Command-line provisioning is also available:

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

Successful output:

```text
DEVICE WIFI_SET_OK YourWifi 192.168.x.x
```

EXE failure logs:

```text
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.err.log
```

Source-mode logs remain under `Bridge\logs`.

## Windows Tray

Recommended standalone executable:

```text
Bridge\dist\CodexLightTray.exe
```

The EXE embeds a 64-bit Python runtime and `pyserial`, shows no console window, and defaults to `WIRELESS` mode. On first launch it extracts its runtime, logs, and local configuration to `%LOCALAPPDATA%\CodexLight`. To exit, right-click the tray icon and choose `Exit`.

Windows Firewall may request network access when UDP is first enabled; allow private networks. The source launchers remain available:

```text
Bridge\start_codex_light_tray.vbs
Bridge\start_codex_light_tray.bat
```

Startup mode can be selected explicitly:

```powershell
Bridge\dist\CodexLightTray.exe auto
Bridge\dist\CodexLightTray.exe wired
Bridge\dist\CodexLightTray.exe wireless
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

Tray menu:

- `Configure WiFi`: configure Wi-Fi over USB.
- `Connection mode`: switch between `Auto`, `Wired only`, and `Wireless only`.
- `Open log folder`: open Bridge logs.
- `Restart monitor`: restart the Bridge monitor process.
- `Exit`: quit the tray app.

## Wired, Wireless, and AUTO Modes

| Mode | Behavior | Recommended use |
| --- | --- | --- |
| `AUTO` | Bridge sends over USB and UDP; firmware prefers a fresh USB heartbeat and can fall back to UDP after USB expires | Daily debugging or mixed connection |
| `WIRED` | USB serial only | Firmware validation, serial debugging, unstable Wi-Fi |
| `WIRELESS` | Wi-Fi UDP only; USB is used only for provisioning or mode setup | Cable-free placement |

The selected mode is stored in ESP32 NVS and survives normal firmware uploads.

## Wireless-Only Workflow

1. Plug into the computer over USB and complete `Configure WiFi` once.
2. From the tray, choose `Connection mode` -> `Wireless only`.
3. Quit the tray.
4. Unplug the computer USB and power the device from a power bank or stable battery-powered 5 V supply.
5. Wait 20 to 60 seconds and check the LED pattern.
6. Start the tray.
7. If the log shows `UDP ack ... active=WIRELESS state=...`, wireless mode is working.

When there is no computer USB connection, these log lines are expected:

```text
SERIAL no matching serial port
SERIAL setup skipped; using saved firmware mode.
```

## LED Behavior and Diagnostics

| LED pattern | Meaning |
| --- | --- |
| Solid green | Idle, task complete, or task aborted |
| Solid red | Codex is reasoning, responding, or running tools |
| Solid yellow | Waiting for approval, permission, or user input |
| Green blink for 2 seconds | First desktop connection established |
| Slow yellow blink | Wi-Fi is connected, but no desktop USB/UDP heartbeat has arrived |
| Alternating red/yellow | No saved Wi-Fi credentials; provision over USB |
| Repeating red double-blink | Saved Wi-Fi exists, but Wi-Fi is reconnecting or failed |

## Manual Bridge Commands

Wired:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

Wireless:

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

AUTO:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
```

Wireless mode requires the computer and ESP32 to be on the same LAN, with router AP/client isolation disabled. Windows Firewall must allow Python to use UDP port `4210`.

## Serial Commands

| Command | Effect |
| --- | --- |
| `GREEN` | Set wired state to green and refresh heartbeat |
| `RED` | Set wired state to red and refresh heartbeat |
| `YELLOW` | Set wired state to yellow and refresh heartbeat |
| `PING` | Refresh wired heartbeat and reply with `PONG` |
| `STATUS` | Print mode, active transport, Wi-Fi, IP, and UDP diagnostics |
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
CODEXLIGHT/1 PING
```

ESP32 replies:

```text
CODEXLIGHT/1 ACK mac=<MAC> mode=<MODE> active=<TRANSPORT> state=<STATE>
```

ESP32 also broadcasts discovery:

```text
CODEXLIGHT/1 HELLO mac=<MAC> mode=<MODE>
```

The default UDP port is `4210`.

## Troubleshooting

### Wi-Fi setup fails

- Confirm USB is connected and PlatformIO Monitor or another serial terminal is not using the COM port.
- ESP32-C3 supports 2.4 GHz Wi-Fi only.
- For the EXE build, check `%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log` and `.err.log`; source-mode logs are under `Bridge/logs`.
- The current firmware uses the ESP32-C3 SDK default Wi-Fi transmit power, governed by PHY and country configuration.

### Wireless mode does not respond

- Slow yellow blink means Wi-Fi is connected and the device is waiting for tray UDP; start the tray.
- Repeating red double-blink means Wi-Fi is reconnecting or failed; check router, SSID/password, and 2.4 GHz availability.
- If `ping 192.168.x.x` fails and `arp -a` does not show the device MAC, the device is not online on the LAN.
- Restart the tray to force device rediscovery.
- Allow Python through Windows Firewall for UDP `4210`.

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
