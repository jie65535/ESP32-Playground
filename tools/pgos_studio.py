#!/usr/bin/env python3
"""PlaygroundOS desktop controller.

The application owns the PGOS TCP listening socket directly.  The device
connects to it, so no serial port or separate command-line server is required
for normal wireless control.
"""

from __future__ import annotations

import argparse
from array import array
import datetime as dt
import json
import sys
import time
from typing import Any

try:
    from PySide6.QtCore import QEvent, QTimer, Qt
    from PySide6.QtGui import QCloseEvent, QImage, QKeyEvent, QPixmap, QResizeEvent
    from PySide6.QtNetwork import QAbstractSocket, QHostAddress, QTcpServer, QTcpSocket
    from PySide6.QtWidgets import (
        QApplication,
        QCheckBox,
        QComboBox,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPlainTextEdit,
        QPushButton,
        QSizePolicy,
        QSplitter,
        QStatusBar,
        QVBoxLayout,
        QWidget,
    )
except ImportError as exc:  # pragma: no cover - exercised by the launcher.
    raise SystemExit(
        "PySide6 is required. Run: "
        "python -m pip install -r tools/requirements-studio.txt"
    ) from exc


APP_STYLE = """
QWidget {
    background: #080b10;
    color: #f5f7fa;
    font-size: 13px;
}
QGroupBox {
    border: 1px solid #202a3a;
    border-radius: 10px;
    margin-top: 12px;
    padding-top: 10px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 5px;
    color: #9aa5b5;
}
QLineEdit, QPlainTextEdit {
    background: #111722;
    border: 1px solid #202a3a;
    border-radius: 7px;
    padding: 7px;
    selection-background-color: #ff654d;
}
QPushButton {
    background: #202a3a;
    border: 1px solid #2d394d;
    border-radius: 8px;
    min-height: 32px;
    padding: 4px 10px;
}
QPushButton:hover {
    border-color: #ff654d;
}
QPushButton:pressed {
    background: #ff654d;
    color: #080b10;
}
QPushButton:disabled {
    color: #586274;
    background: #111722;
}
QCheckBox {
    spacing: 7px;
}
QStatusBar {
    border-top: 1px solid #202a3a;
}
"""


def stamp() -> str:
    return dt.datetime.now().strftime("%H:%M:%S")


