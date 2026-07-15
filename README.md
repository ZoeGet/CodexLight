# CodexLight

简体中文 | [English](README.en.md)

CodexLight 是一个基于 ESP32-C3 SuperMini 的 Codex 状态灯项目。它使用三颗独立 WS2812B LED 显示 Codex 当前状态，并通过电脑端 Bridge 监听本机 Codex Desktop 日志。

当前支持两种连接方式：

- 有线 USB 串口：电脑端自动查找 ESP32 串口，发送 `GREEN` / `RED` / `YELLOW`
- 无线 UDP：电脑端通过局域网 UDP 广播状态，ESP32 连接 Wi-Fi 后监听状态包

两种方式可以同时启用，也可以按需只使用其中一种。

## 状态定义

| 灯光 | Codex 状态 |
| --- | --- |
| 绿灯 | 空闲、完成，或一段时间没有新的 agent 活动 |
| 红灯 | 正在思考、编写、调用工具或执行动作 |
| 黄灯 | 正在等待批准，或等待明确的用户输入 |

## 当前功能

- 已建立 PlatformIO 固件工程，位于 `Firmware/`。
- 已接入 Arduino Framework 和 FastLED。
- 已封装 `LedController`，业务逻辑不直接调用 FastLED API。
- 支持 3 颗独立 WS2812B 单灯珠控制。
- 支持 USB 串口接收状态命令。
- 支持 Wi-Fi UDP 接收状态命令。
- 电脑端 Bridge 支持监听 Codex 本地 JSONL/SQLite 日志。
- 电脑端 Bridge 支持自动串口识别、UDP 广播和 Win10 托盘后台运行。
- 已通过 `pio run` 编译验证。
- 已加入硬件原理图资料，位于 `Hardware/Schematic/`。


## 项目使用框架

CodexLight 分为三个部分：

```text
Codex Desktop 本地日志
        ↓
Bridge 电脑端脚本
        ↓ USB 串口 / UDP 局域网
ESP32-C3 固件
        ↓
三颗独立 WS2812B 状态灯
```

各部分职责：

- `Codex Desktop`：产生本地会话日志和诊断日志。
- `Bridge/`：电脑端桥接程序，监听 Codex 日志，判断 `GREEN` / `RED` / `YELLOW` 状态，并通过 USB 串口或 UDP 发送给 ESP32。
- `Firmware/`：ESP32-C3 固件，接收串口或 UDP 状态命令，控制三颗 WS2812B。
- `Hardware/`：硬件原理图和后续硬件资料。
- `Docs/`：实现说明和后续交接文档。

推荐使用方式：

- 开发和调试阶段：优先使用 USB 串口，命令简单，便于确认硬件是否正常。
- 长期日常使用：使用 Win10 托盘后台模式，同时启用 USB 串口和 UDP。
- 纯无线摆放：配置 Wi-Fi、完成 UDP 配对后，只使用 UDP 模式。

## 首次下载后的配置与编译

首次 clone 或下载项目后，按下面顺序操作。

### 1. 安装基础工具

需要安装：

- Python 3
- PlatformIO
- Git
- ESP32-C3 对应 USB 驱动，取决于你的开发板串口芯片

如果要使用 USB 串口自动识别，需要安装 pyserial：

```powershell
pip install pyserial
```

只使用 UDP 时，电脑端不需要 pyserial。

### 2. 配置 Wi-Fi（仅无线 UDP 需要）

仓库不会提交真实 Wi-Fi 配置。复制示例文件：

```text
Firmware\include\wifi_secrets.example.h
```

为本地私有文件：

```text
Firmware\include\wifi_secrets.h
```

然后填写自己的 Wi-Fi：

```cpp
#define CODEXLIGHT_WIFI_SSID 你的WiFi名称
#define CODEXLIGHT_WIFI_PASSWORD 你的WiFi密码
```

`wifi_secrets.h` 已被 `.gitignore` 忽略，不要提交到 GitHub。

如果只使用 USB 串口，可以不创建 `wifi_secrets.h`。固件仍可编译，并通过串口工作。

### 3. 编译固件

```powershell
cd Firmware
pio run
```

### 4. 烧录固件

连接 ESP32-C3 后运行：

```powershell
pio run -t upload
```

如果 PlatformIO 没有自动识别端口，可以在 `Firmware/platformio.ini` 中临时指定 `upload_port`，或者用 PlatformIO 的设备列表确认端口。

### 5. 首次无线配对（仅 UDP 需要）

ESP32 第一次没有 token 时，上电会自动进入配对窗口。电脑端运行：

```powershell
python Bridge\codex_light_monitor.py --pair --udp-port 4210
```

配对成功后会生成：

```text
Bridge\config.local.json
```

里面保存 UDP token、ESP32 MAC 和最近 IP。这个文件也是本地私有配置，不要提交。

### 6. 启动电脑端 Bridge

控制台方式：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

Win10 托盘后台方式：

```text
Bridge\start_codex_light_tray.bat
```

双击后会在右下角任务栏折叠区显示托盘图标。

### 7. 后续需要修改的文件

通常只需要改这些本地文件：

```text
Firmware\include\wifi_secrets.h     # Wi-Fi 名称和密码，本地私有
Bridge\config.local.json             # UDP 配对后自动生成，本地私有
Bridge\start_codex_light_tray.bat    # 可选：选择串口/UDP 启动参数
```

