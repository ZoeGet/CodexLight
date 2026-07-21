#!/usr/bin/env python3
"""Monitor local Codex logs and emit CodexLight states.

States:
  GREEN  - idle, completed, or no recent agent activity
  RED    - Codex is thinking, writing, or running tools
  YELLOW - Codex is waiting for approval or explicit user input

The script uses only Python's standard library by default. If --serial is used,
pyserial must be installed in the Python environment running this script.
"""

from __future__ import annotations

import argparse
import json
import re
import socket
import sqlite3
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, Optional


STATE_GREEN = "GREEN"
STATE_RED = "RED"
STATE_YELLOW = "YELLOW"
DEFAULT_CONFIG_PATH = Path(__file__).with_name("config.local.json")
SERIAL_READY_DELAY_SECONDS = 2.0
SERIAL_MODE_RETRY_SECONDS = 1.0


@dataclass
class MonitorState:
    state: str = STATE_GREEN
    reason: str = "startup"
    last_activity: float = field(default_factory=time.monotonic)
    last_emit: float = 0.0
    pending_calls: Dict[str, str] = field(default_factory=dict)
    pending_approvals: Dict[str, str] = field(default_factory=dict)
    active_turn_id: Optional[str] = None
    completion_latched: bool = False


class StateEmitter:
    def __init__(
        self,
        serial_port: Optional[str],
        baud: int,
        repeat: bool,
        udp: bool,
        udp_host: str,
        udp_port: int,
        udp_interval: float,
        device_mac: str,
        device_ip: str,
        config_path: Path,
        config: dict,
        firmware_mode: str = "",
        serial_setup_only: bool = False,
        mode_setup_enabled: bool = True,
    ) -> None:
        self.repeat = repeat
        self.last_state: Optional[str] = None
        self.serial_port = serial_port
        self.baud = baud
        self.serial = None
        self.serial_module = None
        self.list_ports_module = None
        self.last_connect_attempt = 0.0
        self.udp_enabled = udp
        self.udp_host = udp_host
        self.udp_port = udp_port
        self.udp_interval = udp_interval
        self.device_mac = normalize_mac(device_mac)
        self.device_ip = device_ip
        self.config_path = config_path
        self.config = config
        self.udp_socket = None
        self.udp_tx_socket = None
        self.last_udp_send = 0.0
        self.last_udp_broadcast_send = 0.0
        self.last_udp_subnet_probe = 0.0
        self.last_serial_send = 0.0
        self.last_udp_listen = 0.0
        self.serial_mode = firmware_mode or ("AUTO" if self.udp_enabled else "WIRED")
        self.serial_setup_only = serial_setup_only
        self.serial_setup_complete = False
        self.serial_mode_confirmed = False
        self.last_serial_mode_send = 0.0
        self.mode_setup_enabled = mode_setup_enabled

        if serial_port:
            try:
                import serial  # type: ignore
                from serial.tools import list_ports  # type: ignore
            except ImportError as exc:
                if self.serial_setup_only and self.udp_enabled:
                    print(
                        f"{time.strftime('%Y-%m-%d %H:%M:%S')} "
                        "SERIAL setup unavailable; using saved firmware mode.",
                        flush=True,
                    )
                    self.serial_port = None
                else:
                    raise SystemExit(
                        "pyserial is required for --serial. Install it with: pip install pyserial"
                    ) from exc
            else:
                self.serial_module = serial
                self.list_ports_module = list_ports
            self.connect_serial(force=True)

        if self.udp_enabled:
            self.udp_tx_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_tx_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.udp_socket.setblocking(False)
            try:
                self.udp_socket.bind(("", self.udp_port))
            except OSError as exc:
                print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} UDP listen disabled: {exc}", flush=True)

    def configure_wifi(self, ssid: str, password: str, timeout: float = 45.0) -> bool:
        if not ssid or len(ssid) > 32 or len(password) > 64 or (password and len(password) < 8):
            print("WIFI_SETUP_ERROR INVALID_CREDENTIALS", flush=True)
            return False

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.connect_serial(force=self.serial is None)
            if self.serial is not None:
                break
            time.sleep(0.5)

        if self.serial is None:
            print("WIFI_SETUP_ERROR SERIAL_NOT_FOUND", flush=True)
            return False

        try:
            self.serial.reset_input_buffer()
            self.serial.write(f"WIFI_SET {ssid}\t{password}\n".encode("utf-8"))
            self.serial.flush()
        except Exception as exc:
            print(f"WIFI_SETUP_ERROR SERIAL_WRITE_FAILED {exc}", flush=True)
            self.close_serial()
            return False

        while time.monotonic() < deadline:
            try:
                line = self.serial.readline().decode("utf-8", errors="replace").strip()
            except Exception as exc:
                print(f"WIFI_SETUP_ERROR SERIAL_READ_FAILED {exc}", flush=True)
                self.close_serial()
                return False
            if not line:
                continue
            print(f"DEVICE {line}", flush=True)
            if line.startswith("WIFI_SET_OK "):
                parts = line.split()
                if len(parts) >= 3:
                    self.device_ip = parts[-1]
                    self.config["last_device_ip"] = self.device_ip
                    save_local_config(self.config_path, self.config)
                return True
            if line.startswith("WIFI_SET_ERROR "):
                return False

        print("WIFI_SETUP_ERROR TIMEOUT", flush=True)
        return False

    def connect_serial(self, force: bool = False) -> None:
        if not self.serial_port or self.serial_module is None:
            return
        if self.serial_setup_only and self.serial_setup_complete:
            return

        now = time.monotonic()
        if not force and now - self.last_connect_attempt < 5.0:
            return
        self.last_connect_attempt = now

        if self.serial is not None and getattr(self.serial, "is_open", False):
            return

        port = self.resolve_serial_port()
        if port is None:
            print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} SERIAL no matching serial port", flush=True)
            if self.serial_setup_only and self.udp_enabled:
                print(
                    f"{time.strftime('%Y-%m-%d %H:%M:%S')} "
                    "SERIAL setup skipped; using saved firmware mode.",
                    flush=True,
                )
                self.serial_setup_complete = True
                self.serial_port = None
            return

        try:
            self.serial = self.serial_module.Serial(port, baudrate=self.baud, timeout=1)
            print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} SERIAL connected {port}", flush=True)
            time.sleep(SERIAL_READY_DELAY_SECONDS)
            self.serial_mode_confirmed = False
            self.last_serial_mode_send = 0.0
            if self.mode_setup_enabled:
                self.service_serial()
            if self.last_state and not self.serial_setup_only:
                self.serial.write((self.last_state + "\n").encode("ascii"))
                self.serial.flush()
                self.last_serial_send = time.monotonic()
        except Exception as exc:  # pyserial raises platform-specific exceptions.
            self.serial = None
            print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} SERIAL connect failed {port}: {exc}", flush=True)
            if self.serial_setup_only and self.udp_enabled:
                print(
                    f"{time.strftime('%Y-%m-%d %H:%M:%S')} "
                    "SERIAL setup skipped; using saved firmware mode.",
                    flush=True,
                )
                self.serial_setup_complete = True
                self.serial_port = None

    def resolve_serial_port(self) -> Optional[str]:
        if not self.serial_port:
            return None
        if self.serial_port.lower() != "auto":
            return self.serial_port
        if self.list_ports_module is None:
            return None

        ports = list(self.list_ports_module.comports())
        if not ports:
            return None

        ranked = sorted(ports, key=self.serial_rank, reverse=True)
        best = ranked[0]
        return best.device if self.serial_rank(best) > 0 else None

    @staticmethod
    def serial_rank(port: object) -> int:
        text = " ".join(
            str(getattr(port, name, "") or "")
            for name in ("device", "name", "description", "manufacturer", "product", "hwid")
        ).lower()
        vid = getattr(port, "vid", None)
        pid = getattr(port, "pid", None)

        score = 0
        # Common ESP32-C3 / USB serial chips and native USB CDC devices.
        if vid in {0x303A, 0x10C4, 0x1A86, 0x0403, 0x239A, 0x2E8A}:
            score += 50
        if pid in {0x1001, 0xEA60, 0x7523, 0x7522, 0x6001}:
            score += 20

        keywords = (
            "esp32",
            "espressif",
            "usb jtag",
            "usb serial",
            "serial port",
            "cp210",
            "ch340",
            "ch341",
            "silicon labs",
            "wch",
            "ftdi",
            "cdc",
            "uart",
        )
        for keyword in keywords:
            if keyword in text:
                score += 10
        return score

    def close_serial(self) -> None:
        if self.serial is None:
            return
        try:
            self.serial.close()
        except Exception:
            pass
        self.serial = None
        self.serial_mode_confirmed = False

    def service_serial(self) -> None:
        if self.serial is None:
            return

        try:
            while self.serial.in_waiting > 0:
                line = self.serial.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if line == f"MODE_OK {self.serial_mode}":
                    self.serial_mode_confirmed = True
                    if self.serial_setup_only:
                        self.serial_setup_complete = True
                if line.startswith(("MODE_OK ", "STATE_OK ", "STATUS ", "PONG")):
                    print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} DEVICE {line}", flush=True)

            if self.serial_setup_only and self.serial_setup_complete:
                self.close_serial()
                return

            now = time.monotonic()
            if (
                not self.serial_mode_confirmed
                and now - self.last_serial_mode_send >= SERIAL_MODE_RETRY_SECONDS
            ):
                self.serial.write(f"MODE {self.serial_mode}\n".encode("ascii"))
                self.serial.flush()
                self.last_serial_mode_send = now
        except Exception as exc:
            print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} SERIAL read failed: {exc}", flush=True)
            self.close_serial()

    def emit(self, state: str, reason: str) -> None:
        state_changed = state != self.last_state
        now = time.monotonic()

        if self.serial_port and self.mode_setup_enabled:
            self.connect_serial()
            self.service_serial()

        should_repeat_udp = (
            self.udp_enabled
            and self.last_state is not None
            and now - self.last_udp_send >= self.udp_interval
        )
        should_repeat_serial = (
            self.serial_port
            and not self.serial_setup_only
            and self.last_state is not None
            and now - self.last_serial_send >= self.udp_interval
        )

        if not self.repeat and not state_changed and not should_repeat_udp and not should_repeat_serial:
            return

        self.last_state = state
        stamp = time.strftime("%Y-%m-%d %H:%M:%S")
        line = f"{stamp} {state:<6} {reason}"
        if self.repeat or state_changed:
            print(line, flush=True)

        if (
            self.serial is not None
            and not self.serial_setup_only
            and (self.repeat or state_changed or should_repeat_serial)
        ):
            try:
                self.serial.write((state + "\n").encode("ascii"))
                self.serial.flush()
                self.last_serial_send = now
            except Exception as exc:
                print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} SERIAL write failed: {exc}", flush=True)
                self.close_serial()

        if self.udp_enabled:
            self.listen_udp_discovery()
            self.emit_udp(state)

    def emit_udp(self, state: str) -> None:
        if self.udp_tx_socket is None:
            return
        payload = f"CODEXLIGHT/1 {state}\n".encode("ascii")
        try:
            now = time.monotonic()
            targets = []
            if self.device_ip:
                targets.append(self.device_ip)
            if not self.device_ip or now - self.last_udp_broadcast_send >= self.udp_interval:
                targets.extend(self.udp_broadcast_targets())
                self.last_udp_broadcast_send = now
                print(
                    f"{time.strftime('%Y-%m-%d %H:%M:%S')} UDP targets {', '.join(unique_ordered(targets))}",
                    flush=True,
                )
            if self.device_ip and now - self.last_udp_subnet_probe >= 10.0:
                targets.extend(self.udp_subnet_probe_targets())
                self.last_udp_subnet_probe = now
            for target in unique_ordered(targets):
                self.udp_tx_socket.sendto(payload, (target, self.udp_port))
            self.last_udp_send = now
        except OSError as exc:
            print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} UDP send failed: {exc}", flush=True)

    def udp_broadcast_targets(self) -> list[str]:
        targets = [self.udp_host, "255.255.255.255"]
        if self.device_ip:
            octets = self.device_ip.split(".")
            if len(octets) == 4:
                targets.append(".".join(octets[:3] + ["255"]))
        return unique_ordered(targets)

    def udp_subnet_probe_targets(self) -> list[str]:
        octets = self.device_ip.split(".") if self.device_ip else []
        if len(octets) != 4:
            return []
        prefix = ".".join(octets[:3])
        return [f"{prefix}.{host}" for host in range(1, 255)]

    def listen_udp_discovery(self) -> None:
        if self.udp_socket is None:
            return

        now = time.monotonic()
        if now - self.last_udp_listen < 0.5:
            return
        self.last_udp_listen = now

        while True:
            try:
                data, sender = self.udp_socket.recvfrom(512)
            except BlockingIOError:
                return
            except OSError:
                return

            message = data.decode("ascii", errors="ignore").strip()
            if not message.startswith("CODEXLIGHT/1 "):
                continue

            if " HELLO" not in message and " ACK" not in message:
                continue

            mac = extract_field(message, "mac")
            if not mac:
                continue

            mac = normalize_mac(mac)
            if self.device_mac and mac != self.device_mac:
                continue

            if sender[0] != self.device_ip:
                self.device_ip = sender[0]
                self.config["last_device_ip"] = self.device_ip
                if mac:
                    self.config["device_mac"] = mac
                save_local_config(self.config_path, self.config)
                print(
                    f"{time.strftime('%Y-%m-%d %H:%M:%S')} UDP device {mac} at {self.device_ip}",
                    flush=True,
                )
            if " ACK" in message:
                print(
                    f"{time.strftime('%Y-%m-%d %H:%M:%S')} UDP ack from {sender[0]} {message}",
                    flush=True,
                )


