# CodexLight Hardware

[项目主页](../README.md) | [Project Home](../README.en.md)

本目录包含 CodexLight 的 BOM、原理图、PCB 制造资料和 3D 打印外壳。

This directory contains the CodexLight bill of materials, schematic, PCB manufacturing files, and 3D-printable enclosure.

## Files / 文件

| Path | Description |
| --- | --- |
| `BOM/BOM.xls` | 物料清单 / Bill of materials |
| `Schematic/Schematic1.pdf` | 原理图 PDF / Schematic PDF |
| `PCB/Source/CodexLight.epro2` | PCB 源工程 / PCB source project |
| `PCB/Gerber/CodexLight_PCB_Gerber.zip` | Gerber 制造包 / Gerber fabrication package |
| `Enclosure/CodexLight_T.stl` | 上壳 / Top enclosure |
| `Enclosure/CodexLight_B.stl` | 下壳 / Bottom enclosure |

## LED Connections / LED 连接

| LED | GPIO |
| --- | --- |
| Yellow WS2812B DIN / 黄灯 DIN | GPIO5 |
| Green WS2812B DIN / 绿灯 DIN | GPIO6 |
| Red WS2812B DIN / 红灯 DIN | GPIO7 |

三颗 LED 使用独立数据线，不是串联灯带。LED 电源和 ESP32-C3 必须共地，并使用稳定 5 V 供电。

The three LEDs use independent data lines, not a chained strip. The LED supply and ESP32-C3 must share ground, and a stable 5 V supply is required.

## Standalone Power / 独立供电

纯无线使用时，ESP32-C3 和 WS2812B 都需要从充电宝、5 V 稳压模块或锂电池升压模块供电。不要只给 LED 供电；ESP32-C3 的 `5V/VIN` 和 GND 也必须接入同一电源系统。

For wireless-only use, both the ESP32-C3 and WS2812B LEDs must be powered from the power bank, 5 V regulator, or Li-ion boost converter. Do not power only the LEDs. The ESP32-C3 `5V/VIN` and GND must be connected to the same supply system.

## Notes / 注意

- 固件默认使用 `NEO_GRB + NEO_KHZ800`。
- 如果更换不同批次 WS2812B 后颜色不对，请检查色序、DIN 方向、焊接和供电。
- USB 数据线仍然建议保留，因为固件烧录和首次 Wi-Fi 配网依赖 USB 串口。
- ESP32-C3 连接 Wi-Fi 时会有电流尖峰；如果使用小型升压模块，确认动态电压不会跌落。

- Firmware defaults to `NEO_GRB + NEO_KHZ800`.
- If another WS2812B batch displays incorrect colors, verify pixel order, DIN orientation, soldering, and power.
- Keep USB data access available because firmware upload and first-time Wi-Fi provisioning use USB serial.
- ESP32-C3 current spikes during Wi-Fi association. If using a small boost converter, verify dynamic voltage does not sag.

## License

Unless a third-party component states otherwise, hardware files follow the repository [MIT License](../LICENSE).