不建议直接修改这些文件，除非你在开发功能：

```text
Firmware\src\main.cpp
Bridge\codex_light_monitor.py
```

## 硬件配置

主控：ESP32-C3 SuperMini

LED：3 颗独立 WS2812B，不是串联灯带。每颗 LED 有独立数据线，每路 `LEDS_PER_CHANNEL = 1`。

| LED | GPIO | 显示颜色 |
| --- | --- | --- |
| Red | GPIO7 | 红色 |
| Green | GPIO6 | 绿色 |
| Yellow | GPIO5 | 黄色 |

硬件连接要求：

- 每个 WS2812B 的 DIN 串联 330 欧姆电阻。
- 每颗 WS2812B 电源旁放置 100nF 去耦电容。
- LED 电源地与 ESP32-C3 地线共地。

## 项目结构

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

目录说明：

- `Bridge/`：电脑端桥接程序，负责监听 Codex 日志并发送灯光状态。
- `Docs/`：项目文档目录。
- `Firmware/`：ESP32-C3 固件工程。
- `Hardware/`：硬件原理图和后续硬件资料。

## 电脑端 Bridge

Bridge 脚本位于：

```text
Bridge/codex_light_monitor.py
```

它监听 Codex 本地日志：

```text
C:\Users\<you>\.codex\sessions\YYYY\MM\DD\rollout-*.jsonl
C:\Users\<you>\.codex\logs_2.sqlite
```

### 控制台运行

只监听并在控制台打印状态：

```powershell
python Bridge\codex_light_monitor.py
```

使用 USB 串口：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

使用 UDP 广播：

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

同时启用 USB 串口和 UDP：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

### Win10 托盘后台运行

双击：

```text
Bridge\start_codex_light_tray.bat
```

默认会同时启用有线串口和无线 UDP：

```bat
set "MONITOR_ARGS=--serial auto --baud 115200 --udp --udp-port 4210"
```

托盘图标会出现在 Win10 右下角任务栏折叠区。右键图标可以打开日志目录、重启监听脚本或退出后台程序。

### Bridge 协议

USB 串口输出：

```text
GREEN
RED
YELLOW
```

UDP 输出默认发到 `255.255.255.255:4210`，并每 2 秒重复发送当前状态作为心跳。配对后 UDP 包会带 token：

```text
CODEXLIGHT/1 token=<paired-token> GREEN
CODEXLIGHT/1 token=<paired-token> RED
CODEXLIGHT/1 token=<paired-token> YELLOW
```

首次使用无线 UDP 时，先让 ESP32 进入配对窗口，然后运行：

```powershell
python Bridge\codex_light_monitor.py --pair --udp-port 4210
```

配对成功后 token 会保存在 ESP32 NVS，电脑端会把 token、ESP32 MAC 和最近 IP 保存到 `Bridge\config.local.json`，不需要写死在代码里。正常运行时，Bridge 优先向保存的 ESP32 IP 单播状态包，并只接受匹配 MAC 的 `HELLO` 来刷新 IP。

串口模式需要安装 pyserial：

```powershell
pip install pyserial
```

UDP 模式不需要第三方 Python 依赖。

## 固件

固件工程位于 `Firmware/`：

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
lib_deps = fastled/FastLED
```

`Firmware/include/config.h` 集中维护硬件和通信参数：

- `RED_LED_PIN = 7`
- `GREEN_LED_PIN = 6`
- `YELLOW_LED_PIN = 5`
- `LEDS_PER_CHANNEL = 1`
- `DEFAULT_BRIGHTNESS = 64`
- `SERIAL_BAUD = 115200`
- `UDP_PORT = 4210`
- `WIRELESS_TIMEOUT_MS = 10000`

`Firmware/include/led.h` 定义 `LedController` 对外接口，包含单独点亮状态灯的方法：

```cpp
void showRed();
void showGreen();
void showYellow();
```

`Firmware/src/main.cpp` 同时处理两种输入：

- 从 `Serial` 读取 `GREEN` / `RED` / `YELLOW`
- 从 UDP 读取 `CODEXLIGHT/1 GREEN` / `RED` / `YELLOW`

收到有效状态后，固件只点亮对应颜色的 LED。

## Wi-Fi 配置

无线 UDP 模式需要配置 Wi-Fi。复制：

```text
Firmware\include\wifi_secrets.example.h
```

为：

```text
Firmware\include\wifi_secrets.h
```

然后填写：

```cpp
#define CODEXLIGHT_WIFI_SSID "你的WiFi名称"
#define CODEXLIGHT_WIFI_PASSWORD "你的WiFi密码"
```

`wifi_secrets.h` 已加入 `.gitignore`，不会提交到 GitHub。没有这个文件时，固件仍然可以编译并通过 USB 串口工作。UDP token 通过配对保存到 ESP32 NVS，不需要写入 `wifi_secrets.h`。

## 编译验证

电脑端脚本语法检查：

```powershell
python -m py_compile Bridge\codex_light_monitor.py
```

固件编译：

```powershell
cd Firmware
pio run
```

## 硬件资料

硬件资料位于 `Hardware/`：

- `Hardware/Schematic/Schematic1.pdf`：当前原理图文件。

## 许可

MIT