def default_sessions_root() -> Path:
    return Path.home() / ".codex" / "sessions"


def default_sqlite_path() -> Path:
    return Path.home() / ".codex" / "logs_2.sqlite"


def normalize_mac(value: str) -> str:
    cleaned = re.sub(r"[^0-9A-Fa-f]", "", value or "")
    if len(cleaned) != 12:
        return ""
    cleaned = cleaned.upper()
    return ":".join(cleaned[index : index + 2] for index in range(0, 12, 2))


def extract_field(message: str, name: str) -> str:
    match = re.search(rf"(?:^|\s){re.escape(name)}=([^\s]+)", message, flags=re.IGNORECASE)
    return match.group(1) if match else ""


def unique_ordered(values: Iterable[str]) -> list[str]:
    seen = set()
    result = []
    for value in values:
        if not value or value in seen:
            continue
        seen.add(value)
        result.append(value)
    return result


def load_local_config(path: Path) -> dict:
    if not path.exists():
        return {}
    try:
        with path.open("r", encoding="utf-8-sig") as handle:
            value = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def save_local_config(path: Path, config: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(config, handle, indent=2)
        handle.write("\n")


def iter_jsonl_files(root: Path, max_age_days: int) -> Iterable[Path]:
    if not root.exists():
        return []

    cutoff = time.time() - max_age_days * 24 * 60 * 60
    files = []
    for path in root.rglob("*.jsonl"):
        try:
            if path.stat().st_mtime >= cutoff:
                files.append(path)
        except OSError:
            continue
    return sorted(files, key=lambda p: p.stat().st_mtime)


def read_new_lines(path: Path, offsets: Dict[Path, int], from_start: bool) -> Iterable[str]:
    try:
        size = path.stat().st_size
    except OSError:
        return []

    if path not in offsets:
        offsets[path] = 0 if from_start else size
        return []

    old_offset = offsets[path]
    if size < old_offset:
        old_offset = 0

    if size == old_offset:
        return []

    lines = []
    try:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            handle.seek(old_offset)
            lines = handle.readlines()
            offsets[path] = handle.tell()
    except OSError:
        return []
    return lines


def json_payload(line: str) -> Optional[dict]:
    try:
        value = json.loads(line)
    except json.JSONDecodeError:
        return None
    return value if isinstance(value, dict) else None


def contains_approval_request(payload: dict) -> bool:
    text = json.dumps(payload, ensure_ascii=False).lower()
    approval_markers = (
        "require_escalated",
        "sandbox_permissions",
        "request_user_input",
        "approval",
        "permission",
    )
    return any(marker in text for marker in approval_markers)


def set_state(ms: MonitorState, state: str, reason: str) -> None:
    ms.state = state
    ms.reason = reason
    ms.last_activity = time.monotonic()


def handle_session_event(ms: MonitorState, event: dict) -> None:
    event_type = event.get("type")
    payload = event.get("payload") if isinstance(event.get("payload"), dict) else {}
    payload_type = payload.get("type")

    if event_type == "event_msg" and payload_type == "task_started":
        ms.completion_latched = False
        ms.active_turn_id = payload.get("turn_id")
        set_state(ms, STATE_RED, "task_started")
        return

    if event_type == "event_msg" and payload_type == "turn_aborted":
        ms.pending_calls.clear()
        ms.pending_approvals.clear()
        ms.active_turn_id = None
        ms.completion_latched = True
        set_state(ms, STATE_GREEN, "turn_aborted")
        return

    if event_type == "event_msg" and payload_type == "task_complete":
        ms.pending_calls.clear()
        ms.pending_approvals.clear()
        ms.active_turn_id = None
        ms.completion_latched = True
        set_state(ms, STATE_GREEN, "task_complete")
        return

    if event_type == "event_msg" and payload_type == "user_message":
        ms.completion_latched = False
        set_state(ms, STATE_RED, "user_message")
        return

    # Codex can append token, message, tool-output, or diagnostic events after
    # the terminal event. Keep completed GREEN latched until a new turn starts.
    if ms.completion_latched:
        return

    if event_type == "event_msg" and payload_type in {"agent_message", "agent_reasoning", "token_count"}:
        if not ms.pending_approvals:
            set_state(ms, STATE_RED, payload_type)
        return

    if event_type != "response_item":
        return

    if payload_type == "function_call":
        call_id = str(payload.get("call_id") or payload.get("id") or "unknown")
        name = str(payload.get("name") or "function_call")
        ms.pending_calls[call_id] = name

        if contains_approval_request(payload):
            ms.pending_approvals[call_id] = name
            set_state(ms, STATE_YELLOW, f"approval_needed:{name}")
        elif not ms.pending_approvals:
            set_state(ms, STATE_RED, f"tool_call:{name}")
        return

    if payload_type == "function_call_output":
        call_id = str(payload.get("call_id") or "unknown")
        name = ms.pending_calls.pop(call_id, "tool")
        was_approval = ms.pending_approvals.pop(call_id, None) is not None

        output = str(payload.get("output") or "")
        if ms.pending_approvals:
            set_state(ms, STATE_YELLOW, "approval_pending")
        elif "process exited with code 1" in output.lower() or "error" in output.lower():
            set_state(ms, STATE_RED, f"tool_output_error:{name}")
        elif was_approval:
            set_state(ms, STATE_RED, f"approval_resolved:{name}")
        else:
            set_state(ms, STATE_RED, f"tool_output:{name}")
        return

    if payload_type in {"message", "reasoning"}:
        if not ms.pending_approvals:
            set_state(ms, STATE_RED, payload_type)


def connect_sqlite(path: Path) -> Optional[sqlite3.Connection]:
    if not path.exists():
        return None
    uri = f"file:{path}?mode=ro"
    try:
        con = sqlite3.connect(uri, uri=True)
        con.row_factory = sqlite3.Row
        return con
    except sqlite3.Error:
        return None


def sqlite_max_id(con: sqlite3.Connection) -> int:
    try:
        row = con.execute("select coalesce(max(id), 0) as max_id from logs").fetchone()
        return int(row["max_id"] if row else 0)
    except sqlite3.Error:
        return 0


def handle_sqlite_logs(ms: MonitorState, con: sqlite3.Connection, last_id: int) -> int:
    try:
        rows = con.execute(
            "select id, level, target from logs "
            "where id > ? order by id asc limit 500",
            (last_id,),
        ).fetchall()
    except sqlite3.Error:
        return last_id

    for row in rows:
        last_id = max(last_id, int(row["id"]))
        level = str(row["level"] or "")
        target = str(row["target"] or "")

        if level == "ERROR" and not ms.completion_latched and not ms.pending_approvals:
            set_state(ms, STATE_RED, f"sqlite_error:{target}")
            continue

    return last_id


def apply_idle_rules(ms: MonitorState, quiet_timeout: float) -> None:
    now = time.monotonic()

    if ms.pending_approvals:
        ms.state = STATE_YELLOW
        ms.reason = "approval_pending"
        return

    if ms.pending_calls:
        ms.state = STATE_RED
        ms.reason = "tool_running"
        return

    if (
        ms.active_turn_id is None
        and ms.state != STATE_GREEN
        and now - ms.last_activity >= quiet_timeout
    ):
        ms.state = STATE_GREEN
        ms.reason = "quiet_timeout"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Monitor Codex logs for CodexLight.")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--sessions-root", type=Path, default=default_sessions_root())
    parser.add_argument("--sqlite", type=Path, default=default_sqlite_path())
    parser.add_argument("--poll", type=float, default=0.5, help="Polling interval in seconds.")
    parser.add_argument("--max-age-days", type=int, default=2, help="Session files to scan.")
    parser.add_argument(
        "--quiet-timeout",
        type=float,
        default=20.0,
        help="Fallback seconds with no activity before returning to GREEN.",
    )
    parser.add_argument(
        "--complete-grace",
        type=float,
        default=1.5,
        help="Deprecated compatibility option; task_complete now controls GREEN.",
    )
    parser.add_argument("--from-start", action="store_true", help="Process existing JSONL content.")
    parser.add_argument("--serial", help="Optional serial port, for example COM5, or auto.")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--firmware-mode",
        choices=("AUTO", "WIRED", "WIRELESS"),
        default="",
        help="Transport mode negotiated over serial. Defaults from enabled outputs.",
    )
    parser.add_argument(
        "--serial-setup-only",
        action="store_true",
        help="Use serial only to save firmware mode, then release the port.",
    )
    parser.add_argument("--udp", action="store_true", help="Broadcast states over UDP.")
    parser.add_argument("--udp-host", default="255.255.255.255", help="UDP host or broadcast address.")
    parser.add_argument("--udp-port", type=int, default=4210, help="UDP destination port.")
    parser.add_argument("--device-mac", default="", help="Expected ESP32 MAC address. Overrides config.local.json.")
    parser.add_argument(
        "--udp-interval",
        type=float,
        default=2.0,
        help="Seconds between repeated UDP state heartbeats.",
    )
    parser.add_argument("--repeat", action="store_true", help="Print/send repeated identical states.")
    parser.add_argument("--wifi-ssid", default="", help="Configure device Wi-Fi over USB serial, then exit.")
    parser.add_argument("--wifi-password", default="", help="Wi-Fi password used with --wifi-ssid.")
    parser.add_argument("--wifi-config", type=Path, default=None, help="JSON file with ssid/password for USB Wi-Fi setup.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = load_local_config(args.config)
    device_mac = normalize_mac(args.device_mac or str(config.get("device_mac") or ""))
    device_ip = str(config.get("last_device_ip") or "")

    wifi_ssid = args.wifi_ssid
    wifi_password = args.wifi_password
    if args.wifi_config is not None:
        wifi_config = load_local_config(args.wifi_config)
        wifi_ssid = str(wifi_config.get("ssid") or "")
        wifi_password = str(wifi_config.get("password") or "")
        if not wifi_ssid:
            print("WIFI_SETUP_ERROR CONFIG_INVALID", flush=True)
            return 2
    wifi_setup_requested = bool(wifi_ssid)

    emitter = StateEmitter(
        args.serial,
        args.baud,
        args.repeat,
        args.udp,
        args.udp_host,
        args.udp_port,
        args.udp_interval,
        device_mac,
        device_ip,
        args.config,
        config,
        args.firmware_mode,
        args.serial_setup_only,
        not wifi_setup_requested,
    )

    if wifi_ssid:
        return 0 if emitter.configure_wifi(wifi_ssid, wifi_password) else 2

    ms = MonitorState()
    offsets: Dict[Path, int] = {}

    con = connect_sqlite(args.sqlite)
    last_sqlite_id = sqlite_max_id(con) if con is not None else 0

    emitter.emit(ms.state, ms.reason)

    while True:
        for path in iter_jsonl_files(args.sessions_root, args.max_age_days):
            for line in read_new_lines(path, offsets, args.from_start):
                event = json_payload(line)
                if event is not None:
                    handle_session_event(ms, event)

        if con is None:
            con = connect_sqlite(args.sqlite)
            last_sqlite_id = sqlite_max_id(con) if con is not None else last_sqlite_id
        else:
            last_sqlite_id = handle_sqlite_logs(ms, con, last_sqlite_id)

        apply_idle_rules(ms, args.quiet_timeout)
        emitter.emit(ms.state, ms.reason)
        time.sleep(args.poll)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nStopped.", flush=True)
        raise SystemExit(130)
