# CodexLight 使用说明

[English](USAGE.en.md) | 简体中文 | [返回项目主页](README.md)

本文说明 CodexLight 的固件烧录、USB Wi-Fi 配网、有线/无线/AUTO 模式、Windows 托盘、串口命令、UDP 协议、排障和验证流程。

## 准备

电脑端需要：

- Windows 10/11
- Python 3.9+
- Codex Desktop
- `pyserial`

```powershell
python -m pip install pyserial
```

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

没有 Wi-Fi 配置时，正常输出类似：

```text
CODEXLIGHT READY
WIFI_PROVISIONING USB_SERIAL
[WiFi] No saved Wi-Fi credentials; waiting for USB provisioning
WIFI_USB_PROVISIONING READY FORMAT=WIFI_SET <ssid><TAB><password>
STATUS mode=AUTO active=NONE wifi=DISCONNECTED ... network=USB_PROVISIONING radio=OFF
```

已经配好 Wi-Fi 时，正常输出类似：

```text
CODEXLIGHT READY
WIFI_PROVISIONING USB_SERIAL
[WiFi] Connecting to YourWifi
WIFI_CONNECTED YourWifi 192.168.x.x
STATUS mode=AUTO active=NONE wifi=CONNECTED ... radio=STA ip=192.168.x.x
```

PlatformIO Monitor 和 Bridge 不能同时占用同一个 COM 口。使用 Bridge 前先关闭串口监视器。

## USB Wi-Fi 配网

现在不再使用手机连接 ESP32 热点配网。推荐使用托盘菜单：

1. 用 USB 连接电脑和 CodexLight。
2. 启动托盘：双击 `Bridge\start_codex_light_tray.vbs`。
3. 右键托盘图标，选择 `Configure WiFi`。
4. 输入路由器 SSID 和密码。
5. 点击 `Save`。

成功后会显示 `WiFi saved and connected.`，设备会把配置保存到 ESP32 NVS。之后断电重启会自动连接。

也可以用命令行配网：

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

成功输出类似：

```text
DEVICE WIFI_SET_OK YourWifi 192.168.x.x
```

失败时查看：

```text
Bridge\logs\wifi_setup.out.log
Bridge\logs\wifi_setup.err.log
```

固件只会在连接成功后保存 Wi-Fi。密码错误、SSID 不存在或连接超时不会覆盖旧配置。

## Windows 托盘

推荐启动方式：

```text
Bridge\start_codex_light_tray.vbs
```

这个启动器会隐藏 PowerShell 窗口。不要直接关闭托盘程序所属的 PowerShell 进程；退出时右键托盘图标选择 `Exit`。

旧批处理启动器仍可用，但可能短暂显示控制台窗口：

```text
Bridge\start_codex_light_tray.bat
```

也可以指定启动模式：

```powershell
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

托盘右键菜单提供：

- `Configure WiFi`：通过 USB 配网。
- `Connection mode`：切换 `Auto`、`Wired only`、`Wireless only`。
- `Open log folder`：打开日志目录。
- `Restart monitor`：重启 Bridge 监控进程。
- `Exit`：退出托盘程序。

## 有线、无线和 AUTO 模式

| 模式 | 行为 | 操作建议 |
| --- | --- | --- |
| `AUTO` | Bridge 同时通过 USB 和 UDP 发送；固件优先使用 6 秒内新鲜的 USB 心跳，USB 断开后可切到 UDP | 日常推荐 |
| `WIRED` | 只使用 USB 串口 | 调试、烧录后验证、Wi-Fi 不稳定时使用 |
| `WIRELESS` | 只使用 Wi-Fi UDP；USB 只用于保存一次模式或配网 | 设备要脱离电脑摆放时使用 |

切换方式：右键托盘图标，进入 `Connection mode`，选择对应模式。Bridge 会自动重启内部监控进程，并通过串口发送：

```text
MODE AUTO
MODE WIRED
MODE WIRELESS
```

模式会保存到 ESP32 NVS，普通固件上传不会清除。

## 手动运行 Bridge

有线：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

无线：

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

有线和无线同时启用：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
```

无线要求电脑和 ESP32 在同一局域网，且路由器没有开启 AP 隔离/客户端隔离。Windows 防火墙需要允许 Python 使用 UDP 4210。

## LED 行为

- 未收到有效电脑心跳：GPIO5 黄灯闪烁。
- 首次建立连接：GPIO6 绿灯闪烁 2 秒。
- Codex 正在思考、回复或执行工具：GPIO7 红灯常亮。
- 等待审批、权限或用户输入：GPIO5 黄灯常亮。
- `task_complete` 或 `turn_aborted`：GPIO6 绿灯常亮。
- 当前有效传输超过 6 秒没有心跳：恢复黄灯闪烁。

## 串口命令

| 命令 | 作用 |
| --- | --- |
| `GREEN` | 设置有线状态为绿色并刷新心跳 |
| `RED` | 设置有线状态为红色并刷新心跳 |
| `YELLOW` | 设置有线状态为黄色并刷新心跳 |
| `PING` | 刷新有线心跳，回复 `PONG` |
| `STATUS` | 输出模式、活动传输、Wi-Fi 状态、IP 等诊断信息 |
| `MODE WIRED` | 只使用 USB，并保存模式 |
| `MODE WIRELESS` | 只使用 UDP，并保存模式 |
| `MODE AUTO` | 同时接受 USB 和 UDP，并保存模式 |
| `WIFI_CONFIG` | 提示当前使用 USB 配网 |
| `WIFI_SET <ssid><TAB><password>` | 通过 USB 设置 Wi-Fi，连接成功后保存 |
| `CLEAR_WIFI` | 清除已保存 Wi-Fi，并关闭 Wi-Fi 等待重新配网 |

## UDP 协议

Bridge 发送：

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
```

ESP32 每 2 秒广播发现包：

```text
CODEXLIGHT/1 HELLO mac=AA:BB:CC:DD:EE:FF mode=AUTO
```

默认 UDP 端口为 `4210`。

## 排障

### Wi-Fi 配网失败

- 确认 USB 已连接，且 PlatformIO Monitor 没有占用 COM 口。
- 使用托盘失败弹窗里的详细日志判断原因。
- 查看 `Bridge/logs/wifi_setup.out.log` 和 `Bridge/logs/wifi_setup.err.log`。
- ESP32-C3 只支持 2.4 GHz Wi-Fi。
- SSID/密码中有特殊字符时，请使用最新托盘版本；当前版本通过临时 JSON 传参，不会被 PowerShell 拆错。

### 无线模式没有反应

- 确认设备串口输出 `WIFI_CONNECTED <ssid> <ip>`。
- 确认电脑和设备在同一局域网。
- 允许 Python 通过 Windows 防火墙使用 UDP 4210。
- 删除 `Bridge/config.local.json` 让 Bridge 重新发现设备。
- 先切到 `AUTO`，插 USB 验证设备能收到状态。

### 黄灯一直闪

- Bridge 没有运行，或选错了连接模式。
- COM 口被 PlatformIO Monitor 占用。
- 无线模式下设备和电脑不在同一局域网。
- 运行 `STATUS` 查看 `mode`、`active`、`wifi`、`network`、`radio`。

### 完全恢复出厂状态

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
