# 获取 Codex 运行状态

本文说明 CodexLight Bridge 如何获取 Codex Desktop 的运行状态，以及如何在 Windows 上手动检查、调试和扩展这套机制。

> 本文针对本项目当前实现。Bridge 读取本机 Codex 的会话日志来推断状态，不读取消息正文，也不调用 API Key 或登录令牌。

## 1. 状态获取链路

```text
Codex Desktop
    ├─ 会话事件：%USERPROFILE%\.codex\sessions\**\*.jsonl
    └─ 运行日志：%USERPROFILE%\.codex\logs_2.sqlite
             ↓
Bridge\Source\codex_light_monitor.py
             ↓
GREEN / RED / YELLOW
             ↓
USB 串口或 UDP
             ↓
ESP32-C3 状态灯
```

Bridge 默认每 0.5 秒轮询一次最近两天修改过的 JSONL 会话文件，并同时以只读方式检查 `logs_2.sqlite`。新建会话会从文件开头读取；Bridge 启动前已经存在的旧内容默认跳过，避免把历史任务误判为当前任务。

## 2. 状态含义

| Bridge 状态 | 含义 | 典型事件或条件 | 状态灯 |
| --- | --- | --- | --- |
| `GREEN` | 空闲、任务已结束或任务已中止 | `task_complete`、`turn_aborted`、长时间无活动 | 绿灯 |
| `RED` | Codex 正在处理任务 | `task_started`、用户消息、推理、回复、工具调用或工具输出 | 红灯 |
| `YELLOW` | Codex 正在等待批准、权限或用户输入 | 工具调用中出现 `require_escalated`、`sandbox_permissions`、`request_user_input`、`approval` 或 `permission` | 黄灯 |

状态不是直接从某一条日志文本读取，而是由 `MonitorState` 根据事件顺序维护：

- 收到 `task_started`：进入 `RED`，记录当前 `turn_id`。
- 收到工具调用：记录 `call_id`；普通工具调用保持 `RED`。
- 工具调用包含审批或权限标记：进入 `YELLOW`，直到对应的工具输出到达。
- 收到 `function_call_output` 或 `custom_tool_call_output`：移除对应的未完成调用；如果仍有其他审批等待，继续保持 `YELLOW`，否则回到 `RED`。
- 收到 `task_complete` 或 `turn_aborted`：清空未完成调用，锁定为 `GREEN`，直到下一次新任务开始。
- 没有明确结束事件时，默认经过 20 秒无活动后回到 `GREEN`。

## 3. 日志位置

### 3.1 会话 JSONL

默认目录：

```text
%USERPROFILE%\.codex\sessions
```

Bridge 会递归查找其中最近修改的 `*.jsonl` 文件。每行应当是一个 JSON 对象，常见结构类似：

```json
{
  "type": "event_msg",
  "payload": {
    "type": "task_started",
    "turn_id": "..."
  }
}
```

工具调用通常位于 `response_item` 事件中，`payload.type` 可能是：

```text
function_call
function_call_output
custom_tool_call
custom_tool_call_output
```

### 3.2 SQLite 运行日志

默认文件：

```text
%USERPROFILE%\.codex\logs_2.sqlite
```

Bridge 以只读模式连接 `logs` 表，只处理上次检查之后的新记录。当前实现主要利用 `ERROR` 级别记录辅助维持 `RED`，SQLite 不是状态判断的主要来源；主要状态仍来自会话 JSONL 事件。

## 4. 手动查看当前日志

### 查看最近修改的会话文件

```powershell
$root = Join-Path $env:USERPROFILE ".codex\sessions"
Get-ChildItem -LiteralPath $root -Recurse -Filter *.jsonl |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 10 FullName, LastWriteTime, Length
```

### 查看某个会话文件末尾内容

```powershell
Get-Content -LiteralPath "C:\path\to\session.jsonl" -Tail 20
```

只查看事件类型，避免把完整消息内容输出到终端：

```powershell
Get-Content -LiteralPath "C:\path\to\session.jsonl" -Tail 100 |
  ForEach-Object {
    try {
      $e = $_ | ConvertFrom-Json
      [pscustomobject]@{
        EventType   = $e.type
        PayloadType = $e.payload.type
      }
    } catch {
      # 忽略尚未写完整的 JSON 行
    }
  }
```

### 查看 SQLite 中最新日志

如果本机安装了 `sqlite3`：

