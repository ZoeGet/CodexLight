# CodexLight Bridge

[English](README_en.md) | 简体中文 | [项目主页](../README.md)

Bridge 运行在 Windows 电脑上，读取本机 Codex Desktop 会话日志，把状态映射为 `GREEN`、`RED`、`YELLOW`，再通过 USB 串口、局域网 UDP 或两者同时发送给 ESP32-C3。

## 功能

- 监听 `~/.codex/sessions/**/*.jsonl` 和 `~/.codex/logs_2.sqlite`。
- 支持 USB 串口、UDP、AUTO 混合模式。
- 提供 Windows 托盘菜单：配网、切换模式、查看日志、重启监控、退出。
- 通过 USB 串口为设备配置 Wi-Fi，不使用 ESP32 AP 热点。
- 自动记住最近发现的 UDP 设备，方便下次启动继续使用无线连接。
- 无电脑 USB 时，托盘会跳过串口设置并继续用已保存的无线模式发送 UDP。

## 状态规则

| Codex 事件 | 输出 |
| --- | --- |
| `task_started`、推理、消息、工具调用和工具输出 | `RED` |
| 工具调用需要审批、权限或用户输入 | `YELLOW` |
| `task_complete` 或 `turn_aborted` | `GREEN` |

Bridge 同时兼容 `function_call` / `function_call_output` 和 `custom_tool_call` / `custom_tool_call_output`。当工具调用包含 `require_escalated`、`sandbox_permissions`、`request_user_input` 或其他审批/权限标记时，状态保持为 `YELLOW`，直到对应的工具输出到达。

Bridge 只发送颜色状态，不发送 Codex 消息正文、工具输出、API Key 或登录令牌。

## 依赖

直接使用已构建的 `CodexLightTray.exe` 不需要另行安装 Python。只有从源码启动或重新构建 EXE 时需要本机 Python；源码启动还需要：

```powershell
python -m pip install pyserial
```

## 托盘启动

普通用户直接双击单文件程序：

```text
Bridge\dist\CodexLightTray.exe
```

EXE 内置 64 位 Python 运行时和串口库，首次启动会释放到 `%LOCALAPPDATA%\CodexLight`，日志和本地设备配置也保存在该目录。首次使用 UDP 时，Windows 防火墙可能询问是否允许网络访问，请允许专用网络。

从源码启动时，推荐双击隐藏启动器，默认进入 `WIRELESS` 模式：

```text
Bridge\start_codex_light_tray.vbs
```

旧批处理也可用，但可能短暂显示控制台窗口：

```text
Bridge\start_codex_light_tray.bat
```

托盘菜单：

- `Configure WiFi`：通过 USB 写入路由器 SSID/密码。
- `Connection mode`：切换 `Auto (wired + wireless)`、`Wired only`、`Wireless only`。
- `Open log folder`：打开当前运行版本的日志目录；EXE 版为 `%LOCALAPPDATA%\CodexLight\logs`，源码版为 `Bridge\logs`。
- `Restart monitor`：重启监控进程。
- `Exit`：退出。

重新构建单文件 EXE：

```powershell
powershell -ExecutionPolicy Bypass -File Bridge\build_tray_exe.ps1
```

输出文件为 `Bridge\dist\CodexLightTray.exe`。构建过程使用本机 64 位 Python 和 Windows 自带的 .NET Framework C# 编译器，不需要联网。

## Wi-Fi 配网

托盘 `Configure WiFi` 会暂停监控进程，打开串口，发送：

```text
WIFI_SET <ssid><TAB><password>
```

设备连接成功后返回：

```text
WIFI_SET_OK <ssid> <ip>
```

EXE 版失败日志保存到：

```text
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.out.log
%LOCALAPPDATA%\CodexLight\logs\wifi_setup.err.log
```

源码版失败日志保存到 `Bridge\logs`。

命令行配网：

```powershell
python Bridge\codex_light_monitor.py --serial auto --wifi-ssid "YourWifi" --wifi-password "YourPassword"
```

## 运行模式

| 模式 | Bridge 行为 |
| --- | --- |
| `WIRED` | 打开串口并只通过 USB 发送状态 |
| `WIRELESS` | 通过 UDP 发送状态；如果 USB 存在，会用串口保存 `MODE WIRELESS` 后释放；如果 USB 不存在，会直接使用设备已保存模式 |
| `AUTO` | 串口和 UDP 同时启用，固件优先使用新鲜串口心跳 |

手动运行：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
python Bridge\codex_light_monitor.py --udp --udp-port 4210
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210 --firmware-mode AUTO
```

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
| `--reset-on-connect` | 打开串口后发送 ESP32 reset pulse，用于无线托盘启动时初始化设备 |
| `--wifi-ssid` / `--wifi-password` | 一次性 USB Wi-Fi 配网 |
| `--wifi-config path.json` | 从 JSON 文件读取 `{ "ssid": "...", "password": "..." }` |

## 日志

- EXE 版：`%LOCALAPPDATA%\CodexLight\logs`
- 源码版：`Bridge\logs`
- 两种版本均包含 `codex_light_monitor.out.log`、`codex_light_monitor.err.log`、`wifi_setup.out.log`、`wifi_setup.err.log`。

## 验证

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
```

## License

Bridge 遵循仓库根目录的 [MIT License](../LICENSE)。
