# CodexLight Firmware

[项目主页](../README.md) | [Project Home](../README.en.md)

本目录是 ESP32-C3 端 PlatformIO Arduino 固件。固件负责 USB Wi-Fi 配网、STA 联网、USB 串口/UDP 心跳接收、通信模式持久化和三路 WS2812B 状态显示。

This directory contains the ESP32-C3 PlatformIO Arduino firmware. It handles USB Wi-Fi provisioning, STA networking, USB serial/UDP heartbeats, persistent transport modes, and three independent WS2812B status LEDs.

## Build Environment / 构建环境

```ini
platform = espressif32@6.6.0
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
```

PlatformIO installs the external dependency automatically:

- `Adafruit NeoPixel`

当前固件不再依赖 `WiFiManager`、`ESPAsyncWebServer` 或 AP 配网页面。

The current firmware no longer depends on `WiFiManager`, `ESPAsyncWebServer`, or an AP provisioning portal.

## Build and Upload / 编译与烧录

```powershell
cd Firmware
pio run
pio run -t upload --upload-port COM4
pio device monitor --port COM4 --baud 115200
```

无 Wi-Fi 配置时启动输出类似：

```text
CODEXLIGHT READY
WIFI_PROVISIONING USB_SERIAL
WIFI_USB_PROVISIONING READY FORMAT=WIFI_SET <ssid><TAB><password>
STATUS ... network=USB_PROVISIONING radio=OFF
```

已联网时输出类似：

```text
WIFI_CONNECTED YourWifi 192.168.x.x
STATUS ... wifi=CONNECTED radio=STA ip=192.168.x.x
```

## Main Configuration / 主要配置

Edit `include/config.h`:

| Setting | Default | Description |
| --- | --- | --- |
| `YELLOW_LED_PIN` | `5` | Yellow LED data pin |
| `GREEN_LED_PIN` | `6` | Green LED data pin |
| `RED_LED_PIN` | `7` | Red LED data pin |
| `DEFAULT_BRIGHTNESS` | `25` | NeoPixel brightness |
| `SERIAL_BAUD` | `115200` | USB CDC baud rate |
| `UDP_PORT` | `4210` | UDP listen/discovery port |
| `LINK_TIMEOUT_MS` | `6000` | Desktop heartbeat timeout |
| `DEFAULT_TRANSPORT_MODE` | `AUTO` | Default mode when NVS has none |

`CONFIG_AP_*` 常量目前保留为兼容配置项，但主固件不会启动 AP。

`CONFIG_AP_*` constants are currently retained for compatibility, but the main firmware does not start an AP.

## Wi-Fi Provisioning / Wi-Fi 配网

Use USB serial:

```text
WIFI_SET <ssid><TAB><password>
```

The firmware attempts STA connection for `WIFI_CONNECT_TIMEOUT_MS`. If successful, it saves credentials in Preferences namespace `wifi`:

```text
ssid
password
```

如果连接失败，固件不会保存这组凭据，并会关闭 Wi-Fi 等待重新配网。

If the connection fails, credentials are not saved and Wi-Fi is turned off while waiting for another provisioning attempt.

Clear Wi-Fi:

```text
CLEAR_WIFI
```

## Transport Modes / 通信模式

| Mode | Behavior |
| --- | --- |
| `WIRED` | Accept USB serial heartbeat only |
| `WIRELESS` | Accept UDP heartbeat only |
| `AUTO` | Accept both; fresh USB heartbeat has priority |

Commands:

```text
MODE WIRED
MODE WIRELESS
MODE AUTO
STATUS
```

The selected mode is saved in Preferences namespace `codexlight`, key `transport`.

## Serial Commands / 串口命令

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
WIFI_SET <ssid><TAB><password>
CLEAR_WIFI
```

`WIFI_CONFIG` only prints the USB provisioning hint. It does not open an AP.

`WIFI_CONFIG` 只提示使用 USB 配网，不会打开热点。

## Files / 文件

- `src/main.cpp`: serial commands, UDP heartbeat, transport selection, LED state machine.
- `src/config_portal.cpp`: STA Wi-Fi connection and USB provisioning wrapper.
- `src/storage.cpp`: Preferences read/write for Wi-Fi credentials.
- `src/led.cpp`: three independent Adafruit NeoPixel outputs.
- `include/config.h`: user-editable GPIO, timing, brightness, and port settings.
- `platformio.ini`: PlatformIO environment and dependencies.

## NVS and Reset / NVS 与擦除

Wi-Fi credentials and transport mode are stored in NVS. Normal firmware upload does not erase them.

```powershell
pio run -t erase --upload-port COM4
pio run -t upload --upload-port COM4
```

Usually `CLEAR_WIFI` and `MODE ...` are enough; full erase is only needed for factory reset or corrupted NVS.

## License

Firmware follows the repository [MIT License](../LICENSE). Third-party dependencies keep their own licenses.