```powershell
sqlite3 "$env:USERPROFILE\.codex\logs_2.sqlite" `
  "SELECT id, level, target FROM logs ORDER BY id DESC LIMIT 20;"
```

没有 `sqlite3` 命令时，可以只使用 Bridge 自身，不需要额外安装 SQLite 工具。

## 5. 启动 Bridge 监控

推荐直接启动托盘程序：

```text
Bridge\CodexLightTray.exe
```

从源码启动并使用 USB 串口：

```powershell
python Bridge\Source\codex_light_monitor.py --serial auto --baud 115200
```

只使用局域网 UDP：

```powershell
python Bridge\Source\codex_light_monitor.py --udp --udp-port 4210
```

同时使用 USB 和 UDP：

```powershell
python Bridge\Source\codex_light_monitor.py `
  --serial auto --baud 115200 `
  --udp --udp-port 4210 --firmware-mode AUTO
```

调试旧会话内容时，可以显式从文件开头处理：

```powershell
python Bridge\Source\codex_light_monitor.py --serial auto --from-start
```

常用调试参数：

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `--sessions-root` | `%USERPROFILE%\\.codex\\sessions` | 指定会话目录 |
| `--sqlite` | `%USERPROFILE%\\.codex\\logs_2.sqlite` | 指定 SQLite 日志 |
| `--poll` | `0.5` 秒 | 轮询间隔 |
| `--max-age-days` | `2` 天 | 扫描最近修改的会话文件范围 |
| `--quiet-timeout` | `20` 秒 | 无活动后回到 `GREEN` 的等待时间 |
| `--from-start` | 关闭 | 启动时处理已有 JSONL 内容 |
| `--repeat` | 关闭 | 即使状态未变化也重复发送状态 |

## 6. 如何确认状态已经被获取

1. 启动 Bridge，并保持托盘程序或命令行窗口运行。
2. 在 Codex Desktop 中发送一个简单任务。
3. 观察状态灯：任务处理期间应为红色，完成后应为绿色。
4. 触发需要审批、权限或用户输入的操作时，应显示黄色。
5. 查看 Bridge 日志确认是否发现会话文件和设备：

   - EXE 版：`%LOCALAPPDATA%\CodexLight\logs`
   - 源码版：`Bridge\Source\logs`

如果状态灯完全不变化，先检查会话文件是否在任务期间更新，再检查 Bridge 是否使用了正确的 `--sessions-root` 和 `--sqlite` 路径。

## 7. 常见问题

### 一直是绿色

- Codex Desktop 没有产生新的会话事件。
- Bridge 启动时扫描范围不包含实际会话文件；尝试增加 `--max-age-days`。
- 任务在 Bridge 启动前已经完成；默认不会重放旧 JSONL，调试时使用 `--from-start`。
- Bridge 没有权限读取 `%USERPROFILE%\.codex`。

### 一直是红色

- 当前会话没有写入 `task_complete` 或 `turn_aborted`。
- 仍存在未完成工具调用。
- 可以暂时降低 `--quiet-timeout` 验证轮询和空闲回退逻辑，但这只是诊断手段。

### 一直是黄色

- 某个审批工具调用没有收到对应的工具输出。
- 日志中仍有另一个待批准的调用；Bridge 会在所有审批完成前保持黄色。

### 能看到日志但灯不亮

这表示“状态获取”和“状态发送”需要分开排查：

1. 先确认 Bridge 命令行或 `codex_light_monitor.out.log` 中状态原因是否变化。
2. 有线模式检查 COM 口是否被 PlatformIO Monitor 或其他串口程序占用。
3. 无线模式检查设备 IP、UDP 端口 `4210` 和 Windows 防火墙专用网络权限。

## 8. 安全与兼容性注意事项

- JSONL 和 SQLite 可能包含会话元数据、工具参数或错误信息；排障时不要把完整日志上传到公开位置。
- 只读取日志即可得到本项目需要的三态信号，不要修改 Codex 会话文件或 SQLite 数据库。
- Codex 日志事件格式属于本地实现依赖。若事件名称或目录结构发生变化，应先在样例日志上验证，再修改 `Bridge\Source\codex_light_monitor.py` 的解析逻辑。
- 修改状态规则后，应运行语法检查，并用 `task_started`、工具调用、审批等待、`task_complete`、`turn_aborted` 这几类事件逐项验证。

## 9. 相关源码

- [Bridge README](../Bridge/README.md)
- [codex_light_monitor.py](../Bridge/Source/codex_light_monitor.py)
- [CodexLight 使用说明](../USAGE.md)
