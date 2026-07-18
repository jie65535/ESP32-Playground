#!/usr/bin/env python3
"""Minimal PlaygroundOS TCP heartbeat server.

This is intentionally dependency-free. It accepts the first PGOS transport
protocol messages (HELLO, heartbeat and PONG) and sends periodic PING frames.
It is a diagnostic server, not an authenticated remote shell.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import socketserver
import threading
import time
from typing import Any


def stamp() -> str:
    return dt.datetime.now().strftime("%H:%M:%S")


def print_message(peer: tuple[str, int], line: str) -> None:
    if line.startswith("PGOS/1 HELLO "):
        payload: Any = line[len("PGOS/1 HELLO ") :]
        try:
            payload = json.loads(payload)
        except json.JSONDecodeError:
            pass
        print(f"[{stamp()}] {peer} HELLO {payload}", flush=True)
        return
    try:
        payload = json.loads(line)
    except json.JSONDecodeError:
        payload = line
    if isinstance(payload, dict) and payload.get("type") == "heartbeat":
        print(
            f"[{stamp()}] {peer} heartbeat device={payload.get('device_id')} "
            f"ip={payload.get('ip')} rssi={payload.get('rssi')} "
            f"heap={payload.get('heap')}",
            flush=True,
        )
    else:
        print(f"[{stamp()}] {peer} RX {payload}", flush=True)


class PlaygroundHandler(socketserver.BaseRequestHandler):
    def handle(self) -> None:
        peer = self.client_address
        self.request.settimeout(1.0)
        buffer = bytearray()
        next_ping = time.monotonic() + 10.0
        print(f"[{stamp()}] CONNECT {peer}", flush=True)
        try:
            while True:
                try:
                    data = self.request.recv(1024)
                except TimeoutError:
                    data = None
                if data:
                    buffer.extend(data)
                    while b"\n" in buffer:
                        raw, _, remainder = buffer.partition(b"\n")
                        buffer = bytearray(remainder)
                        line = raw.decode("utf-8", errors="replace").strip()
                        if line:
                            print_message(peer, line)
                elif data is None:
                    if time.monotonic() >= next_ping:
                        self.request.sendall(b"PING\n")
                        next_ping = time.monotonic() + 10.0
                    continue
                elif data == b"":
                    break
                if time.monotonic() >= next_ping:
                    self.request.sendall(b"PING\n")
                    next_ping = time.monotonic() + 10.0
        except (ConnectionError, OSError):
            pass
        finally:
            print(f"[{stamp()}] DISCONNECT {peer}", flush=True)


class PlaygroundServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PlaygroundOS TCP diagnostic server")
    parser.add_argument("--listen", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=19000)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    with PlaygroundServer((args.listen, args.port), PlaygroundHandler) as server:
        print(
            f"PlaygroundOS server listening on {args.listen}:{args.port}; "
            "Ctrl+C to stop",
            flush=True,
        )
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nStopping.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
