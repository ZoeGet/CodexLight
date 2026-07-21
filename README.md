# CodexLight

[English](README.en.md) | 简体中文 | [使用说明](USAGE.md) | [English Usage Guide](USAGE.en.md)

CodexLight 是一套基于 ESP32-C3 的 Codex Desktop 状态灯。电脑端 Bridge 读取本机 Codex 会话日志，将当前状态通过 USB 串口、局域网 UDP 或两者同时发送给 ESP32-C3；固件驱动三颗独立 WS2812B LED 显示工作、等待和完成状态。

本项目是社区开源项目，与 OpenAI 没有官方关联或背书。

## 当前方案

- Wi-Fi 配网采用 USB 串口配置，不再使用 ESP32 AP 热点配网。
- 托盘程序提供 `Configure WiFi` 菜单，输入 SSID/密码后通过 USB 发送给设备。
- 设备只在连接成功后保存 Wi-Fi；失败配置不会写入 Flash。
- 支持 `AUTO`、`WIRED`、`WIRELESS` 三种持久化连接模式。
- Windows 托盘可用隐藏启动器 `Bridge/start_codex_light_tray.vbs` 后台运行，不保留 PowerShell 窗口。

## 状态定义

| 状态 | Codex 条件 | LED | GPIO |
| --- | --- | --- | --- |
| `GREEN` | 空闲、任务完成或任务中止 | 绿灯 | GPIO6 |
| `RED` | 正在思考、回复、运行工具或处理任务 | 红灯 | GPIO7 |
| `YELLOW` | 等待审批、权限确认或用户输入 | 黄灯 | GPIO5 |
| 未连接 | 6 秒内没有收到有效电脑端心跳 | 黄灯闪烁 | GPIO5 |

首次收到有效电脑端心跳时，绿灯会闪烁 2 秒作为连接提示，然后显示真实状态。任务完成后的绿色会保持到下一次任务开始、进入等待状态或连接断开。

## 快速开始

1. 安装依赖：

   ```powershell
   python -m pip install pyserial
   ```

2. 编译并烧录固件：

   ```powershell
   cd Firmware
   pio run
   pio run -t upload --upload-port COM4
   ```

3. 双击启动隐藏托盘：

   ```text
   Bridge\start_codex_light_tray.vbs
   ```

4. 右键托盘图标，选择 `Configure WiFi`，输入路由器 SSID 和密码。

5. 右键托盘图标，在 `Connection mode` 中选择：

   - `Auto (wired + wireless)`：推荐日常使用，有线优先，无线备用。
   - `Wired only`：只用 USB。
   - `Wireless only`：只用 Wi-Fi UDP。

完整操作见 [USAGE.md](USAGE.md)。

## 硬件

推荐器件：

- ESP32-C3 SuperMini 或兼容 ESP32-C3 开发板
- 三颗 WS2812B RGB LED
- 稳定 5 V 电源
- USB 数据线

接线：

| 功能 | ESP32-C3 引脚 |
| --- | --- |
| 黄灯 DIN | GPIO5 |
| 绿灯 DIN | GPIO6 |
| 红灯 DIN | GPIO7 |
| WS2812B VCC | 5 V |
| WS2812B GND | GND |

三颗 WS2812B 是三路独立数据输入，不是串联灯带。当前固件使用 `NEO_GRB + NEO_KHZ800`，全局亮度在 [Firmware/include/config.h](Firmware/include/config.h) 的 `DEFAULT_BRIGHTNESS` 中配置。

## 目录结构

```text
CodexLight/
├── Bridge/        # Windows 端日志监听、串口/UDP 发送和托盘程序
├── Firmware/      # ESP32-C3 PlatformIO 固件
├── Hardware/      # BOM、原理图、PCB、Gerber、外壳 STL
├── Docs/          # 使用与实现说明
├── README.md      # 中文项目说明
├── README.en.md   # English project documentation
├── USAGE.md       # 中文使用手册
└── USAGE.en.md    # English usage guide
```

## 文档

- [中文使用说明](USAGE.md)
- [English Usage Guide](USAGE.en.md)
- [Bridge 说明](Bridge/README.md)
- [Bridge English Guide](Bridge/README_en.md)
- [Firmware 说明](Firmware/README.md)
- [使用与实现说明](Docs/使用与实现说明.md)
- [Usage and Implementation Guide](Docs/USAGE_AND_IMPLEMENTATION.md)

## 安全说明

- UDP 控制协议没有加密或鉴权，只应在可信局域网内使用。
- Wi-Fi 密码通过 USB 串口写入 ESP32 NVS，固件不会打印密码。
- 不要提交 `Bridge/config.local.json`、`Bridge/logs/`、Wi-Fi 密码、设备 IP 等本地配置。

## 验证

```powershell
python -B -m py_compile Bridge\codex_light_monitor.py
powershell -NoProfile -Command "$e=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath 'Bridge\CodexLightTray.ps1' -Raw), [ref]$e) | Out-Null; if($e){$e; exit 1}else{'OK'}"
cd Firmware
pio run
```

## 许可证

本项目使用 [MIT License](LICENSE)。第三方依赖遵循各自许可证。
