#!/usr/bin/env python3
"""Host client for the Ersa Wearable NDJSON USB control protocol."""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from typing import Any, Iterable, Optional

from rich import box
from rich.console import Console
from rich.panel import Panel
from rich.table import Table

PROTOCOL_VERSION = 1
MAX_FRAME_BYTES = 4096  # Host-side ceiling; firmware is expected to enforce 512 B.
DEFAULT_BAUD = 115200
console = Console()
COMMANDS = {
    "status": "system.status",
    "battery": "battery.read",
    "ble": "ble.status",
    "power": "power.status",
    "logs": "logs.read",
}


class EwctlError(Exception):
    """A user-facing connection or protocol error."""


def encode_request(request_id: int, command: str, args: Optional[dict] = None) -> bytes:
    if not command or "\n" in command or "\r" in command:
        raise EwctlError("command must be a non-empty single-line name")
    request: dict[str, Any] = {"v": PROTOCOL_VERSION, "id": request_id, "cmd": command}
    if args:
        request["args"] = args
    frame = json.dumps(request, separators=(",", ":"), ensure_ascii=True).encode("utf-8") + b"\n"
    if len(frame) > MAX_FRAME_BYTES:
        raise EwctlError("request exceeds host frame limit")
    return frame


def decode_response(line: bytes) -> Optional[dict]:
    """Decode a reply, returning None for console chatter or malformed lines."""
    if len(line) > MAX_FRAME_BYTES:
        return None
    try:
        value = json.loads(line.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None
    return value if isinstance(value, dict) else None


def find_port(explicit: Optional[str]) -> str:
    if explicit:
        return explicit
    try:
        from serial.tools import list_ports
    except ImportError as exc:
        raise EwctlError("pyserial is required; install with: python3 -m pip install -r requirements-ewctl.txt") from exc

    ports = list(list_ports.comports())
    # ESP32-C3 native USB Serial/JTAG commonly enumerates as Espressif VID 0x303A.
    esp = [p.device for p in ports if p.vid == 0x303A]
    if len(esp) == 1:
        return esp[0]
    if len(esp) > 1:
        raise EwctlError("multiple Espressif serial devices found; pass --port explicitly")
    candidates = [p.device for p in ports if p.device.startswith(("/dev/ttyACM", "/dev/ttyUSB"))]
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise EwctlError("no serial device found; connect the watch or pass --port")
    raise EwctlError("multiple serial devices found; pass --port explicitly")


class Session:
    def __init__(self, port: str, baud: int, timeout: float):
        try:
            import serial
        except ImportError as exc:
            raise EwctlError("pyserial is required; install with: python3 -m pip install -r requirements-ewctl.txt") from exc
        try:
            self.serial = serial.Serial(port=port, baudrate=baud, timeout=0.15, write_timeout=timeout)
        except Exception as exc:  # pyserial exposes platform-specific exception classes.
            raise EwctlError(f"could not open {port}: {exc}") from exc
        self.timeout = timeout
        self.next_id = int(time.monotonic_ns() & 0x7FFFFFFF) or 1

    def close(self) -> None:
        self.serial.close()

    def request(self, command: str, args: Optional[dict] = None) -> dict:
        request_id = self.next_id
        self.next_id = (self.next_id + 1) & 0x7FFFFFFF or 1
        frame = encode_request(request_id, command, args)
        try:
            self.serial.write(frame)
            self.serial.flush()
        except Exception as exc:
            raise EwctlError(f"write failed: {exc}") from exc

        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            try:
                line = self.serial.readline(MAX_FRAME_BYTES + 1)
            except Exception as exc:
                raise EwctlError(f"read failed: {exc}") from exc
            if not line:
                continue
            if len(line) > MAX_FRAME_BYTES and not line.endswith(b"\n"):
                # Skip this chunk without an unbounded drain loop. Its eventual
                # tail will fail JSON parsing; the outer deadline remains active.
                continue
            reply = decode_response(line.strip())
            if reply is None or reply.get("id") != request_id:
                continue
            if reply.get("v") != PROTOCOL_VERSION:
                raise EwctlError(f"device replied with unsupported protocol version: {reply.get('v')!r}")
            return reply
        raise EwctlError(f"timeout waiting for reply to {command!r}; is the watch firmware control bridge enabled?")


def parse_args_json(text: Optional[str]) -> Optional[dict]:
    if text is None:
        return None
    try:
        value = json.loads(text)
    except json.JSONDecodeError as exc:
        raise EwctlError(f"invalid --args JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise EwctlError("--args must be a JSON object")
    return value


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ewctl", description="Inspect an Ersa Wearable over its USB serial control bridge.")
    parser.add_argument("--port", help="serial port (auto-detects a single ESP32 device)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="serial baud rate (default: %(default)s)")
    parser.add_argument("--timeout", type=float, default=3.0, help="per-command timeout in seconds")
    parser.add_argument("--json", action="store_true", help="print machine-readable JSON instead of Rich output")
    sub = parser.add_subparsers(dest="operation", required=True)
    for alias in ("status", "battery", "ble"):
        item = sub.add_parser(alias, help=f"request {COMMANDS[alias]}")
        item.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    power = sub.add_parser("power", help="inspect or test CPU power settings")
    power.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    power_actions = power.add_subparsers(dest="power_action")
    get_frequency = power_actions.add_parser("cpu-freq-get", help="show measured CPU frequency and override")
    get_frequency.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    set_frequency = power_actions.add_parser("cpu-freq-set", help="temporarily force CPU frequency (0 restores automatic scaling)")
    set_frequency.add_argument("mhz", choices=("0", "40", "80", "160"))
    set_frequency.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    logs = sub.add_parser("logs", help=f"request {COMMANDS['logs']}")
    logs.add_argument("--follow", action="store_true", help="poll new records until Ctrl-C")
    logs.add_argument("--interval", type=float, default=0.1, help="seconds between polls when caught up (default: %(default)s)")
    logs.add_argument("--batch-size", type=int, default=4, choices=range(1, 5), help="records requested per USB transaction (default: %(default)s)")
    logs.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    raw = sub.add_parser("command", help="send a protocol command")
    raw.add_argument("name")
    raw.add_argument("--args", help="command arguments as a JSON object")
    raw.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    poll = sub.add_parser("poll", help="poll one or more status commands without reopening USB")
    poll.add_argument("commands", nargs="+", choices=tuple(COMMANDS))
    poll.add_argument("--interval", type=float, default=5.0, help="seconds between polls (minimum 0.5)")
    poll.add_argument("--count", type=int, default=0, help="number of polls; 0 runs until Ctrl-C")
    poll.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print machine-readable JSON")
    return parser


def print_json(value: Any) -> None:
    print(json.dumps(value, ensure_ascii=False, indent=2), flush=True)


def display_reply(reply: dict, title: str, json_output: bool = False) -> None:
    if json_output:
        print_json(reply)
        return
    if not reply.get("ok"):
        error = reply.get("error", {})
        console.print(Panel(f"[bold red]{error.get('code', 'error')}[/]  {error.get('message', 'request failed')}", title=title, border_style="red"))
        return
    data = reply.get("data", {})
    if isinstance(data, dict) and isinstance(data.get("records"), list):
        records = data["records"]
        if not records:
            console.print("[dim]No new log records.[/]")
        for record in records:
            if isinstance(record, dict):
                console.print(f"[dim]{record.get('sequence', '')}[/] {record.get('line', '')}", markup=False)
        return
    table = Table(box=box.SIMPLE, show_header=False, pad_edge=False)
    table.add_column("Field", style="cyan", no_wrap=True)
    table.add_column("Value", overflow="fold")
    if isinstance(data, dict):
        for key, value in data.items():
            rendered = json.dumps(value, ensure_ascii=False) if isinstance(value, (dict, list)) else str(value)
            table.add_row(key.replace("_", " "), rendered)
    else:
        table.add_row("result", str(data))
    console.print(Panel(table, title=f"[bold]{title}[/]", border_style="blue", expand=False))


def run(args: argparse.Namespace) -> int:
    if args.timeout <= 0:
        raise EwctlError("--timeout must be positive")
    if args.operation == "poll":
        if args.interval < 0.5:
            raise EwctlError("--interval must be at least 0.5 seconds")
        if args.count < 0:
            raise EwctlError("--count cannot be negative")

    port = find_port(args.port)
    session = Session(port, args.baud, args.timeout)
    print(f"ewctl: connected to {port}", file=sys.stderr)
    try:
        if args.operation == "poll":
            iteration = 0
            while args.count == 0 or iteration < args.count:
                started = time.monotonic()
                for alias in args.commands:
                    reply = session.request(COMMANDS[alias])
                    if args.json:
                        print_json({"observed_at": datetime.now(timezone.utc).isoformat(), "command": alias, "reply": reply})
                    else:
                        display_reply(reply, f"{alias} · {datetime.now(timezone.utc).astimezone().strftime('%H:%M:%S')}")
                iteration += 1
                if args.count == 0 or iteration < args.count:
                    time.sleep(max(0.0, args.interval - (time.monotonic() - started)))
            return 0

        if args.operation == "logs" and args.follow:
            if args.interval < 0.05:
                raise EwctlError("--interval must be at least 0.05 seconds")
            cursor = None
            while True:
                request_args = {"limit": args.batch_size}
                if cursor is not None:
                    request_args["cursor"] = cursor
                reply = session.request(COMMANDS["logs"], request_args)
                display_reply(reply, "logs", args.json)
                data = reply.get("data")
                if isinstance(data, dict) and isinstance(data.get("next_cursor"), int):
                    cursor = data["next_cursor"]
                records = data.get("records", []) if isinstance(data, dict) else []
                if not isinstance(records, list) or len(records) < args.batch_size:
                    time.sleep(args.interval)

        if args.operation == "power":
            if args.power_action == "cpu-freq-get":
                command, command_args = "power.cpu-freq-get", None
            elif args.power_action == "cpu-freq-set":
                command, command_args = "power.cpu-freq-set", {"cpu_mhz": int(args.mhz)}
            else:
                command, command_args = "power.status", None
        elif args.operation == "logs":
            command, command_args = COMMANDS["logs"], {"limit": args.batch_size}
        else:
            command = COMMANDS[args.operation] if args.operation in COMMANDS else args.name
            command_args = parse_args_json(getattr(args, "args", None))
        reply = session.request(command, command_args)
        display_reply(reply, args.operation if args.operation != "command" else command, args.json)
        return 0 if reply.get("ok") else 2
    finally:
        session.close()


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return run(args)
    except KeyboardInterrupt:
        print("ewctl: interrupted", file=sys.stderr)
        return 130
    except EwctlError as exc:
        print(f"ewctl: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