class PgosStudio(QMainWindow):
    MAX_LOG_BLOCKS = 1500

    def __init__(self, listen_address: str, port: int) -> None:
        super().__init__()
        self.listen_address = listen_address
        self.listen_port = port
        self.server = QTcpServer(self)
        self.server.newConnection.connect(self._accept_connections)
        self.bench_server = QTcpServer(self)
        self.bench_server.newConnection.connect(self._accept_bench_connections)
        self.mirror_server = QTcpServer(self)
        self.mirror_server.newConnection.connect(self._accept_mirror_connections)
        self.socket: QTcpSocket | None = None
        self.bench_socket: QTcpSocket | None = None
        self.mirror_socket: QTcpSocket | None = None
        self.receive_buffer = bytearray()
        self.bench_buffer = bytearray()
        self.mirror_buffer = bytearray()
        self.last_mirror_image: QImage | None = None
        self.mirror_frame_count = 0
        self.mirror_requested = False
        self.command_buttons: list[QPushButton] = []
        self.latency_button: QPushButton | None = None
        self.benchmark_buttons: list[QPushButton] = []
        self.bench_mode = ""
        self.bench_total = 0
        self.bench_transferred = 0
        self.bench_started_ns = 0
        self.bench_send_offset = 0
        self.bench_result_buffer = bytearray()
        self.benchmark_running = False
        self.bench_pattern = bytes(
            (index * 31 + 17) & 0xFF for index in range(64 * 1024)
        )
        self.ping_started_ns: int | None = None
        self.mirror_window_started = time.perf_counter()
        self.mirror_window_bytes = 0
        self.mirror_window_frames = 0
        self.request_id = 1
        self.device_payload: dict[str, Any] = {}

        self.ping_timer = QTimer(self)
        self.ping_timer.setInterval(10_000)
        self.ping_timer.timeout.connect(self._send_ping)
        self.ping_timer.start()

        self.setWindowTitle("PGOS Studio")
        self.resize(1220, 720)
        self.setMinimumSize(900, 560)
        self.setStyleSheet(APP_STYLE)
        self._build_ui()
        QApplication.instance().installEventFilter(self)
        QTimer.singleShot(0, self.start_listening)

    def _build_ui(self) -> None:
        root = QWidget(self)
        root_layout = QVBoxLayout(root)
        root_layout.setContentsMargins(12, 10, 12, 8)
        root_layout.setSpacing(9)

        connection_bar = QHBoxLayout()
        title = QLabel("PGOS Studio")
        title.setStyleSheet("font-size: 22px; font-weight: 700;")
        connection_bar.addWidget(title)
        connection_bar.addStretch(1)
        connection_bar.addWidget(QLabel("监听"))
        self.listen_edit = QLineEdit(self.listen_address)
        self.listen_edit.setFixedWidth(120)
        connection_bar.addWidget(self.listen_edit)
        self.port_edit = QLineEdit(str(self.listen_port))
        self.port_edit.setFixedWidth(72)
        connection_bar.addWidget(self.port_edit)
        self.listen_button = QPushButton("启动")
        self.listen_button.clicked.connect(self.toggle_listening)
        connection_bar.addWidget(self.listen_button)
        root_layout.addLayout(connection_bar)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.addWidget(self._build_controls())
        splitter.addWidget(self._build_mirror())
        splitter.addWidget(self._build_log())
        splitter.setSizes([270, 590, 360])
        splitter.setStretchFactor(1, 1)
        root_layout.addWidget(splitter, 1)

        self.setCentralWidget(root)
        self.setStatusBar(QStatusBar(self))
        self.statusBar().showMessage("等待设备连接")

    def _build_controls(self) -> QWidget:
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 4, 0)

        device_group = QGroupBox("设备")
        device_layout = QGridLayout(device_group)
        self.connection_value = QLabel("未连接")
        self.device_value = QLabel("--")
        self.ip_value = QLabel("--")
        self.rssi_value = QLabel("--")
        self.heap_value = QLabel("--")
        self.cpu_value = QLabel("--")
        self.psram_value = QLabel("--")
        values = (
            ("状态", self.connection_value),
            ("设备", self.device_value),
            ("IP", self.ip_value),
            ("RSSI", self.rssi_value),
            ("Heap", self.heap_value),
            ("Main CPU", self.cpu_value),
            ("PSRAM", self.psram_value),
        )
        for row, (name, value) in enumerate(values):
            label = QLabel(name)
            label.setStyleSheet("color: #9aa5b5;")
            device_layout.addWidget(label, row, 0)
            device_layout.addWidget(value, row, 1)
        layout.addWidget(device_group)

        quality_group = QGroupBox("网络质量")
        quality_layout = QGridLayout(quality_group)
        self.latency_value = QLabel("--")
        self.mirror_rate_value = QLabel("--")
        self.benchmark_value = QLabel("--")
        quality_layout.addWidget(QLabel("控制 RTT"), 0, 0)
        quality_layout.addWidget(self.latency_value, 0, 1)
        quality_layout.addWidget(QLabel("镜像速率"), 1, 0)
        quality_layout.addWidget(self.mirror_rate_value, 1, 1)
        quality_layout.addWidget(QLabel("吞吐结果"), 2, 0)
        quality_layout.addWidget(self.benchmark_value, 2, 1, 1, 2)
        self.latency_button = QPushButton("测延迟")
        self.latency_button.clicked.connect(self.measure_latency)
        self.latency_button.setEnabled(False)
        quality_layout.addWidget(self.latency_button, 0, 2)
        self.benchmark_size = QComboBox()
        self.benchmark_size.addItem("1 MiB", 1 * 1024 * 1024)
        self.benchmark_size.addItem("4 MiB", 4 * 1024 * 1024)
        self.benchmark_size.addItem("16 MiB", 16 * 1024 * 1024)
        quality_layout.addWidget(self.benchmark_size, 1, 2)
        upload_button = QPushButton("设备上行")
        upload_button.clicked.connect(self.start_benchmark_upload)
        download_button = QPushButton("设备下行")
        download_button.clicked.connect(self.start_benchmark_download)
        for button in (upload_button, download_button):
            button.setEnabled(False)
            self.benchmark_buttons.append(button)
        quality_layout.addWidget(upload_button, 3, 1)
        quality_layout.addWidget(download_button, 3, 2)
        layout.addWidget(quality_group)

        nav_group = QGroupBox("方向控制")
        nav = QGridLayout(nav_group)
        nav.addWidget(self._command_button("↑", "up"), 0, 1)
        nav.addWidget(self._command_button("←", "left"), 1, 0)
        nav.addWidget(self._command_button("确认", "ok"), 1, 1)
        nav.addWidget(self._command_button("→", "right"), 1, 2)
        nav.addWidget(self._command_button("↓", "down"), 2, 1)
        nav.addWidget(self._command_button("返回", "back"), 3, 0, 1, 2)
        nav.addWidget(self._command_button("桌面", "home"), 3, 2)
        layout.addWidget(nav_group)

        layout.addStretch(1)
        return panel

    def _build_mirror(self) -> QWidget:
        group = QGroupBox("设备屏幕")
        layout = QVBoxLayout(group)
        self.mirror_label = QLabel(
            "实时镜像通道尚未接入\n\n"
            "目标：320 × 240 RGB565\n"
            "BulkData TCP channel"
        )
        self.mirror_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.mirror_label.setMinimumSize(480, 360)
        self.mirror_label.setSizePolicy(
            QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding
        )
        self.mirror_label.setStyleSheet(
            "background: #05070a; border: 1px solid #202a3a; "
            "border-radius: 10px; color: #586274; font-size: 16px;"
        )
        layout.addWidget(self.mirror_label, 1)
        actions = QHBoxLayout()
        self.mirror_button = QPushButton("开始镜像")
        self.mirror_button.clicked.connect(self.toggle_mirror)
        self.mirror_button.setEnabled(False)
        self.push_button = QPushButton("推送画面")
        self.push_button.setEnabled(False)
        actions.addStretch(1)
        actions.addWidget(self.mirror_button)
        actions.addWidget(self.push_button)
        layout.addLayout(actions)
        self.mirror_info = QLabel("等待设备 BulkData 连接")
        self.mirror_info.setStyleSheet("color: #586274;")
        layout.addWidget(self.mirror_info)
        return group

    def _build_log(self) -> QWidget:
        group = QGroupBox("事件与日志")
        layout = QVBoxLayout(group)
        actions = QHBoxLayout()
        self.pause_logs = QCheckBox("暂停")
        clear_button = QPushButton("清空")
        clear_button.clicked.connect(self.log_view_clear)
        actions.addWidget(self.pause_logs)
        actions.addStretch(1)
        actions.addWidget(clear_button)
        layout.addLayout(actions)
        self.log_view = QPlainTextEdit()
        self.log_view.setReadOnly(True)
        self.log_view.document().setMaximumBlockCount(self.MAX_LOG_BLOCKS)
        layout.addWidget(self.log_view, 1)
        return group

    def _command_button(
        self, label: str, command: str, *grid_span: int
    ) -> QPushButton:
        del grid_span
        button = QPushButton(label)
        button.clicked.connect(lambda _checked=False, value=command: self.send_command(value))
        button.setEnabled(False)
        self.command_buttons.append(button)
        return button

    def _set_device_controls_enabled(self, enabled: bool) -> None:
        for button in self.command_buttons:
            button.setEnabled(enabled)
        self.mirror_button.setEnabled(enabled and self.mirror_server.isListening())
        if self.latency_button is not None:
            self.latency_button.setEnabled(enabled)
        for button in self.benchmark_buttons:
            button.setEnabled(enabled and self.bench_server.isListening())

    def toggle_mirror(self) -> None:
        """Ask the device to start/stop its host-owned mirror stream."""
        requested = not self.mirror_requested
        if self.send_command("mirror on" if requested else "mirror off"):
            self.mirror_requested = requested
            self.mirror_button.setText("停止镜像" if requested else "开始镜像")

    def measure_latency(self) -> None:
        self._send_ping(force=True)

    def _selected_benchmark_bytes(self) -> int:
        value = self.benchmark_size.currentData()
        return int(value) if value is not None else 4 * 1024 * 1024

    def start_benchmark_upload(self) -> None:
        if self.benchmark_running:
            return
        total = self._selected_benchmark_bytes()
        if self.send_command(f"bench upload {total}"):
            self.benchmark_value.setText("等待设备建立上行测速连接…")

    def start_benchmark_download(self) -> None:
        if self.benchmark_running:
            return
        total = self._selected_benchmark_bytes()
        if self.send_command(f"bench download {total}"):
            self.benchmark_value.setText("等待设备建立下行测速连接…")

    def toggle_listening(self) -> None:
        if self.server.isListening():
            self.stop_listening()
        else:
            self.start_listening()

    def start_listening(self) -> None:
        if self.server.isListening():
            return
        try:
            port = int(self.port_edit.text())
        except ValueError:
            QMessageBox.warning(self, "PGOS Studio", "端口必须是数字")
            return
        if not 1 <= port <= 65533:
            QMessageBox.warning(
                self, "PGOS Studio", "控制端口必须在 1 到 65533 之间"
            )
            return
        address = QHostAddress(self.listen_edit.text().strip())
        if address.isNull() or not self.server.listen(address, port):
            message = self.server.errorString() or "无法监听指定地址"
            self.append_log(f"监听失败：{message}", force=True)
            QMessageBox.warning(self, "PGOS Studio", message)
            return
        if not self.bench_server.listen(address, port + 1):
            self.benchmark_value.setText("测速端口不可用")
            self.append_log(
                f"测速端口 {port + 1} 监听失败：{self.bench_server.errorString()}",
                force=True,
            )
            QMessageBox.warning(
                self,
                "PGOS Studio",
                f"测速端口 {port + 1} 无法监听。请关闭其他测速服务器后重启 Studio。",
            )
        mirror_port = port + 2
        mirror_address = QHostAddress(self.listen_edit.text().strip())
        if mirror_port <= 65535 and not self.mirror_server.listen(
            mirror_address, mirror_port
        ):
            self.append_log(
                f"镜像端口 {mirror_port} 监听失败：{self.mirror_server.errorString()}",
                force=True,
            )
        else:
            self.append_log(f"镜像监听 {mirror_address.toString()}:{mirror_port}")
        self.listen_button.setText("停止")
        self.listen_edit.setEnabled(False)
        self.port_edit.setEnabled(False)
        self.append_log(
            f"监听 {self.server.serverAddress().toString()}:{self.server.serverPort()}",
            force=True,
        )
        self.statusBar().showMessage("监听中，等待设备连接")

    def stop_listening(self) -> None:
        self._close_socket()
        self._close_bench_socket()
        self._close_mirror_socket()
        self.server.close()
        self.bench_server.close()
        self.mirror_server.close()
        self.listen_button.setText("启动")
        self.listen_edit.setEnabled(True)
        self.port_edit.setEnabled(True)
        self.connection_value.setText("已停止")
        self._set_device_controls_enabled(False)
        self.statusBar().showMessage("监听已停止")
        self.append_log("监听已停止", force=True)

    def _accept_connections(self) -> None:
        while self.server.hasPendingConnections():
            incoming = self.server.nextPendingConnection()
            if incoming is None:
                continue
            if self.socket is not None:
                self.append_log("新的设备连接替换了旧会话", force=True)
                self._close_socket()
            self.socket = incoming
            self.receive_buffer.clear()
            incoming.readyRead.connect(self._read_socket)
            incoming.disconnected.connect(self._device_disconnected)
            peer = f"{incoming.peerAddress().toString()}:{incoming.peerPort()}"
            self.connection_value.setText("已连接")
            self.connection_value.setStyleSheet("color: #68d391; font-weight: 700;")
            self.mirror_requested = False
            self.mirror_button.setText("开始镜像")
            self._set_device_controls_enabled(True)
            self.statusBar().showMessage(f"设备已连接：{peer}")
            self.append_log(f"设备连接 {peer}", force=True)
            QTimer.singleShot(150, lambda: self.send_command("status"))

    def _accept_bench_connections(self) -> None:
        while self.bench_server.hasPendingConnections():
            incoming = self.bench_server.nextPendingConnection()
            if incoming is None:
                continue
            self._close_bench_socket()
            self.benchmark_running = False
            self.bench_socket = incoming
            self.bench_buffer.clear()
            self.bench_result_buffer.clear()
            self.bench_mode = ""
            self.bench_total = 0
            self.bench_transferred = 0
            self.bench_send_offset = 0
            incoming.readyRead.connect(self._read_bench_socket)
            incoming.bytesWritten.connect(self._pump_benchmark_download)
            incoming.disconnected.connect(self._bench_disconnected)
            self.append_log(
                f"测速连接 {incoming.peerAddress().toString()}:{incoming.peerPort()}",
                force=True,
            )

    def _read_bench_socket(self) -> None:
        if self.bench_socket is None:
            return
        self.bench_buffer.extend(bytes(self.bench_socket.readAll()))
        while True:
            if not self.bench_mode:
                marker = self.bench_buffer.find(b"\n")
                if marker < 0:
                    if len(self.bench_buffer) > 128:
                        self._fail_benchmark("header too long")
                    return
                header = bytes(self.bench_buffer[:marker]).decode(
                    "ascii", errors="replace"
                ).strip()
                del self.bench_buffer[: marker + 1]
                parts = header.split()
                if (
                    len(parts) != 3
                    or parts[0] != "PGOS_BENCH/1"
                    or parts[1] not in {"UPLOAD", "DOWNLOAD"}
                ):
                    self._fail_benchmark(f"invalid benchmark header: {header}")
                    return
                try:
                    total = int(parts[2])
                except ValueError:
                    self._fail_benchmark("invalid benchmark size")
                    return
                if total <= 0 or total > 64 * 1024 * 1024:
                    self._fail_benchmark("benchmark size out of range")
                    return
                self.bench_mode = parts[1]
                self.bench_total = total
                self.bench_transferred = 0
                self.bench_send_offset = 0
                self.bench_started_ns = time.perf_counter_ns()
                self.benchmark_running = True
                self.append_log(
                    f"测速开始 {self.bench_mode.lower()} {total / 1024 / 1024:.1f} MiB",
                    force=True,
                )
                if self.bench_mode == "DOWNLOAD":
                    self._pump_benchmark_download()

            if self.bench_mode == "UPLOAD":
                remaining = self.bench_total - self.bench_transferred
                if remaining > 0 and self.bench_buffer:
                    count = min(remaining, len(self.bench_buffer))
                    del self.bench_buffer[:count]
                    self.bench_transferred += count
                if self.bench_transferred < self.bench_total:
                    return
                elapsed_us = max(
                    1, (time.perf_counter_ns() - self.bench_started_ns) // 1000
                )
                if self.bench_socket is not None:
                    self.bench_socket.write(
                        f"RESULT {self.bench_total} {elapsed_us}\n".encode("ascii")
                    )
                self._finish_benchmark("上行", elapsed_us, elapsed_us)
                return

            if self.bench_mode == "DOWNLOAD":
                if self.bench_send_offset < self.bench_total:
                    self._pump_benchmark_download()
                    return
                marker = self.bench_buffer.find(b"\n")
                if marker < 0:
                    return
                result = bytes(self.bench_buffer[:marker]).decode(
                    "ascii", errors="replace"
                ).strip()
                del self.bench_buffer[: marker + 1]
                parts = result.split()
                if len(parts) != 3 or parts[0] != "RESULT":
                    self._fail_benchmark("invalid download result")
                    return
                try:
                    received = int(parts[1])
                    device_elapsed_us = int(parts[2])
                except ValueError:
                    self._fail_benchmark("invalid download metrics")
                    return
                if received != self.bench_total or device_elapsed_us <= 0:
                    self._fail_benchmark("download result mismatch")
                    return
                host_elapsed_us = max(
                    1, (time.perf_counter_ns() - self.bench_started_ns) // 1000
                )
                self._finish_benchmark(
                    "下行", device_elapsed_us, host_elapsed_us
                )
                return

            return

    def _pump_benchmark_download(self, _written: int = 0) -> None:
        if self.bench_socket is None or self.bench_mode != "DOWNLOAD":
            return
        max_queued = 256 * 1024
        while (
            self.bench_send_offset < self.bench_total
            and self.bench_socket.bytesToWrite() < max_queued
        ):
            remaining = self.bench_total - self.bench_send_offset
            count = min(len(self.bench_pattern), remaining)
            written = self.bench_socket.write(self.bench_pattern[:count])
            if written <= 0:
                self._fail_benchmark("download socket write failed")
                return
            self.bench_send_offset += written

    def _finish_benchmark(
        self, direction: str, device_elapsed_us: int, host_elapsed_us: int
    ) -> None:
        device_mbps = self.bench_total * 8.0 / device_elapsed_us
        host_mbps = self.bench_total * 8.0 / max(1, host_elapsed_us)
        self.benchmark_value.setText(
            f"{direction} {device_mbps:.2f} Mbps · host {host_mbps:.2f}"
        )
        self.append_log(
            f"测速完成 {direction} device={device_mbps:.2f} Mbps "
            f"host={host_mbps:.2f} Mbps",
            force=True,
        )
        self.benchmark_running = False
        if self.bench_socket is not None:
            self.bench_socket.disconnectFromHost()
        self.bench_mode = ""

    def _fail_benchmark(self, message: str) -> None:
        self.append_log(f"测速失败：{message}", force=True)
        self.benchmark_value.setText("失败")
        self.benchmark_running = False
        self.bench_mode = ""
        self._close_bench_socket()

    def _accept_mirror_connections(self) -> None:
        while self.mirror_server.hasPendingConnections():
            incoming = self.mirror_server.nextPendingConnection()
            if incoming is None:
                continue
            self._close_mirror_socket()
            self.mirror_socket = incoming
            self.mirror_buffer.clear()
            self.mirror_window_started = time.perf_counter()
            self.mirror_window_bytes = 0
            self.mirror_window_frames = 0
            self.mirror_requested = True
            self.mirror_button.setText("停止镜像")
            incoming.readyRead.connect(self._read_mirror_socket)
            incoming.disconnected.connect(self._mirror_disconnected)
            self.mirror_info.setText("镜像通道已连接，等待帧")
            self.append_log("屏幕 BulkData 通道已连接", force=True)

    def _read_socket(self) -> None:
        if self.socket is None:
            return
        self.receive_buffer.extend(bytes(self.socket.readAll()))
        while b"\n" in self.receive_buffer:
            raw, _, remainder = self.receive_buffer.partition(b"\n")
            self.receive_buffer = bytearray(remainder)
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                self._handle_line(line)

    def _read_mirror_socket(self) -> None:
        if self.mirror_socket is None:
            return
        data = bytes(self.mirror_socket.readAll())
        self.mirror_window_bytes += len(data)
        self.mirror_buffer.extend(data)
        while True:
            if len(self.mirror_buffer) < 22:
                return
            if not self.mirror_buffer.startswith(b"PGMF"):
                marker = self.mirror_buffer.find(b"PGMF", 1)
                if marker < 0:
                    del self.mirror_buffer[:-3]
                    return
                del self.mirror_buffer[:marker]
                continue

            version = self.mirror_buffer[4]
            frame_type = self.mirror_buffer[5]
            width = int.from_bytes(self.mirror_buffer[6:8], "big")
            height = int.from_bytes(self.mirror_buffer[8:10], "big")
            payload_length = int.from_bytes(self.mirror_buffer[10:14], "big")
            frame_id = int.from_bytes(self.mirror_buffer[14:18], "big")
            if (
                version != 1
                or frame_type != 1
                or width <= 0
                or height <= 0
                or payload_length != width * height * 2
                or payload_length > 4 * 1024 * 1024
            ):
                del self.mirror_buffer[:4]
                self.append_log("丢弃无效镜像帧头", force=True)
                continue
            frame_length = 22 + payload_length
            if len(self.mirror_buffer) < frame_length:
                return
            payload = bytes(self.mirror_buffer[22:frame_length])
            del self.mirror_buffer[:frame_length]
            self._display_rgb565_frame(payload, width, height, frame_id)

    def _display_rgb565_frame(
        self, payload: bytes, width: int, height: int, frame_id: int
    ) -> None:
        pixels = array("H")
        pixels.frombytes(payload)
        if sys.byteorder == "little":
            pixels.byteswap()
        image = QImage(
            pixels.tobytes(), width, height, width * 2, QImage.Format.Format_RGB16
        ).copy()
        self.last_mirror_image = image
        self.mirror_frame_count += 1
        self.mirror_window_frames += 1
        self._refresh_mirror_pixmap()
        elapsed = time.perf_counter() - self.mirror_window_started
        if elapsed >= 0.5:
            mbps = self.mirror_window_bytes * 8.0 / elapsed / 1_000_000.0
            fps = self.mirror_window_frames / elapsed
            self.mirror_rate_value.setText(f"{fps:.1f} FPS · {mbps:.2f} Mbps")
            self.mirror_window_started = time.perf_counter()
            self.mirror_window_bytes = 0
            self.mirror_window_frames = 0
        self.mirror_info.setText(
            f"实时镜像 · {width}×{height} · frame {frame_id} · "
            f"received {self.mirror_frame_count}"
        )

    def _refresh_mirror_pixmap(self) -> None:
        if self.last_mirror_image is None:
            return
        pixmap = QPixmap.fromImage(self.last_mirror_image)
        pixmap = pixmap.scaled(
            self.mirror_label.size(),
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        )
        self.mirror_label.setText("")
        self.mirror_label.setPixmap(pixmap)

    def resizeEvent(self, event: QResizeEvent) -> None:
        super().resizeEvent(event)
        self._refresh_mirror_pixmap()

    def _handle_line(self, line: str) -> None:
        if line.startswith("PGOS/1 HELLO "):
            payload = self._json_payload(line[len("PGOS/1 HELLO ") :])
            if isinstance(payload, dict):
                self.device_payload.update(payload)
                self.device_value.setText(str(payload.get("device_id", "--")))
                self.ip_value.setText(str(payload.get("ip", "--")))
            self.append_log(f"HELLO {payload}", force=True)
            return
        if line.startswith("STATE "):
            parts = line.split(" ", 2)
            payload = self._json_payload(parts[2]) if len(parts) == 3 else line
            if isinstance(payload, dict):
                self._update_status(payload)
            self.append_log(f"STATE {payload}")
            return
        if line.startswith("ACK "):
            self.append_log(line)
            return
        if line == "PONG":
            if self.ping_started_ns is not None:
                elapsed_ms = (
                    time.perf_counter_ns() - self.ping_started_ns
                ) / 1_000_000.0
                self.latency_value.setText(f"{elapsed_ms:.1f} ms")
                self.ping_started_ns = None
            return
        payload = self._json_payload(line)
        if isinstance(payload, dict) and payload.get("type") == "heartbeat":
            self._update_status(payload)
            return
        self.append_log(f"RX {payload}")

    @staticmethod
    def _json_payload(value: str) -> Any:
        try:
            return json.loads(value)
        except json.JSONDecodeError:
            return value

    def _update_status(self, payload: dict[str, Any]) -> None:
        self.device_payload.update(payload)
        if payload.get("device_id"):
            self.device_value.setText(str(payload["device_id"]))
        if payload.get("ip"):
            self.ip_value.setText(str(payload["ip"]))
        if payload.get("rssi") is not None:
            self.rssi_value.setText(f"{payload['rssi']} dBm")
        if payload.get("heap") is not None:
            self.heap_value.setText(f"{int(payload['heap']) // 1024} KiB")
        if payload.get("main_loop_busy") is not None:
            self.cpu_value.setText(f"{int(payload['main_loop_busy'])}%")
        if payload.get("free_psram") is not None:
            self.psram_value.setText(f"{int(payload['free_psram']) // 1024} KiB free")
        app = payload.get("app")
        if app:
            self.statusBar().showMessage(f"设备在线 · 当前页面：{app}")

    def send_command(self, command: str) -> bool:
        if self.socket is None or self.socket.state() != QAbstractSocket.SocketState.ConnectedState:
            self.statusBar().showMessage("没有设备连接")
            self.append_log(f"未发送：{command}（设备未连接）")
            return False
        request_id = self.request_id
        self.request_id += 1
        frame = f"CMD {request_id} {command}\n".encode("utf-8")
        if self.socket.write(frame) < 0:
            self.append_log(f"发送失败：{command}", force=True)
            return False
        self.append_log(f"TX #{request_id} {command}")
        return True

    def _send_ping(self, force: bool = False) -> None:
        if (
            self.socket is not None
            and self.socket.state()
            == QAbstractSocket.SocketState.ConnectedState
            and (force or self.ping_started_ns is None)
        ):
            self.ping_started_ns = time.perf_counter_ns()
            self.socket.write(b"PING\n")

    def _device_disconnected(self) -> None:
        sender = self.sender()
        if self.socket is not None and sender is not self.socket:
            return
        self.append_log("设备断开；等待自动重连", force=True)
        self.connection_value.setText("等待重连")
        self.connection_value.setStyleSheet("color: #f6ad55; font-weight: 700;")
        self._set_device_controls_enabled(False)
        self.statusBar().showMessage("设备断开，服务器仍在监听")
        if self.socket is not None:
            self.socket.deleteLater()
        self.socket = None
        self.receive_buffer.clear()

    def _close_socket(self) -> None:
        if self.socket is None:
            return
        socket = self.socket
        self.socket = None
        self.receive_buffer.clear()
        try:
            socket.disconnected.disconnect(self._device_disconnected)
        except RuntimeError:
            pass
        socket.disconnectFromHost()
        socket.close()
        socket.deleteLater()

    def _bench_disconnected(self) -> None:
        sender = self.sender()
        if self.bench_socket is not None and sender is not self.bench_socket:
            return
        if self.benchmark_running:
            self._fail_benchmark("测速连接断开")
        self.bench_socket = None
        self.bench_buffer.clear()

    def _close_bench_socket(self) -> None:
        if self.bench_socket is None:
            return
        socket = self.bench_socket
        self.bench_socket = None
        self.bench_buffer.clear()
        try:
            socket.disconnected.disconnect(self._bench_disconnected)
        except RuntimeError:
            pass
        try:
            socket.readyRead.disconnect(self._read_bench_socket)
        except RuntimeError:
            pass
        try:
            socket.bytesWritten.disconnect(self._pump_benchmark_download)
        except RuntimeError:
            pass
        socket.disconnectFromHost()
        socket.close()
        socket.deleteLater()

    def _mirror_disconnected(self) -> None:
        sender = self.sender()
        if self.mirror_socket is not None and sender is not self.mirror_socket:
            return
        self.append_log("屏幕 BulkData 通道断开；等待设备重连", force=True)
        self.mirror_info.setText("镜像通道断开，等待重连")
        if self.mirror_socket is not None:
            self.mirror_socket.deleteLater()
        self.mirror_socket = None
        self.mirror_buffer.clear()

    def _close_mirror_socket(self) -> None:
        if self.mirror_socket is None:
            return
        socket = self.mirror_socket
        self.mirror_socket = None
        self.mirror_buffer.clear()
        try:
            socket.disconnected.disconnect(self._mirror_disconnected)
        except RuntimeError:
            pass
        socket.disconnectFromHost()
        socket.close()
        socket.deleteLater()

    def log_view_clear(self) -> None:
        self.log_view.clear()

    def append_log(self, message: str, force: bool = False) -> None:
        if self.pause_logs.isChecked() and not force:
            return
        self.log_view.appendPlainText(f"[{stamp()}] {message}")

    def eventFilter(self, watched: object, event: QEvent) -> bool:
        del watched
        if event.type() != QEvent.Type.KeyPress:
            return False
        focus = QApplication.focusWidget()
        if isinstance(focus, (QLineEdit, QPlainTextEdit)):
            return False
        key_event = event
        if not isinstance(key_event, QKeyEvent):
            return False
        mapping = {
            Qt.Key.Key_Up: "up",
            Qt.Key.Key_Down: "down",
            Qt.Key.Key_Left: "left",
            Qt.Key.Key_Right: "right",
            Qt.Key.Key_Return: "ok",
            Qt.Key.Key_Enter: "ok",
            Qt.Key.Key_Backspace: "back",
            Qt.Key.Key_Escape: "back",
            Qt.Key.Key_Home: "home",
        }
        command = mapping.get(key_event.key())
        if command is None:
            return False
        self.send_command(command)
        return True

    def closeEvent(self, event: QCloseEvent) -> None:
        self.stop_listening()
        QApplication.instance().removeEventFilter(self)
        event.accept()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PlaygroundOS desktop controller")
    parser.add_argument("--listen", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=19000)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    app = QApplication(sys.argv[:1])
    app.setApplicationName("PGOS Studio")
    window = PgosStudio(args.listen, args.port)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
