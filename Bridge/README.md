# CodexLight Bridge

[English](README_en.md) | 简体中文 | [项目主页](../README.md)

Bridge 运行在 Windows 电脑上，读取本机 Codex Desktop 会话日志，将状态映射为 `GREEN`、`RED`、`YELLOW`，再通过 USB 串口、局域网 UDP 或两者同时发送给 ESP32-C3。

## 功能

- 监听 `~/.codex/sessions/**/*.jsonl` 和 `~/.codex/logs_2.sqlite`。
- 支持 USB 串口、UDP、AUTO 混合模式。
- 提供 Windows 托盘菜单：配网、切换模式、查看日志、重启监控、退出。
- 通过 USB 串口为设备配置 Wi-Fi，不使用 ESP32 AP 热点。
- 发现 UDP 设备后将 MAC 和 IP 保存到 `Bridge/config.local.json`。

## 状态规则

| Codex 事件 | 输出 |
| --- | --- |
| `task_started`、推理、消息、工具调用和工具输出 | `RED` |
| 工具调用需要审批、权限或用户输入 | `YELLOW` |
| `task_complete` 或 `turn_aborted` | `GREEN` |

Bridge 只发送颜色状态，不发送 Codex 消息正文、工具输出、API Key 或登录令牌。

## 依赖

```powershell
python -m pip install pyserial
```

## 托盘启动

推荐双击隐藏启动器：

```text
Bridge\start_codex_light_tray.vbs
```

旧批处理也可用，但可能显示控制台窗口：

```text
Bridge\start_codex_light_tray.bat
```

托盘菜单：

- `Configure WiFi`：通过 USB 写入路由器 SSID/密码。
- `Connection mode`：切换 `Auto (wired + wireless)`、`Wired only`、`Wireless only`。
- `Open log folder`：打开 `Bridge/logs`。
- `Restart monitor`：重启监控进程。
- `Exit`：退出。

## Wi-Fi 配网

托盘 `Configure WiFi` 会暂停监控进程，打开串口，发送：

```text
WIFI_SET <ssid><TAB><password>
```

设备连接成功后返回：

```text
WIFI_SET_OK <ssid> <ip>
```

失败日志保存在：

```text
Bridge\logs\wifi_setup.out.log
Bridge\logs\wifi_setup.err.log
```

命令行配网：

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

## 运行模式

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

| 模式 | Bridge 行为 |
| --- | --- |
| `WIRED` | 只打开串口并发送状态 |
| `WIRELESS` | 只通过 UDP 发送状态；USB 可用于保存 `MODE WIRELESS` 后释放 |
| `AUTO` | 串口和 UDP 同时启用，固件优先使用新鲜串口心跳 |

## 常用参数

| 参数 | 说明 |
| --- | --- |
| `--serial COM4` | 指定串口 |
| `--serial auto` | 自动选择常见 ESP32/USB 串口 |
| `--baud 115200` | 串口波特率 |
| `--udp` | 启用 UDP 状态发送和发现 |
| `--udp-port 4210` | UDP 端口 |
| `--firmware-mode AUTO` | 通过串口保存固件模式 |
| `--serial-setup-only` | 串口只用于模式设置，确认后释放 |
| `--wifi-ssid` / `--wifi-password` | 一次性 USB Wi-Fi 配网 |
| `--wifi-config path.json` | 从 JSON 文件读取 `{ "ssid": "...", "password": "..." }` |

查看完整参数：

```powershell
python Bridge\codex_light_monitor.py --help
```

## 日志和本地文件

- `Bridge/logs/codex_light_monitor.out.log`
- `Bridge/logs/codex_light_monitor.err.log`
- `Bridge/logs/wifi_setup.out.log`
- `Bridge/logs/wifi_setup.err.log`
- `Bridge/config.local.json`

这些文件是本地运行状态，不应提交。

## 验证

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
```

## License

Bridge 遵循仓库根目录的 [MIT License](../LICENSE)。
