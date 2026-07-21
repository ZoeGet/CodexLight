# CodexLight Hardware

[项目主页](../README.md) | [Project Home](../README.en.md)

本目录包含 CodexLight 的原理图、PCB 制造资料和 3D 打印外壳。

This directory contains the CodexLight schematic, PCB manufacturing files, and 3D-printable enclosure.

## Files / 文件

| Path | Description |
| --- | --- |
| `BOM/BOM.xlsx` | 元器件物料清单 / Bill of materials |
| `Schematic/Schematic1.pdf` | 电路原理图 / Schematic PDF |
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

三颗 LED 使用独立数据线，不是串联灯带。LED 电源和 ESP32-C3 必须共地，并使用稳定的 5 V 供电。

The three LEDs use independent data lines, not a chained strip. The LED supply and ESP32-C3 must share ground, and a stable 5 V supply is required.

## Notes / 注意

- 固件默认使用 `NEO_GRB + NEO_KHZ800`。
- 如果更换不同批次 WS2812B 后颜色不对，请检查色序、DIN 方向、焊接和供电。
- USB 数据线仍然建议保留，固件 Wi-Fi 配网依赖 USB 串口。

- Firmware defaults to `NEO_GRB + NEO_KHZ800`.
- If another WS2812B batch displays incorrect colors, verify pixel order, DIN orientation, soldering, and power.
- Keep USB data access available because firmware Wi-Fi provisioning uses USB serial.

## License

Unless a third-party component states otherwise, hardware files follow the repository [MIT License](../LICENSE).
