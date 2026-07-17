# CodexLight Bridge

The Bridge monitors local Codex Desktop logs and emits `GREEN`, `RED`, or `YELLOW`.

```powershell
# USB serial
python Bridge\codex_light_monitor.py --serial auto --baud 115200

# UDP on the same LAN
python Bridge\codex_light_monitor.py --udp --udp-port 4210

# Both transports
python Bridge\codex_light_monitor.py --serial auto --baud 115200 --udp --udp-port 4210
```

Serial auto-detection requires `pyserial`:

```powershell
pip install pyserial
```

The current state is repeated every two seconds over each enabled transport so the ESP32 can detect connection loss.
