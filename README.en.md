# CodexLight

English | [简体中文](README.md) | [English Usage Guide](USAGE.en.md) | [中文使用说明](USAGE.md)

CodexLight is an ESP32-C3FH4 status light for Codex Desktop. A Windows Bridge reads local Codex session logs, maps activity to `GREEN`, `RED`, or `YELLOW`, and sends the state to the device over USB serial, LAN UDP, or both. The firmware drives three independent WS2812B LEDs for idle, working, and waiting states.

The custom all-in-one board integrates the ESP32-C3FH4, a 2.4 GHz antenna, a 40 MHz crystal, native USB Type-C, battery charging and 5 V boost power, and a 3.3 V regulator. No separate ESP32-C3 development board is required.

This is an independent community project and is not officially affiliated with or endorsed by OpenAI.

## Current Design

- Wi-Fi provisioning uses USB serial. The firmware no longer opens an ESP32 AP portal.
- The tray app provides `Configure WiFi`, which sends SSID/password to the device over USB.
- Wi-Fi credentials are saved to ESP32 NVS only after a successful connection. Bad credentials do not overwrite the previous working setup.
- Saved Wi-Fi connects non-blockingly at boot and keeps retrying after disconnects.
- ESP32-C3 Wi-Fi transmit power defaults to 8.5 dBm to improve wireless stability on the current hardware.
- Standalone operation does not depend on USB serial. The device can run from its battery connector or a stable 5 V supply in wireless mode.
- Persistent `AUTO`, `WIRED`, and `WIRELESS` transport modes are supported.
- `Bridge/start_codex_light_tray.vbs` starts the tray hidden and defaults to wireless mode.

## State Mapping

| State | Codex condition | LED | GPIO |
| --- | --- | --- | --- |
| `GREEN` | Idle, task completed, or task aborted | Green | GPIO6 |
| `RED` | Reasoning, responding, running tools, or processing a task | Red | GPIO7 |
| `YELLOW` | Waiting for approval, permission, or explicit user input | Yellow | GPIO5 |
| No desktop heartbeat | No valid USB/UDP heartbeat for 6 seconds | Slow yellow blink | GPIO5 |

When the first valid desktop heartbeat arrives, the green LED blinks for two seconds as a connection indication, then the real state is shown. Completed green stays latched until the next task starts, a waiting state appears, or the connection is lost.

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

3. Start the hidden tray launcher. It defaults to wireless mode:

   ```text
   Bridge\start_codex_light_tray.vbs
   ```

4. For first-time setup, connect the device with a data-capable Type-C cable, right-click the tray icon, choose `Configure WiFi`, and enter the router SSID/password.
5. After Wi-Fi is saved, unplug the computer USB and power the device from a stable 5 V supply or a protected single-cell Li-ion battery connected through MX1.25. The tray controls the light over UDP.

See [USAGE.en.md](USAGE.en.md) for the full workflow.

## Wireless Operation

1. Complete `Configure WiFi` once over USB.
2. Use `Wireless only`, or start the default `start_codex_light_tray.vbs` launcher.
3. Power the complete device from a stable 5 V supply or a protected single-cell Li-ion battery connected through MX1.25, without connecting USB to the computer.
4. The tray log should show:

   ```text
   SERIAL no matching serial port
   SERIAL setup skipped; using saved firmware mode.
   UDP ack from 192.168.x.x CODEXLIGHT/1 ACK ... active=WIRELESS state=...
   ```

This means there is no computer serial link and the device is working over Wi-Fi UDP.

## Standalone LED Diagnostics

| LED pattern | Meaning |
| --- | --- |
| Alternating red/yellow | No saved Wi-Fi credentials; provision over USB |
| Repeating red double-blink | Saved Wi-Fi exists, but the device is reconnecting or failed to connect |
| Slow yellow blink | Wi-Fi is connected and the device is waiting for UDP heartbeats |
| Normal red/green/yellow | Desktop state is being received |

## Hardware

<img src="Hardware/Images/schematic.png" width="800" />

The current hardware is a custom all-in-one board built around the ESP32-C3FH4. See [Hardware/Schematic/Schematic.pdf](Hardware/Schematic/Schematic.pdf) for the complete schematic.

| Function block | Main components and connections |
| --- | --- |
| MCU and clock | ESP32-C3FH4 with an external 40 MHz crystal; RST drives CHIP_EN and BOOT drives GPIO9 |
| Wireless RF | AN9520-245 2.4 GHz antenna with an R9/C11/C12 matching footprint |
| USB Type-C | Native USB D+/D- connect to GPIO19/GPIO18 through 22 Ω resistors; CC1 and CC2 each use a 5.1 kΩ pull-down |
| USB power | VBUS feeds the system +5 V rail through a BAT60JFILM Schottky diode |
| Battery power | ETA9697E8A provides single-cell linear charging and 5 V boost power with a 2.2 µH inductor, MX1.25 battery connector, and power switch |
| 3.3 V rail | TLV75733PDBVR regulates +5 V down to +3.3 V for the ESP32-C3FH4 and logic circuitry |
| Status LEDs | Yellow on GPIO5, green on GPIO6, and red on GPIO7; each data line has a 330 Ω resistor and each LED has 100 nF decoupling |

The three WS2812B LEDs use independent data inputs; they are not a chained strip. The LEDs run from +5 V while the ESP32-C3FH4 runs from +3.3 V, and both rails must share ground. The firmware uses `NEO_GRB + NEO_KHZ800`. Default brightness is `DEFAULT_BRIGHTNESS = 50` and can be configured in [Firmware/include/config.h](Firmware/include/config.h).

The schematic leaves the ETA9697E8A NTC and STAT pins without external temperature sensing or a status indicator. Verify battery connector polarity, use a protected cell, and calculate the charge current from R5 and the selected battery capacity. See the [open-source platform project description](Docs/立创开源平台项目说明.md) for detailed circuit and assembly notes.

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

## Verification

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
cd Firmware
pio run
```

## License

This project is licensed under the [MIT License](LICENSE). Third-party dependencies remain subject to their own licenses.
