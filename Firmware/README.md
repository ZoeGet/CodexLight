# CodexLight Firmware

[项目主页](../README.md) | [Project Home](../README.en.md)

本目录是 ESP32-C3 端 PlatformIO Arduino 固件。固件负责 USB Wi-Fi 配网、STA 联网、USB 串口/UDP 心跳接收、通信模式持久化、无串口启动诊断灯码和三路 WS2812B 状态显示。

This directory contains the ESP32-C3 PlatformIO Arduino firmware. It handles USB Wi-Fi provisioning, STA networking, USB serial/UDP heartbeats, persistent transport modes, standalone LED diagnostics, and three independent WS2812B status LEDs.

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

普通串口监视器会占用 COM 口。使用 Bridge 或托盘前请关闭 PlatformIO Monitor。

Close PlatformIO Monitor before starting the Bridge or tray because only one process can use the COM port.

## Main Configuration / 主要配置

Edit `include/config.h`:

| Setting | Default | Description |
| --- | --- | --- |
| `YELLOW_LED_PIN` | `5` | Yellow LED data pin |
| `GREEN_LED_PIN` | `6` | Green LED data pin |
| `RED_LED_PIN` | `7` | Red LED data pin |
| `DEFAULT_BRIGHTNESS` | `50` | NeoPixel brightness |
| `SERIAL_BAUD` | `115200` | USB CDC baud rate |
| `UDP_PORT` | `4210` | UDP listen/discovery port |
| `LINK_TIMEOUT_MS` | `6000` | Desktop heartbeat timeout |
| `WIFI_CONNECT_TIMEOUT_MS` | `15000` | Blocking connect timeout used only during USB provisioning validation |
| `WIFI_RECONNECT_INTERVAL_MS` | `10000` | Non-blocking saved Wi-Fi retry interval |
| `DEFAULT_TRANSPORT_MODE` | `AUTO` | Default mode when NVS has none |
| `DEBUG_SERIAL` | `false` | Default debug logging; keep disabled for standalone wireless operation |

`CONFIG_AP_*` constants are retained only for compatibility. The main firmware does not start a SoftAP.

`CONFIG_AP_*` 常量仅保留兼容用途，主固件不会启动热点。

## Wi-Fi Provisioning / Wi-Fi 配网

Use USB serial:

```text
WIFI_SET <ssid><TAB><password>
```

During provisioning, the firmware validates the credentials by attempting STA connection. Credentials are saved in Preferences namespace `wifi` only after successful connection:

```text
ssid
password
```

配网失败不会覆盖旧配置。保存过 Wi-Fi 后，启动时使用非阻塞连接；如果连接失败，固件保留凭据并按 `WIFI_RECONNECT_INTERVAL_MS` 持续重试。

Failed provisioning does not overwrite existing credentials. Once Wi-Fi is saved, boot uses a non-blocking connect path. If the connection fails, credentials are kept and the firmware retries every `WIFI_RECONNECT_INTERVAL_MS`.

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

## LED Runtime and Diagnostics / LED 运行与诊断

| Pattern | Meaning |
| --- | --- |
| Solid green | Idle, task complete, or task aborted |
| Solid red | Codex is working |
| Solid yellow | Waiting for approval, permission, or user input |
| Green blink for 2 seconds | First desktop heartbeat connected |
| Slow yellow blink | Wi-Fi is connected but no desktop heartbeat has arrived |
| Alternating red/yellow | No saved Wi-Fi credentials |
| Repeating red double-blink | Saved Wi-Fi exists but reconnect is in progress or failed |

独立供电时不要依赖串口输出判断状态，直接看灯码即可。

When running from a power bank or battery, use the LED diagnostics instead of serial output.

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
LED_TEST RED
LED_TEST GREEN
LED_TEST YELLOW
LED_TEST OFF
```

`WIFI_CONFIG` only prints the USB provisioning hint. It does not open an AP.

`WIFI_CONFIG` 只提示使用 USB 配网，不会打开热点。

## UDP Protocol / UDP 协议

Accepted packets:

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
CODEXLIGHT/1 PING
```

Acknowledgement:

```text
CODEXLIGHT/1 ACK mac=<MAC> mode=<MODE> active=<TRANSPORT> state=<STATE>
```

Discovery:

```text
CODEXLIGHT/1 HELLO mac=<MAC> mode=<MODE>
```

## Files / 文件

- `src/main.cpp`: serial commands, UDP heartbeat, transport selection, LED state machine, diagnostics.
- `src/config_portal.cpp`: STA Wi-Fi, USB provisioning wrapper, non-blocking saved Wi-Fi reconnect.
- `src/storage.cpp`: Preferences read/write for Wi-Fi credentials.
- `src/led.cpp`: three independent Adafruit NeoPixel outputs.
- `include/config.h`: user-editable GPIO, timing, brightness, port, and Wi-Fi power settings.
- `platformio.ini`: PlatformIO environment and dependencies.

## NVS and Reset / NVS 与擦除

Wi-Fi credentials and transport mode are stored in NVS. Normal firmware upload does not erase them.

```powershell
pio run -t erase --upload-port COM4
pio run -t upload --upload-port COM4
```

Usually `CLEAR_WIFI` and `MODE ...` are enough. Full erase is only needed for factory reset or corrupted NVS.

## License

Firmware follows the repository [MIT License](../LICENSE). Third-party dependencies keep their own licenses.
