#!/usr/bin/env python3
"""Negative regressions for provenance and Core isolation. No hardware access."""
from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VERIFY = ROOT / "tools/verify_core_import.py"
SCANNER = ROOT / "scripts/check_core_v2_architecture.py"


class GuardRegression(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory(prefix="ecu-app-guards-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        inventory = json.loads((ROOT / "provenance/core_v2_import.json").read_text(encoding="utf-8"))
        for entry in inventory["files"]:
            dest = self.root / entry["path"]
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / entry["path"], dest)
        (self.root / "provenance").mkdir()
        shutil.copyfile(ROOT / "provenance/core_v2_import.json", self.root / "provenance/core_v2_import.json")

    def run_tool(self, tool: Path, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run([sys.executable, str(tool), *args], check=False,
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)

    def test_clean_import(self) -> None:
        result = self.run_tool(VERIFY, "--root", str(self.root))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("files=91", result.stdout)

    def test_modified_core_rejected(self) -> None:
        path = self.root / "src/core_v2/src/can_types.cpp"
        path.write_bytes(path.read_bytes() + b"\n// unexpected change\n")
        self.assertNotEqual(self.run_tool(VERIFY, "--root", str(self.root)).returncode, 0)

    def test_removed_test_rejected(self) -> None:
        (self.root / "tests/core_v2_foundation_tests.cpp").unlink()
        self.assertNotEqual(self.run_tool(VERIFY, "--root", str(self.root)).returncode, 0)

    def test_added_core_file_rejected(self) -> None:
        (self.root / "src/core_v2/new.cpp").write_text("// unreviewed\n", encoding="utf-8")
        self.assertNotEqual(self.run_tool(VERIFY, "--root", str(self.root)).returncode, 0)

    def test_duplicate_manifest_entry_rejected(self) -> None:
        path = self.root / "provenance/core_v2_import.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["files"].append(data["files"][0])
        path.write_text(json.dumps(data), encoding="utf-8")
        self.assertNotEqual(self.run_tool(VERIFY, "--root", str(self.root)).returncode, 0)

    def test_path_escape_rejected(self) -> None:
        path = self.root / "provenance/core_v2_import.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["files"][0]["path"] = "../outside"
        path.write_text(json.dumps(data), encoding="utf-8")
        self.assertNotEqual(self.run_tool(VERIFY, "--root", str(self.root)).returncode, 0)

    def test_blob_mismatch_rejected(self) -> None:
        path = self.root / "provenance/core_v2_import.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["files"][0]["git_blob_sha1"] = "0" * 40
        path.write_text(json.dumps(data), encoding="utf-8")
        self.assertNotEqual(self.run_tool(VERIFY, "--root", str(self.root)).returncode, 0)

    def test_clean_architecture(self) -> None:
        result = self.run_tool(SCANNER, str(self.root / "src/core_v2"))
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_forbidden_dependencies_rejected(self) -> None:
        core = self.root / "src/core_v2"
        probe = core / "boundary_probe.cpp"
        for construct in ["#include <thread>", "#include <mutex>", "#include <vector>",
                          "#include <windows.h>", "#include <linux/can.h>",
                          "std::chrono::steady_clock::now();", 'system("echo forbidden");',
                          '#include "../legacy.hpp"', "#include <QObject>",
                          "auto* x = new int;", "for (;;) {}"]:
            with self.subTest(construct=construct):
                probe.write_text(construct + "\n", encoding="utf-8")
                result = self.run_tool(SCANNER, str(core))
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("forbidden", result.stdout)

    def test_comment_not_a_dependency(self) -> None:
        core = self.root / "src/core_v2"
        (core / "comment_probe.cpp").write_text('// std::thread new QML\nconst char* s = "#include <QObject>";\n', encoding="utf-8")
        result = self.run_tool(SCANNER, str(core))
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_build_dependency_rejected(self) -> None:
        core = self.root / "src/core_v2"
        path = core / "CMakeLists.txt"
        path.write_text(path.read_text(encoding="utf-8") + "\nfind_package(Threads REQUIRED)\n", encoding="utf-8")
        self.assertNotEqual(self.run_tool(SCANNER, str(core)).returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
