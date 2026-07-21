# CodexLight

English | [简体中文](README.md) | [English Usage Guide](USAGE.en.md) | [中文使用说明](USAGE.md)

CodexLight is an ESP32-C3 status light for Codex Desktop. A desktop Bridge reads local Codex session logs and sends the current state to the ESP32-C3 over USB serial, LAN UDP, or both. The firmware drives three independent WS2812B LEDs for working, waiting, and completed states.

This is an independent community project and is not officially affiliated with or endorsed by OpenAI.

## Current Design

- Wi-Fi provisioning is done over USB serial. The firmware no longer opens an ESP32 AP provisioning portal.
- The tray app provides `Configure WiFi`, which sends SSID/password to the device over USB.
- The device saves Wi-Fi credentials only after a successful connection. Failed credentials are not persisted.
- Persistent `AUTO`, `WIRED`, and `WIRELESS` transport modes are supported.
- The Windows tray can be started without a PowerShell window by double-clicking `Bridge/start_codex_light_tray.vbs`.

## State Mapping

| State | Codex condition | LED | GPIO |
| --- | --- | --- | --- |
| `GREEN` | Idle, task completed, or task aborted | Green | GPIO6 |
| `RED` | Reasoning, responding, running tools, or processing a task | Red | GPIO7 |
| `YELLOW` | Waiting for approval, permission, or explicit user input | Yellow | GPIO5 |
| Disconnected | No valid desktop heartbeat for 6 seconds | Blinking yellow | GPIO5 |

When the first valid desktop heartbeat arrives, the green LED blinks for two seconds as a connection indication. After a task completes, green remains latched until another task starts, the Bridge enters a waiting state, or the connection is lost.

## Quick Start

1. Install the desktop dependency:

   ```powershell
   python -m pip install pyserial
   ```

2. Build and upload the firmware:

   ```powershell
   cd Firmware
   pio run
   pio run -t upload --upload-port COM4
   ```

3. Start the hidden tray launcher:

   ```text
   Bridge\start_codex_light_tray.vbs
   ```

4. Right-click the tray icon, choose `Configure WiFi`, and enter the router SSID and password.

5. Right-click the tray icon and choose a connection mode:

   - `Auto (wired + wireless)`: recommended daily mode; wired takes priority, wireless is available as fallback.
   - `Wired only`: USB serial only.
   - `Wireless only`: Wi-Fi UDP only.

See [USAGE.en.md](USAGE.en.md) for the full workflow.

## Hardware

Recommended parts:

- ESP32-C3 SuperMini or a compatible ESP32-C3 development board
- Three WS2812B RGB LEDs
- A stable 5 V power supply
- A USB data cable

Wiring:

| Function | ESP32-C3 pin |
| --- | --- |
| Yellow LED DIN | GPIO5 |
| Green LED DIN | GPIO6 |
| Red LED DIN | GPIO7 |
| WS2812B VCC | 5 V |
| WS2812B GND | GND |

The LEDs use three independent data inputs; they are not a chained strip. The current firmware uses `NEO_GRB + NEO_KHZ800`. Global brightness is configured by `DEFAULT_BRIGHTNESS` in [Firmware/include/config.h](Firmware/include/config.h).

## Repository Layout

```text
CodexLight/
├── Bridge/        # Windows log monitor, serial/UDP sender, and tray app
├── Firmware/      # ESP32-C3 PlatformIO firmware
├── Hardware/      # BOM, schematic, PCB, Gerber, enclosure STL files
├── Docs/          # Usage and implementation notes
├── README.md      # Chinese project documentation
├── README.en.md   # English project documentation
├── USAGE.md       # Chinese usage guide
└── USAGE.en.md    # English usage guide
```

## Documentation

- [中文使用说明](USAGE.md)
- [English Usage Guide](USAGE.en.md)
- [Bridge 说明](Bridge/README.md)
- [Bridge English Guide](Bridge/README_en.md)
- [Firmware Guide](Firmware/README.md)
- [使用与实现说明](Docs/使用与实现说明.md)
- [Usage and Implementation Guide](Docs/USAGE_AND_IMPLEMENTATION.md)

## Security

- UDP control is neither encrypted nor authenticated; use it only on a trusted LAN.
- Wi-Fi credentials are written to ESP32 NVS over USB serial; firmware does not print passwords.
- Do not commit `Bridge/config.local.json`, `Bridge/logs/`, Wi-Fi passwords, device IPs, or other local configuration.

## Verification

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
cd Firmware
pio run
```

## License

This project is licensed under the [MIT License](LICENSE). Third-party dependencies remain subject to their own licenses.
