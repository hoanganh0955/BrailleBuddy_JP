#!/usr/bin/env python3
"""Serial CLI for BrailleBuddy_JP."""

from __future__ import annotations

import argparse
import io
import sys
import threading

try:
    import serial
    from serial.tools import list_ports
except ModuleNotFoundError:
    serial = None
    list_ports = None


def configure_utf8_console() -> None:
    """Make Japanese output reliable in Windows CMD and PowerShell."""
    if hasattr(sys.stdout, "buffer"):
        sys.stdout = io.TextIOWrapper(
            sys.stdout.buffer,
            encoding="utf-8",
            errors="replace",
            line_buffering=True,
        )
    if hasattr(sys.stderr, "buffer"):
        sys.stderr = io.TextIOWrapper(
            sys.stderr.buffer,
            encoding="utf-8",
            errors="replace",
            line_buffering=True,
        )


def print_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("シリアルポートが見つかりません。")
        return
    for port in ports:
        print(f"{port.device}  {port.description}")


def reader_loop(connection: serial.Serial, stop_event: threading.Event) -> None:
    while not stop_event.is_set():
        try:
            raw = connection.readline()
        except serial.SerialException as error:
            if not stop_event.is_set():
                print(f"通信エラー：{error}")
            stop_event.set()
            return

        if not raw:
            continue
        print(raw.decode("utf-8", errors="replace").rstrip("\r\n"))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Braille Buddy の日本語シリアルCLI",
    )
    parser.add_argument("--port", help="例：COM9")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="使用できるシリアルポートを表示する",
    )
    return parser.parse_args()


def main() -> int:
    configure_utf8_console()
    args = parse_args()

    if serial is None or list_ports is None:
        print(
            "pyserial がありません。先に pip install -r requirements.txt を実行してください。",
            file=sys.stderr,
        )
        return 2

    if args.list_ports:
        print_ports()
        return 0

    if not args.port:
        print("--port を指定してください。例：--port COM9", file=sys.stderr)
        return 2

    try:
        connection = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            timeout=0.20,
            write_timeout=1.0,
        )
    except (serial.SerialException, OSError) as error:
        print(f"{args.port} を開けません：{error}", file=sys.stderr)
        print("Arduino IDE のシリアルモニターを閉じてください。", file=sys.stderr)
        return 1

    stop_event = threading.Event()
    reader = threading.Thread(
        target=reader_loop,
        args=(connection, stop_event),
        daemon=True,
    )
    reader.start()

    print(f"{args.port} を {args.baud} baud で開きました。")
    print("終了：Ctrl+C　コマンド一覧：help")

    try:
        while not stop_event.is_set():
            try:
                command = input()
            except EOFError:
                break

            if command.strip().lower() in {"quit", "exit"}:
                break
            try:
                connection.write((command + "\n").encode("utf-8"))
                connection.flush()
            except (serial.SerialException, OSError) as error:
                print(f"送信エラー：{error}", file=sys.stderr)
                break
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        connection.close()
        reader.join(timeout=1.0)
        print("終了しました。")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
