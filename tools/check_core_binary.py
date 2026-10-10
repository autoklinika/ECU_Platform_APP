#!/usr/bin/env python3
"""Inspect uninstrumented ELF Core artifacts. Requires GNU-compatible nm/readelf."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    try:
        for tool in ("nm", "readelf"):
            if not shutil.which(tool):
                raise ValueError(f"required tool missing: {tool}")
        build = args.build.resolve()
        library = build / "src/core_v2/libecu_core_v2.a"
        if not library.is_file():
            raise ValueError(f"Core library missing: {library}")
        external = []
        for line in run("nm", "-uC", str(library)).splitlines():
            match = re.search(r"\bU\s+(.+)$", line)
            if not match:
                continue
            symbol = match.group(1).strip()
            if symbol.startswith("ecu::core::v2::"):
                continue
            if symbol in {"memcpy", "memset", "__stack_chk_fail"}:
                continue  # Permitted compiler-generated primitives, as in source V2 gate.
            external.append(symbol)
        if external:
            raise ValueError(f"unexpected external runtime symbols: {sorted(set(external))}")
        objects = [p for p in (build / "src/core_v2/CMakeFiles/ecu_core_v2.dir").rglob("*")
                   if p.is_file() and p.suffix in {".o", ".obj"}]
        if not objects:
            raise ValueError("no Core object files inspected")
        for obj in objects:
            if re.search(r"\.(?:init_array|fini_array|ctors|dtors)\b", run("readelf", "-SW", str(obj))):
                raise ValueError(f"dynamic static initialization found: {obj}")
        print(f"CORE_BINARY_INDEPENDENCE=PASS objects={len(objects)}")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"CORE_BINARY_INDEPENDENCE=FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
