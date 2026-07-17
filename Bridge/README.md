# CodexLight Bridge

[English](README_en.md) | 简体中文 | [项目主页](../README.md)

Bridge 运行在 Codex Desktop 所在的 Windows 电脑上。它持续读取本机 Codex 会话日志，将当前状态转换为 `GREEN`、`RED` 或 `YELLOW`，再通过 USB 串口、UDP 或两者同时发送给 ESP32-C3。

## 状态规则

| Codex 事件 | 输出 |
| --- | --- |
| `task_started`、思考、消息、工具调用和工具输出 | `RED` |
| 工具调用需要批准、权限或用户输入 | `YELLOW` |
| `task_complete` 或 `turn_aborted` | `GREEN` |

活动任务不会因为普通 `item/completed` 或短暂无日志而提前切换为绿色。检测到批准请求后，Bridge 会锁存 `YELLOW`，并忽略并行工具调用、无关工具输出和 SQLite 诊断错误，直到对应批准调用返回。收到 `task_complete` 或 `turn_aborted` 后，Bridge 会锁存 `GREEN`，忽略同一轮尾随的日志和诊断错误，直到下一次 `task_started` 或 `user_message`。

## 要求

- Windows 10/11
- Python 3.9+
- Codex Desktop
- 有线模式需要 `pyserial`

```powershell
python -m pip install pyserial
```

## 启动方式

以下命令均在仓库根目录执行。

### 有线串口

```powershell
python Bridge\codex_light_monitor.py --serial COM4 --baud 115200
```

自动选择常见 ESP32 串口：

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

Bridge 成功打开串口后会等待 2 秒。仅启用串口时会重复发送 `MODE WIRED`，直到 ESP32 回复 `MODE_OK WIRED`，确保固件已经切换并接受有线状态。之后每 2 秒重发当前状态作为心跳。

### 无线 UDP

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

电脑和 ESP32 必须处于同一局域网。Bridge 默认广播到 `255.255.255.255:4210`，收到 ESP32 的 `HELLO` 后记录设备 MAC 和 IP，并优先使用单播。

本地发现结果保存在：

```text
Bridge/config.local.json
```

该文件已被 Git 忽略。需要重新发现设备时可以删除它。

### 同时启用串口和 UDP

```powershell
python Bridge\codex_light_monitor.py --serial COM4 --baud 115200 --udp --udp-port 4210
```

串口和 UDP 同时启用时，Bridge 会自动发送 `MODE AUTO`，固件会同时接受两种心跳并优先使用有效的有线连接。

### Windows 托盘

双击：

```text
Bridge\start_codex_light_tray.bat
```

双击默认使用 `AUTO`，同时启动自动串口和 UDP，并由固件优先使用有效的有线连接。也可以在 PowerShell 中明确选择模式：

```powershell
Bridge\start_codex_light_tray.bat auto
Bridge\start_codex_light_tray.bat wired
Bridge\start_codex_light_tray.bat wireless
```

| 模式 | Bridge 参数 | 说明 |
| --- | --- | --- |
| `auto` | 串口 + UDP | 默认模式，有线有效时优先有线 |
| `wired` | 仅串口 | Bridge 自动发送并确认 `MODE WIRED` |
| `wireless` | UDP 状态；串口只做一次模式设置 | 有 USB 时保存 `MODE WIRELESS` 后立即释放串口；无 USB 时使用固件已保存模式 |

串口、波特率和 UDP 端口集中配置在批处理文件顶部的 `SERIAL_PORT`、`SERIAL_BAUD` 和 `UDP_PORT`。托盘提示文字会显示当前启动模式，右键菜单中的 `Connection mode` 可以直接在 `Auto`、`Wired only` 和 `Wireless only` 之间切换；切换时 Bridge 会自动重启。无线模式在 USB 可用时会先完成 `MODE WIRELESS` 握手，然后关闭串口，后续状态只通过 UDP 发送。

右键托盘图标可以：

- 打开 `Bridge/logs` 日志目录
- 切换有线、无线或自动模式
- 重启 Bridge
- 退出 Bridge

## 常用参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--serial COM4` | 禁用 | 使用指定串口 |
| `--serial auto` | 禁用 | 自动选择常见 ESP32/USB 串口 |
| `--baud` | `115200` | 串口波特率 |
| `--firmware-mode` | 自动判断 | 通过串口协商 `AUTO`、`WIRED` 或 `WIRELESS` |
| `--serial-setup-only` | 禁用 | 串口只用于保存模式，收到确认后立即释放 |
| `--udp` | 禁用 | 启用 UDP 状态发送和设备发现 |
| `--udp-host` | `255.255.255.255` | 未发现设备时的 UDP 目标 |
| `--udp-port` | `4210` | UDP 端口 |
| `--udp-interval` | `2.0` | 状态心跳间隔，串口和 UDP 共用 |
| `--device-mac` | 空 | 仅接受指定 ESP32 MAC 的发现包 |
| `--sessions-root` | `~/.codex/sessions` | Codex JSONL 会话目录 |
| `--sqlite` | `~/.codex/logs_2.sqlite` | Codex 诊断日志数据库 |
| `--poll` | `0.5` | 日志轮询间隔 |
| `--max-age-days` | `2` | 扫描最近多少天的会话文件 |
| `--quiet-timeout` | `20` | 无活动任务时的绿色回退时间 |
| `--from-start` | 禁用 | 启动时处理已有 JSONL 内容 |
| `--repeat` | 禁用 | 在控制台打印重复状态 |

查看完整参数：

```powershell
python Bridge\codex_light_monitor.py --help
```

`--complete-grace` 是保留的兼容参数，当前绿色状态由 `task_complete` 控制。

## 协议

### 串口

每行发送一个 ASCII 状态：

```text
GREEN
RED
YELLOW
```

### UDP

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
```

ESP32 发现包：

```text
CODEXLIGHT/1 HELLO mac=AA:BB:CC:DD:EE:FF mode=WIRELESS
```

## 验证

不创建 `__pycache__` 的语法检查：

```powershell
python -B -c "import ast,pathlib; ast.parse(pathlib.Path('Bridge/codex_light_monitor.py').read_text(encoding='utf-8')); print('OK')"
```

前台启动后，正常日志类似：

```text
2026-07-17 13:19:18 SERIAL connected COM4
2026-07-17 13:19:20 GREEN  startup
2026-07-17 13:20:07 RED    reasoning
2026-07-17 13:20:45 YELLOW approval_pending
2026-07-17 13:21:02 GREEN  task_complete
```

## 故障排查

- `SERIAL connect failed`：关闭 PlatformIO Monitor 或其他占用该 COM 口的程序。
- `SERIAL no matching serial port`：使用 `--serial COM4` 显式指定端口。
- 无线状态不更新：检查同一局域网、防火墙 UDP 4210 和固件的 `WIRELESS`/`AUTO` 模式。
- 状态来自错误设备：使用 `--device-mac AA:BB:CC:DD:EE:FF` 限定设备。
- 状态判断异常：删除旧 Bridge 进程并重新启动，确保加载最新脚本。

## 安全

UDP 数据没有加密或鉴权，只应在可信局域网内使用。

## 许可证

Bridge 遵循仓库根目录的 [MIT License](../LICENSE)。
