# CodexLight 使用说明

[English](USAGE.en.md) | 简体中文 | [返回项目主页](README.md)

本文说明 CodexLight 的固件烧录、USB Wi-Fi 配网、有线/无线/AUTO 模式、Windows 托盘、串口命令、UDP 协议、纯无线供电和排障流程。

## 准备

电脑端需要：

- Windows 10/11
- Codex Desktop

直接使用 `Bridge\dist\CodexLightTray.exe` 不需要安装 Python。只有从源码运行 Bridge 时才需要 Python 3.9+ 和 `pyserial`。

固件端需要 PlatformIO Core 或 VS Code PlatformIO IDE。PlatformIO 会自动安装 `Adafruit NeoPixel`。

## 编译和烧录

```powershell
cd C:\path\to\CodexLight\Firmware
pio run
pio run -t upload --upload-port COM4
```

把 `COM4` 换成实际 ESP32-C3 端口。查看串口输出：

```powershell
pio device monitor --port COM4 --baud 115200
```

PlatformIO Monitor、其他串口程序和 Bridge 不能同时占用同一个 COM 口。使用托盘前先关闭串口监视器。

## USB Wi-Fi 配网

当前固件不再开启 ESP32 热点配网，推荐使用托盘：

1. 用 USB 数据线连接电脑和 CodexLight。
2. 双击 `Bridge\dist\CodexLightTray.exe` 启动托盘。默认是 `WIRELESS` 模式。
3. 右键托盘图标，选择 `Configure WiFi`。
4. 输入 2.4 GHz 路由器 SSID 和密码。
5. 点击 `Save`。

成功后会显示 `WiFi saved and connected.`。设备只会在连接成功后保存 Wi-Fi；密码错误、SSID 不存在或连接超时不会覆盖旧配置。

命令行配网：

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

成功输出类似：

```text
DEVICE WIFI_SET_OK YourWifi 192.168.x.x
```

EXE 版失败日志：

```text
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.err.log
```

源码版日志仍位于 `Bridge\logs`。

## Windows 托盘

推荐直接双击单文件 EXE：

```text
Bridge\dist\CodexLightTray.exe
```

EXE 内置 64 位 Python 运行时和 `pyserial`，无需额外安装依赖；它不会显示控制台窗口，并默认进入 `WIRELESS` 模式。首次启动会把运行环境、日志和本地配置释放到 `%LOCALAPPDATA%\CodexLight`。退出时请右键托盘图标选择 `Exit`。

首次启用 UDP 时，Windows 防火墙可能询问是否允许网络访问，请允许专用网络。源码启动器仍然保留：

```text
Bridge\start_codex_light_tray.vbs
Bridge\start_codex_light_tray.bat
```

可指定模式：

```powershell
Bridge\dist\CodexLightTray.exe auto
Bridge\dist\CodexLightTray.exe wired
Bridge\dist\CodexLightTray.exe wireless
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

托盘菜单：

- `Configure WiFi`：通过 USB 配网。
- `Connection mode`：切换 `Auto`、`Wired only`、`Wireless only`。
- `Open log folder`：打开日志目录。
- `Restart monitor`：重启 Bridge 监控进程。
- `Exit`：退出托盘程序。

## 有线、无线和 AUTO 模式

| 模式 | 行为 | 使用建议 |
| --- | --- | --- |
| `AUTO` | Bridge 同时发 USB 和 UDP；固件优先使用 6 秒内新鲜的 USB 心跳，USB 断开后可切到 UDP | 日常调试或混合连接 |
| `WIRED` | 只使用 USB 串口 | 烧录后验证、串口调试、Wi-Fi 不稳定时 |
| `WIRELESS` | 只使用 Wi-Fi UDP；USB 只用于配网或保存模式后释放 | 设备脱离电脑摆放时推荐 |

模式会保存到 ESP32 NVS，普通固件上传不会清除。

## 纯无线使用流程

1. 插电脑 USB，使用 `Configure WiFi` 成功保存 Wi-Fi。
2. 右键托盘，选择 `Connection mode` -> `Wireless only`。
3. 关闭托盘。
4. 断开电脑 USB，使用充电宝或锂电池升压 5 V 供电。
5. 等 20 到 60 秒，观察灯码。
6. 打开托盘。
7. 如果日志出现 `UDP ack ... active=WIRELESS state=...`，纯无线已可用。

无电脑 USB 时，日志中出现下面两行是正常的：

```text
SERIAL no matching serial port
SERIAL setup skipped; using saved firmware mode.
```

## LED 行为和诊断灯码

| 灯光 | 含义 |
| --- | --- |
| 绿灯常亮 | 空闲、任务完成或任务中止 |
| 红灯常亮 | Codex 正在思考、回复或运行工具 |
| 黄灯常亮 | 等待审批、权限或用户输入 |
| 绿灯闪烁 2 秒 | 首次建立电脑连接 |
| 黄灯慢闪 | Wi-Fi 已连接，但未收到电脑 UDP/USB 心跳 |
| 红黄交替 | 没有保存 Wi-Fi 配置，需要 USB 配网 |
| 红色双闪循环 | 有 Wi-Fi 配置，但 Wi-Fi 正在重连或连接失败 |

## 手动运行 Bridge

有线：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

无线：

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

AUTO：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
```

