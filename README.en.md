# CodexLight

[简体中文](README.md) | English

CodexLight is a Codex status light project based on ESP32-C3 SuperMini. It uses three independent WS2812B LEDs to show the current Codex state, and a desktop-side Bridge monitors local Codex Desktop logs.

Two connection methods are supported:

- Wired USB serial: the desktop bridge automatically finds the ESP32 serial port and sends `GREEN` / `RED` / `YELLOW`
- Wireless UDP: the desktop bridge broadcasts state packets on the LAN, and the ESP32 listens over Wi-Fi

Both methods can be enabled at the same time, or you can choose only one.

## State Mapping

| Light | Codex state |
| --- | --- |
| Green | idle, completed, or no recent agent activity |
| Red | thinking, writing, running tools, or taking action |
| Yellow | waiting for approval or explicit user input |

## Current Features

- PlatformIO firmware project under `Firmware/`.
- Arduino Framework and FastLED are integrated.
- `LedController` wraps LED control so business logic does not call FastLED directly.
- Three independent single-pixel WS2812B LEDs are supported.
- USB serial state commands are supported.
- Wi-Fi UDP state commands are supported.
- Desktop Bridge monitors local Codex JSONL/SQLite logs.
- Desktop Bridge supports automatic serial detection, UDP broadcast, and Win10 tray background mode.
- Firmware has been verified with `pio run`.
- Hardware schematic files are under `Hardware/Schematic/`.


## Project Usage Framework

CodexLight is split into three main parts:

```text
Codex Desktop local logs
        ↓
Desktop Bridge script
        ↓ USB serial / UDP LAN
ESP32-C3 firmware
        ↓
Three independent WS2812B status LEDs
```

Responsibilities:

- `Codex Desktop`: produces local session logs and diagnostic logs.
- `Bridge/`: desktop-side bridge program that monitors Codex logs, derives `GREEN` / `RED` / `YELLOW`, and sends the state to the ESP32 over USB serial or UDP.
- `Firmware/`: ESP32-C3 firmware that receives serial or UDP state commands and controls the three WS2812B LEDs.
- `Hardware/`: schematic and future hardware files.
- `Docs/`: implementation notes and handoff documentation.

Recommended usage:

- Development and debugging: use USB serial first because it is simple and easy to verify.
- Daily use: use Win10 tray background mode with both USB serial and UDP enabled.
- Wireless placement: configure Wi-Fi, complete UDP pairing, then use UDP mode only.

## First-Time Setup, Build, and Upload

After cloning or downloading the project for the first time, follow these steps.

### 1. Install Basic Tools

Required tools:

- Python 3
- PlatformIO
- Git
- ESP32-C3 USB driver, depending on the USB serial chip on your board

For automatic USB serial detection, install pyserial:

```powershell
pip install pyserial
```

If you only use UDP, pyserial is not required on the desktop side.

### 2. Configure Wi-Fi (Only Required for Wireless UDP)

The repository does not commit real Wi-Fi credentials. Copy the example file:

```text
Firmware\include\wifi_secrets.example.h
```

to the local private file:

```text
Firmware\include\wifi_secrets.h
```

Then fill in your own Wi-Fi credentials:

```cpp
#define CODEXLIGHT_WIFI_SSID YourWiFiName
#define CODEXLIGHT_WIFI_PASSWORD YourWiFiPassword
```

`wifi_secrets.h` is ignored by `.gitignore`; do not commit it to GitHub.

If you only use USB serial, you do not need to create `wifi_secrets.h`. The firmware still builds and works over serial.

### 3. Build Firmware

```powershell
cd Firmware
pio run
```

### 4. Upload Firmware

Connect the ESP32-C3 and run:

```powershell
pio run -t upload
```

If PlatformIO does not detect the port automatically, temporarily set `upload_port` in `Firmware/platformio.ini`, or use the PlatformIO device list to identify the port.

### 5. First-Time Wireless Pairing (Only Required for UDP)

A fresh ESP32 without a token enters a pairing window automatically on boot. Run this on the desktop:

```powershell
python Bridge\codex_light_monitor.py --pair --udp-port 4210
```

After pairing succeeds, this local file is generated:

```text
Bridge\config.local.json
```

It stores the UDP token, ESP32 MAC, and recent IP. This is also a local private config file and should not be committed.

### 6. Start the Desktop Bridge

Console mode:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

Win10 tray background mode:

```text
Bridge\start_codex_light_tray.bat
```

Double-clicking it shows a tray icon in the folded notification area at the right side of the taskbar.

### 7. Files Users Usually Modify

Usually only these local files need to be changed:

```text
Firmware\include\wifi_secrets.h     # Wi-Fi SSID and password, local private file
Bridge\config.local.json             # Generated after UDP pairing, local private file
Bridge\start_codex_light_tray.bat    # Optional: choose serial/UDP startup arguments
```

Avoid modifying these unless you are developing features:

```text
Firmware\src\main.cpp
Bridge\codex_light_monitor.py
```

## Hardware Configuration

MCU: ESP32-C3 SuperMini

LEDs: three independent WS2812B LEDs. They are not connected as a chained strip. Each LED has its own data line, and each channel uses `LEDS_PER_CHANNEL = 1`.

