#!/usr/bin/env python3
"""Dependency-free PGOS TCP throughput test server."""

from __future__ import annotations

import argparse
import socket
import time


BUFFER_SIZE = 64 * 1024


def read_line(connection: socket.socket) -> str:
    data = bytearray()
    while True:
        byte = connection.recv(1)
        if not byte:
            raise ConnectionError("connection closed while reading header")
        if byte == b"\n":
            return data.decode("ascii", errors="replace").strip()
        data.extend(byte)
        if len(data) > 128:
            raise ValueError("benchmark header too long")


def run_session(connection: socket.socket, peer: tuple[str, int]) -> None:
    connection.settimeout(30.0)
    header = read_line(connection)
    parts = header.split()
    if len(parts) != 3 or parts[0] != "PGOS_BENCH/1":
        raise ValueError(f"unsupported header: {header}")
    direction = parts[1]
    total = int(parts[2])
    if direction not in {"UPLOAD", "DOWNLOAD"} or total <= 0:
        raise ValueError(f"invalid benchmark request: {header}")

    print(f"[bench] {peer} {direction} {total} bytes", flush=True)
    if direction == "UPLOAD":
        remaining = total
        received = 0
        started = time.perf_counter_ns()
        while remaining:
            data = connection.recv(min(BUFFER_SIZE, remaining))
            if not data:
                raise ConnectionError("device closed upload")
            received += len(data)
            remaining -= len(data)
        elapsed_us = max(1, (time.perf_counter_ns() - started) // 1000)
        mbps = received * 8.0 / elapsed_us
        connection.sendall(f"RESULT {received} {elapsed_us}\n".encode("ascii"))
        print(
            f"[bench] upload received={received} elapsed_us={elapsed_us} "
            f"mbps={mbps:.2f}",
            flush=True,
        )
        return

    pattern = bytes((index * 31 + 17) & 0xFF for index in range(BUFFER_SIZE))
    remaining = total
    sent = 0
    started = time.perf_counter_ns()
    while remaining:
        chunk = pattern[: min(len(pattern), remaining)]
        connection.sendall(chunk)
        sent += len(chunk)
        remaining -= len(chunk)
    server_elapsed_us = max(1, (time.perf_counter_ns() - started) // 1000)
    result = read_line(connection)
    parts = result.split()
    if len(parts) != 3 or parts[0] != "RESULT":
        raise ValueError(f"invalid device result: {result}")
    device_elapsed_us = int(parts[2])
    print(
        f"[bench] download sent={sent} server_elapsed_us={server_elapsed_us} "
        f"device_elapsed_us={device_elapsed_us} "
        f"device_mbps={sent * 8.0 / max(1, device_elapsed_us):.2f}",
        flush=True,
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PlaygroundOS throughput server")
    parser.add_argument("--listen", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=19001)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((args.listen, args.port))
        server.listen(1)
        print(f"PGOS benchmark listening on {args.listen}:{args.port}", flush=True)
        while True:
            connection, peer = server.accept()
            with connection:
                try:
                    run_session(connection, peer)
                except (ConnectionError, OSError, ValueError) as exc:
                    print(f"[bench] {peer} error: {exc}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