无线模式要求电脑和 ESP32 在同一局域网，路由器没有开启 AP 隔离/客户端隔离，Windows 防火墙允许 Python 使用 UDP 4210。

## 串口命令

| 命令 | 作用 |
| --- | --- |
| `GREEN` | 设置有线状态为绿色并刷新心跳 |
| `RED` | 设置有线状态为红色并刷新心跳 |
| `YELLOW` | 设置有线状态为黄色并刷新心跳 |
| `PING` | 刷新有线心跳，回复 `PONG` |
| `STATUS` | 输出模式、活动传输、Wi-Fi、IP、UDP 诊断 |
| `MODE WIRED` | 只使用 USB，并保存模式 |
| `MODE WIRELESS` | 只使用 UDP，并保存模式 |
| `MODE AUTO` | 接受 USB 和 UDP，并保存模式 |
| `WIFI_CONFIG` | 提示当前使用 USB 配网 |
| `WIFI_SET <ssid><TAB><password>` | 通过 USB 设置 Wi-Fi，连接成功后保存 |
| `CLEAR_WIFI` | 清除已保存 Wi-Fi，等待重新配网 |

## UDP 协议

Bridge 发送：

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
CODEXLIGHT/1 PING
```

ESP32 回复：

```text
CODEXLIGHT/1 ACK mac=<MAC> mode=<MODE> active=<TRANSPORT> state=<STATE>
```

ESP32 也会广播发现包：

```text
CODEXLIGHT/1 HELLO mac=<MAC> mode=<MODE>
```

默认 UDP 端口是 `4210`。

## 排障

### Wi-Fi 配网失败

- 确认 USB 已连接，且 PlatformIO Monitor 或其他串口程序没有占用 COM 口。
- ESP32-C3 只支持 2.4 GHz Wi-Fi。
- EXE 版查看 `%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log` 和 `.err.log`；源码版查看 `Bridge/logs`。
- 当前固件使用 ESP32-C3 SDK 默认的 Wi-Fi 发射功率，由 PHY 和地区配置决定。

### 纯无线没有响应

- 黄灯慢闪表示 Wi-Fi 已连接，只是在等托盘 UDP；打开托盘即可。
- 红色双闪表示 Wi-Fi 正在重连或失败，检查路由器、SSID、密码和 2.4 GHz 网络。
- `ping 192.168.x.x` 不通且 `arp -a` 没有设备 MAC 时，设备没有在局域网在线。
- 重启托盘可让 Bridge 重新发现设备 IP。
- Windows 防火墙需要允许 Python UDP 4210。

### 恢复出厂状态

```powershell
cd Firmware
pio run -t erase --upload-port COM4
pio run -t upload --upload-port COM4
```

## 开发验证

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
cd Firmware
pio run
```

更多架构说明见 [Docs/使用与实现说明.md](Docs/使用与实现说明.md)。