| LED | GPIO | Color |
| --- | --- | --- |
| Red | GPIO7 | Red |
| Green | GPIO6 | Green |
| Yellow | GPIO5 | Yellow |

Hardware connection notes:

- Each WS2812B DIN line has a 330 ohm series resistor.
- Each WS2812B has a 100nF decoupling capacitor near its power pins.
- LED power ground and ESP32-C3 ground must be connected together.

## Project Structure

```text
CodexLight/
├── README.md
├── README.en.md
├── .gitignore
├── Bridge/
│   ├── CodexLightTray.ps1
│   ├── README.md
│   ├── README_en.md
│   ├── codex_light_monitor.py
│   └── start_codex_light_tray.bat
├── Docs/
├── Firmware/
│   ├── platformio.ini
│   ├── include/
│   │   ├── config.h
│   │   ├── led.h
│   │   └── wifi_secrets.example.h
│   └── src/
│       ├── main.cpp
│       └── led.cpp
└── Hardware/
    └── Schematic/
        └── Schematic1.pdf
```

Directory overview:

- `Bridge/`: desktop-side bridge program that monitors Codex logs and sends light states.
- `Docs/`: project documentation directory.
- `Firmware/`: ESP32-C3 firmware project.
- `Hardware/`: schematic and future hardware files.

## Desktop Bridge

The bridge script is located at:

```text
Bridge/codex_light_monitor.py
```

It monitors local Codex logs:

```text
C:\Users\<you>\.codex\sessions\YYYY\MM\DD\rollout-*.jsonl
C:\Users\<you>\.codex\logs_2.sqlite
```

### Console Mode

Monitor and print states only:

```powershell
python Bridge\codex_light_monitor.py
```

Use USB serial:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

Use UDP broadcast:

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

Enable both USB serial and UDP:

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

### Win10 Tray Background Mode

Double-click:

```text
Bridge\start_codex_light_tray.bat
```

By default, it enables both wired serial and wireless UDP:

```bat
set "MONITOR_ARGS=--serial auto --baud 115200 --udp --udp-port 4210"
```

The tray icon appears in the folded notification area at the right side of the Win10 taskbar. Right-click it to open the log folder, restart the monitor script, or exit the background program.

### Bridge Protocols

USB serial output:

```text
GREEN
RED
YELLOW
```

UDP output is sent to `255.255.255.255:4210` by default, and the current state is repeated every 2 seconds as a heartbeat. After pairing, UDP packets include a token:

```text
CODEXLIGHT/1 token=<paired-token> GREEN
CODEXLIGHT/1 token=<paired-token> RED
CODEXLIGHT/1 token=<paired-token> YELLOW
```

For first-time wireless UDP use, put the ESP32 into pairing mode, then run:

```powershell
python Bridge\codex_light_monitor.py --pair --udp-port 4210
```

After pairing succeeds, the token is stored in ESP32 NVS, while the desktop stores the token, ESP32 MAC, and recent IP in `Bridge\config.local.json`; it does not need to be hard-coded. During normal operation, the Bridge prefers unicast state packets to the saved ESP32 IP and only accepts matching-MAC `HELLO` packets to refresh that IP.

Serial mode requires pyserial:

```powershell
pip install pyserial
```

UDP mode has no third-party Python dependency.

## Firmware

The firmware project is located in `Firmware/`:

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
lib_deps = fastled/FastLED
```

`Firmware/include/config.h` centralizes hardware and communication parameters:

- `RED_LED_PIN = 7`
- `GREEN_LED_PIN = 6`
- `YELLOW_LED_PIN = 5`
- `LEDS_PER_CHANNEL = 1`
- `DEFAULT_BRIGHTNESS = 64`
- `SERIAL_BAUD = 115200`
- `UDP_PORT = 4210`
- `WIRELESS_TIMEOUT_MS = 10000`

`Firmware/include/led.h` defines the public `LedController` API, including single-state display methods:

```cpp
void showRed();
void showGreen();
void showYellow();
```

`Firmware/src/main.cpp` handles both input paths:

- `GREEN` / `RED` / `YELLOW` from `Serial`
- `CODEXLIGHT/1 GREEN` / `RED` / `YELLOW` from UDP

After receiving a valid state, the firmware lights only the matching LED.

## Wi-Fi Configuration

Wireless UDP mode requires Wi-Fi credentials. Copy:

```text
Firmware\include\wifi_secrets.example.h
```

to:

```text
Firmware\include\wifi_secrets.h
```

Then fill in:

```cpp
#define CODEXLIGHT_WIFI_SSID "YourWiFiName"
#define CODEXLIGHT_WIFI_PASSWORD "YourWiFiPassword"
```

`wifi_secrets.h` is ignored by Git and will not be committed to GitHub. Without this file, the firmware still builds and works over USB serial. The UDP token is stored in ESP32 NVS through pairing and does not need to be written to `wifi_secrets.h`.

## Verification

Desktop script syntax check:

```powershell
python -m py_compile Bridge\codex_light_monitor.py
```

Firmware build:

```powershell
cd Firmware
pio run
```

## Hardware Files

Hardware files are located under `Hardware/`:

- `Hardware/Schematic/Schematic1.pdf`: current schematic file.

## License

MIT
