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
import sys
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
    if line.startswith("ACK ") or line.startswith("STATE "):
        print(f"[{stamp()}] {peer} {line}", flush=True)
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
        server: PlaygroundServer = self.server  # type: ignore[assignment]
        server.register(self.request, peer)
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
            server.unregister(self.request)
            print(f"[{stamp()}] DISCONNECT {peer}", flush=True)


class PlaygroundServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, server_address: tuple[str, int]) -> None:
        super().__init__(server_address, PlaygroundHandler)
        self._clients: dict[object, tuple[str, int]] = {}
        self._clients_lock = threading.Lock()

    def register(self, connection: object, peer: tuple[str, int]) -> None:
        with self._clients_lock:
            self._clients[connection] = peer

    def unregister(self, connection: object) -> None:
        with self._clients_lock:
            self._clients.pop(connection, None)

    def peers(self) -> list[tuple[str, int]]:
        with self._clients_lock:
            return list(self._clients.values())

    def broadcast_command(self, request_id: int, command: str) -> int:
        payload = f"CMD {request_id} {command}\n".encode("utf-8")
        sent = 0
        with self._clients_lock:
            connections = list(self._clients)
        for connection in connections:
            try:
                connection.sendall(payload)  # type: ignore[attr-defined]
                sent += 1
            except OSError:
                self.unregister(connection)
        return sent


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PlaygroundOS TCP diagnostic server")
    parser.add_argument("--listen", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=19000)
    parser.add_argument(
        "--send",
        action="append",
        default=[],
        help="send a remote command after the first device connects",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    with PlaygroundServer((args.listen, args.port)) as server:
        print(
            f"PlaygroundOS server listening on {args.listen}:{args.port}; "
            "Ctrl+C to stop",
            flush=True,
        )
        if args.send:
            worker = threading.Thread(target=server.serve_forever, daemon=True)
            worker.start()
            deadline = time.monotonic() + 30.0
            while not server.peers() and time.monotonic() < deadline:
                time.sleep(0.1)
            if not server.peers():
                print("[HOST] no device connected within 30 seconds", flush=True)
                server.shutdown()
                return 2
            for request_id, command in enumerate(args.send, start=1):
                sent = server.broadcast_command(request_id, command)
                print(
                    f"[HOST] request={request_id} sent_to={sent}: {command}",
                    flush=True,
                )
                time.sleep(0.5)
            time.sleep(3.0)
            server.shutdown()
            return 0

        if not sys.stdin.isatty():
            try:
                server.serve_forever()
            except KeyboardInterrupt:
                print("\nStopping.")
            return 0

        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        print("Commands: up, down, ok, status, page system/display/network")
        print("Meta: /clients, /quit")
        request_id = 1
        try:
            while True:
                command = input("pgos> ").strip()
                if not command:
                    continue
                if command == "/quit":
                    break
                if command == "/clients":
                    print(server.peers())
                    continue
                sent = server.broadcast_command(request_id, command)
                print(f"[HOST] request={request_id} sent_to={sent}: {command}")
                request_id += 1
        except (EOFError, KeyboardInterrupt):
            print()
        finally:
            server.shutdown()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
