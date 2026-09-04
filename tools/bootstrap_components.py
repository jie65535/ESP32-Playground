#!/usr/bin/env python3
"""Restore the pinned ESP-IDF component snapshots used by PlaygroundOS.

The large upstream trees intentionally stay out of Git.  This script restores
them from the commits recorded in dependencies/components.lock.json and then
applies the small, reviewed compatibility patches kept under dependencies/.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LOCK_PATH = ROOT / "dependencies" / "components.lock.json"
COMPONENTS = ROOT / "components"
PATCHES = ROOT / "dependencies" / "patches"


def run(*args: str, cwd: Path | None = None) -> None:
    print("+", " ".join(args))
    subprocess.run(args, cwd=cwd, check=True)


def load_revisions() -> dict[str, str]:
    data = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    return {
        entry["name"]: entry["commit"]
        for entry in data["components"]
        if "commit" in entry
    }


def output(*args: str, cwd: Path | None = None) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def checkout_commit(repo: str, commit: str, destination: Path) -> Path:
    """Check out one immutable commit without downloading repository history."""
    destination.mkdir()
    run("git", "init", "--quiet", cwd=destination)
    run("git", "remote", "add", "origin", repo, cwd=destination)
    run(
        "git",
        "fetch",
        "--quiet",
        "--depth",
        "1",
        "--no-tags",
        "origin",
        commit,
        cwd=destination,
    )
    run("git", "checkout", "--quiet", "--detach", "FETCH_HEAD", cwd=destination)
    actual = output("git", "rev-parse", "HEAD", cwd=destination)
    if actual != commit:
        raise RuntimeError(f"Expected {commit} from {repo}, got {actual}")
    return destination


def copy_tree(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        source,
        destination,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns(".git"),
    )


def remove_tree(path: Path, force: bool) -> None:
    if not path.exists():
        return
    if not force:
        raise RuntimeError(
            f"Refusing to overwrite existing dependency tree: {path}. "
            "Remove it or rerun with --force."
        )
    shutil.rmtree(path)


def apply_patch(component: Path, patch_name: str) -> None:
    run("git", "apply", str(PATCHES / patch_name), cwd=component)


def restore(force: bool, components: Path) -> None:
    revisions = load_revisions()
    required = {"arduino", "bluepad32", "btstack", "lvgl"}
    missing = required.difference(revisions)
    if missing:
        raise RuntimeError(f"Dependency lock is missing: {', '.join(sorted(missing))}")

    components.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="pgos-components-") as temp_name:
        temp = Path(temp_name)

        # Arduino and LVGL are ordinary repositories at their locked commits.
        arduino_repo = checkout_commit(
            "https://github.com/espressif/arduino-esp32.git",
            revisions["arduino"],
            temp / "arduino",
        )
        remove_tree(components / "arduino", force)
        copy_tree(arduino_repo, components / "arduino")
        apply_patch(components / "arduino", "arduino.patch")
        shutil.copy2(
            ROOT / "dependencies" / "compat" / "arduino" / "idf_component.yml",
            components / "arduino" / "idf_component.yml",
        )

        lvgl_repo = checkout_commit(
            "https://github.com/lvgl/lvgl.git",
            revisions["lvgl"],
            temp / "lvgl",
        )
        remove_tree(components / "lvgl", force)
        copy_tree(lvgl_repo, components / "lvgl")

        # Restore the Bluepad32 component and the BTstack source revision used
        # by its ESP32 port, then apply the PGOS compatibility deltas.
        bluepad_repo = checkout_commit(
            "https://github.com/ricardoquesada/bluepad32.git",
            revisions["bluepad32"],
            temp / "bluepad32",
        )
        run(
            "git",
            "submodule",
            "update",
            "--init",
            "--depth",
            "1",
            "external/btstack",
            cwd=bluepad_repo,
        )
        btstack_repo = bluepad_repo / "external" / "btstack"
        actual_btstack = output("git", "rev-parse", "HEAD", cwd=btstack_repo)
        if actual_btstack != revisions["btstack"]:
            raise RuntimeError(
                f"Expected BTstack {revisions['btstack']}, got {actual_btstack}"
            )

        remove_tree(components / "bluepad32", force)
        copy_tree(
            bluepad_repo / "src" / "components" / "bluepad32",
            components / "bluepad32",
        )
        apply_patch(components / "bluepad32", "bluepad32.patch")

        btstack_destination = components / "btstack"
        remove_tree(btstack_destination, force)
        btstack_destination.mkdir(parents=True)
        copy_tree(
            btstack_repo / "port" / "esp32" / "components" / "btstack",
            btstack_destination,
        )
        for relative in (
            "src",
            "platform/freertos",
            "platform/lwip",
            "3rd-party/bluedroid",
            "3rd-party/hxcmod-player",
            "3rd-party/lc3-google",
            "3rd-party/lwip/dhcp-server",
            "3rd-party/md5",
            "3rd-party/micro-ecc",
            "3rd-party/yxml",
        ):
            copy_tree(btstack_repo / relative, btstack_destination / relative)
        embedded_destination = btstack_destination / "platform" / "embedded"
        embedded_destination.mkdir(parents=True)
        for filename in (
            "hal_time_ms.h",
            "hal_uart_dma.h",
            "hci_dump_embedded_stdout.c",
            "hci_dump_embedded_stdout.h",
        ):
            shutil.copy2(
                btstack_repo / "platform" / "embedded" / filename,
                embedded_destination / filename,
            )
        apply_patch(btstack_destination, "btstack.patch")

        # The adapter is a small PGOS-specific IDF component assembled from
        # bluepad32-arduino.  Keep it tracked instead of cloning a whole repo.
        remove_tree(components / "bluepad32_arduino", force)
        copy_tree(
            ROOT / "dependencies" / "compat" / "bluepad32_arduino",
            components / "bluepad32_arduino",
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace existing ignored component trees",
    )
    parser.add_argument(
        "--destination",
        type=Path,
        default=COMPONENTS,
        help=argparse.SUPPRESS,
    )
    args = parser.parse_args()
    restore(args.force, args.destination.resolve())
    print("Restored pinned ESP-IDF components")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
