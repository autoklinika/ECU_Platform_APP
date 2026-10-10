#!/usr/bin/env python3
"""Verify the recorded byte-exact Core import offline; never rewrites it."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys


def verify(root: Path) -> int:
    manifest = json.loads((root / "provenance/core_v2_import.json").read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError("unsupported provenance schema")
    if manifest.get("source_repository") != "autoklinika/ECU_Platrorm_V2":
        raise ValueError("unexpected source repository")
    entries = manifest.get("files")
    if not isinstance(entries, list) or not entries:
        raise ValueError("empty import inventory")
    seen: set[str] = set()
    for entry in entries:
        relative = entry["path"]
        parts = PurePosixPath(relative)
        if parts.is_absolute() or ".." in parts.parts or "\\" in relative or not parts.parts:
            raise ValueError(f"invalid import path: {relative}")
        if relative in seen:
            raise ValueError(f"duplicate import path: {relative}")
        seen.add(relative)
        path = root / relative
        if any((root.joinpath(*parts.parts[:index])).is_symlink()
               for index in range(1, len(parts.parts) + 1)):
            raise ValueError(f"symlink not allowed in import: {relative}")
        if entry.get("source_path") != relative:
            raise ValueError(f"unexpected relocation: {relative}")
        content = path.read_bytes()
        if len(content) != entry["size_bytes"]:
            raise ValueError(f"size differs: {relative}")
        if hashlib.sha256(content).hexdigest() != entry["sha256"]:
            raise ValueError(f"SHA-256 differs: {relative}")
        blob = b"blob " + str(len(content)).encode("ascii") + b"\0" + content
        if hashlib.sha1(blob).hexdigest() != entry["git_blob_sha1"]:
            raise ValueError(f"Git blob differs: {relative}")
    imported = {str(p.relative_to(root)).replace("\\", "/")
                for p in (root / "src/core_v2").rglob("*") if p.is_file() or p.is_symlink()}
    imported.update(str(p.relative_to(root)).replace("\\", "/")
                    for p in (root / "tests").glob("core_v2_*_tests.cpp"))
    imported.update({"scripts/check_core_v2_architecture.py", "scripts/check_core_v2_architecture.sh"})
    if imported != seen:
        raise ValueError(f"inventory differs: missing={sorted(seen - imported)}, extra={sorted(imported - seen)}")
    print(f"CORE_V2_IMPORT_INTEGRITY=PASS files={len(entries)} source={manifest['source_commit']}")
    return len(entries)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        verify(args.root.resolve())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"CORE_V2_IMPORT_INTEGRITY=FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
