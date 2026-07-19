"""Prevent stale incremental firmware in pioarduino's IDF build.

The ESP-IDF integration compiles project sources into PIOBUILDFILES, but with
this Arduino-as-component layout SCons can consider firmware.elf current after
one of those objects changes.  The integration's middleware also filters an
explicit object dependency, so compare project inputs with the ELF and force
only the required link.
"""

from pathlib import Path

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


program = env.get("PIOMAINPROG")
elf_path = Path(env.subst("$PROGPATH"))
project_dir = Path(env.subst("$PROJECT_DIR"))
input_roots = [project_dir / "src", project_dir / "include", project_dir / "main"]
input_suffixes = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}

latest_input_mtime = 0.0
for root in input_roots:
    if not root.is_dir():
        continue
    for source in root.rglob("*"):
        if source.is_file() and source.suffix.lower() in input_suffixes:
            latest_input_mtime = max(latest_input_mtime, source.stat().st_mtime)

if program and (
    not elf_path.is_file() or latest_input_mtime > elf_path.stat().st_mtime
):
    env.AlwaysBuild(program)
    print("PGOS incremental guard: project input changed, relinking firmware")
