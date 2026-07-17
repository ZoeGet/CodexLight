# CodexLight

CodexLight is an ESP32-C3 status light for Codex Desktop. The desktop Bridge sends `GREEN`, `RED`, or `YELLOW` over USB serial or UDP on the same LAN.

## LED behavior

| GPIO | LED | Behavior |
| --- | --- | --- |
| GPIO5 | Yellow | Blinks with a one-second cycle while no desktop heartbeat is available |
| GPIO6 | Green | Blinks for two seconds when a desktop link is established |
| GPIO7 | Red | Shows the real `RED` state |

After the connection animation, the LEDs show the real state received from the Bridge.

## Transport modes

- `AUTO`: accepts serial and UDP; a recent wired heartbeat takes priority.
- `WIRED`: accepts serial only.
- `WIRELESS`: accepts UDP only.

Serial control commands:

```text
MODE AUTO
MODE WIRED
MODE WIRELESS
STATUS
WIFI_CONFIG
CLEAR_WIFI
```

## Wi-Fi provisioning

WiFiManager runs in non-blocking mode. When provisioning is required, connect a phone to:

```text
SSID: CodexLight-XXXX
Password: 123456789
URL: http://192.168.4.1
```

Select a 2.4 GHz Wi-Fi network. The computer and ESP32 must be on the same LAN for UDP control.

## Build and upload

```powershell
cd Firmware
pio run
pio run -t upload
```

## Run the Bridge

```powershell
# Wired
python Bridge\codex_light_monitor.py --serial auto --baud 115200

# Wireless
python Bridge\codex_light_monitor.py --udp --udp-port 4210

# Both
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

The Bridge repeats the current state every two seconds as a heartbeat.
