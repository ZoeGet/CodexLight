# CodexLight Bridge

`codex_light_monitor.py` 监听 Codex Desktop 本地日志，并输出三种状态：

- `GREEN`：空闲或完成。
- `RED`：正在思考、执行工具或处理任务。
- `YELLOW`：等待批准或用户输入。

## 有线模式

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200
```

需要安装：

```powershell
pip install pyserial
```

## 无线模式

电脑和 ESP32 必须连接到同一个局域网：

```powershell
python Bridge\codex_light_monitor.py --udp --udp-port 4210
```

UDP 默认广播到 `255.255.255.255:4210`。ESP32 的 `HELLO` 包可让 Bridge 记录设备 IP，之后优先单播发送。

## 同时启用

```powershell
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

Bridge 每 2 秒通过已启用的通道重发当前状态。ESP32 以此作为连接心跳。

## 协议

串口：

```text
GREEN
RED
YELLOW
```

UDP：

```text
CODEXLIGHT/1 GREEN
CODEXLIGHT/1 RED
CODEXLIGHT/1 YELLOW
```

## 托盘启动

双击：

```text
Bridge\start_codex_light_tray.bat
```

默认同时启用串口和 UDP。
