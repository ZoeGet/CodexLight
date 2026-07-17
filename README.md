# CodexLight

CodexLight 是基于 ESP32-C3 SuperMini 的 Codex 状态灯。电脑端 Bridge 监听 Codex Desktop 本地日志，通过 USB 串口或同一局域网内的 UDP 向 ESP32 发送状态。

## LED 定义

| GPIO | LED | 行为 |
| --- | --- | --- |
| GPIO5 | 黄灯 | 未收到电脑端心跳时按 1 秒周期闪烁；真实状态为 `YELLOW` 时常亮 |
| GPIO6 | 绿灯 | 连接电脑端后闪烁 2 秒；真实状态为 `GREEN` 时常亮 |
| GPIO7 | 红灯 | 真实状态为 `RED` 时常亮 |

三颗 LED 均为独立 WS2812B，每颗 LED 的 DIN 分别接 GPIO5、GPIO6、GPIO7。

## 通信模式

固件支持三种模式：

- `AUTO`：同时接受串口和 UDP，最近的有效串口心跳优先。
- `WIRED`：只接受 USB 串口。
- `WIRELESS`：只接受 UDP。

默认模式在 `Firmware/include/config.h` 中设置：

```cpp
constexpr const char* DEFAULT_TRANSPORT_MODE = "AUTO";
```

也可以通过串口动态切换并保存：

```text
MODE AUTO
MODE WIRED
MODE WIRELESS
STATUS
```

## 手机 AP 配网

固件使用 WiFiManager 非阻塞配网。没有可用 Wi-Fi 凭据时会开启：

```text
热点：CodexLight-XXXX
密码：123456789
地址：http://192.168.4.1
```

手机连接热点后选择 2.4 GHz Wi-Fi 并输入密码。配网期间 USB 串口仍可使用。

串口配网命令：

```text
WIFI_CONFIG
CLEAR_WIFI
```

## 编译和烧录

```powershell
cd Firmware
pio run
pio run -t upload
```

串口波特率已配置为 `115200`：

```powershell
pio device monitor
```

## 启动 Bridge

仅有线：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

仅无线：

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

有线和无线同时启用：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

串口自动识别需要：

```powershell
pip install pyserial
```

Bridge 每 2 秒重发当前状态作为心跳。ESP32 超过 6 秒没有收到当前模式对应的心跳，就回到 GPIO5 黄灯闪烁。

更详细的运行说明见 `Docs/重构后使用说明.md`。
