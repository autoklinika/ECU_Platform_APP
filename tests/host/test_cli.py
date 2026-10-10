"""Black-box tests of the real C++ host. No sockets, DUTs, GUI or privileged actions."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest

HOST = str(Path(sys.argv[1]).resolve())
del sys.argv[1]
ROOT = Path(__file__).resolve().parents[2]


def package_text(module="example.app", service="demo.echo", version="1.0.0"):
    return (f"schema=1\nid={module}\nversion={version}\napi=1.0\n"
            f"requires={service}\noperation.execute={service}\n")


def q(path):
    return json.dumps(str(path).replace("\\", "/"))


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ecu app proof ")
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def package(self, folder="one", text=None):
        directory = self.root / folder
        directory.mkdir(exist_ok=True)
        (directory / "manifest.ecu").write_bytes((text or package_text()).encode("ascii"))
        return directory

    def run_script(self, commands):
        result = subprocess.run([HOST], input="\n".join(commands) + "\n", text=True,
                                capture_output=True, timeout=20, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        return [json.loads(line) for line in result.stdout.splitlines()]

    def test_lifecycle_and_denial_before_grant(self):
        p = self.package()
        out = self.run_script([f"install {q(p)}", "list", "activate example.app",
            'run example.app 1 execute "hello"', "allow example.app execute",
            'run example.app 1 execute "hello"', "deactivate example.app 1",
            'run example.app 1 execute "late"', "remove example.app", "list", "quit"])
        self.assertEqual(out[1]["modules"][0]["state"], "disabled")
        self.assertEqual(out[3]["status"], "permission_denied")
        self.assertEqual(out[3]["operation_id"], 0)
        self.assertEqual(out[5]["payload"], "hello")
        self.assertEqual(out[5]["operation_id"], 1)
        self.assertEqual(out[7]["status"], "not_active")
        self.assertEqual(out[9]["modules"], [])

    def test_new_arbitrary_package_id_and_operation_without_rebuild(self):
        p = self.package(text=package_text("another.lab.app").replace("operation.execute", "operation.new.name"))
        out = self.run_script([f"install {q(p)}", "allow another.lab.app new.name",
            "activate another.lab.app", 'run another.lab.app 1 new.name "added without rebuilding"'])
        self.assertEqual(out[-1]["payload"], "added without rebuilding")

    def test_independent_failure_and_survivor_in_same_process(self):
        echo = self.package("echo", package_text("good"))
        fail = self.package("fail", package_text("bad", "demo.fail"))
        out = self.run_script([f"install {q(echo)}", f"install {q(fail)}",
            "allow good execute", "allow bad execute", "activate good", "activate bad",
            'run bad 2 execute ""', 'run good 1 execute "still healthy"', "list"])
        self.assertEqual(out[-3]["status"], "operation_failed")
        self.assertEqual(out[-2]["payload"], "still healthy")
        modules = {m["id"]: m for m in out[-1]["modules"]}
        self.assertEqual(modules["bad"]["last_operation"]["status"], "operation_failed")
        self.assertEqual(modules["good"]["last_operation"]["status"], "ok")

    def test_rejections_leave_catalog_unchanged(self):
        good = self.package()
        missing = self.package("missing", package_text("missing", "hardware.can"))
        api = self.package("api", package_text("new.api").replace("api=1.0", "api=2.0"))
        invalid = self.package("invalid", package_text("invalid") + "grants=all\n")
        out = self.run_script([f"install {q(good)}", f"install {q(good)}",
            f"install {q(missing)}", f"install {q(api)}", f"install {q(invalid)}", "list"])
        self.assertEqual([x["status"] for x in out[:-1]],
            ["ok", "duplicate_id", "missing_capability", "incompatible_api", "invalid_manifest"])
        self.assertEqual(len(out[-1]["modules"]), 1)

    def test_update_blocks_active_cas_and_clears_permissions(self):
        v1 = self.package()
        v2 = self.package("v2", package_text(version="1.1.0"))
        out = self.run_script([f"install {q(v1)}", "allow example.app execute", "activate example.app",
            f"replace {q(v2)} 1.0.0", "remove example.app", "deactivate example.app 1",
            f"replace {q(v2)} 9.0.0", f"replace {q(v2)} 1.0.0", "activate example.app",
            'run example.app 1 execute "old"', 'run example.app 2 execute "no grant"', "list"])
        self.assertEqual(out[3]["status"], "active_module")
        self.assertEqual(out[4]["status"], "active_module")
        self.assertEqual(out[6]["status"], "version_conflict")
        self.assertEqual(out[7]["status"], "ok")
        self.assertEqual(out[9]["status"], "stale_activation")
        self.assertEqual(out[10]["status"], "permission_denied")
        self.assertEqual(out[11]["modules"][0]["version"], "1.1.0")

    def test_stale_stop_cannot_disable_new_activation(self):
        p = self.package()
        out = self.run_script([f"install {q(p)}", "activate example.app", "deactivate example.app 1",
                              "activate example.app", "deactivate example.app 1", "list"])
        self.assertEqual(out[-2]["status"], "stale_activation")
        self.assertEqual(out[-1]["modules"][0]["state"], "active")

    def test_unknown_command_and_malformed_arguments_recover(self):
        out = self.run_script(["shell rm anything", "list extra", 'install "unterminated',
                              "activate", "deactivate example.app -1", "deactivate example.app 18446744073709551616", "list"])
        self.assertTrue(all(x["status"] == "invalid_command" for x in out[:-1]))
        self.assertEqual(out[-1]["modules"], [])

    def test_bounded_commands_and_manifest(self):
        p = self.package(text=package_text() + "#" + "x" * 9000)
        out = self.run_script(["x" * 10000, "list", f"install {q(p)}", "list"])
        self.assertEqual(out[0]["status"], "invalid_command")
        self.assertEqual(out[1]["modules"], [])
        self.assertEqual(out[2]["status"], "invalid_manifest")
        self.assertEqual(out[3]["modules"], [])

    def test_json_quoting_is_valid(self):
        p = self.package()
        text = 'a "quoted" and \\ slashed value'
        out = self.run_script([f"install {q(p)}", "allow example.app execute", "activate example.app",
                              "run example.app 1 execute " + json.dumps(text)])
        self.assertEqual(out[-1]["payload"], text)

    def test_package_content_never_executes(self):
        p = self.package()
        marker = self.root / "MUST_NOT_EXIST"
        (p / "install.py").write_text(f"from pathlib import Path\nPath({str(marker)!r}).touch()\n")
        out = self.run_script([f"install {q(p)}", "activate example.app", "list"])
        self.assertFalse(marker.exists())
        self.assertEqual(out[-1]["modules"][0]["completed_operations"], 0)

    def test_missing_and_symlink_manifest(self):
        missing = self.root / "missing"
        out = self.run_script([f"install {q(missing)}", "list"])
        self.assertEqual(out[0]["status"], "io_error")
        self.assertEqual(out[1]["modules"], [])
        p = self.package()
        link = self.root / "linked"
        link.mkdir()
        try:
            (link / "manifest.ecu").symlink_to(p / "manifest.ecu")
        except (OSError, NotImplementedError):
            return  # Symlink creation can require extra Windows privileges.
        out = self.run_script([f"install {q(link)}", "list"])
        self.assertEqual(out[0]["status"], "io_error")
        self.assertEqual(out[1]["modules"], [])

    def test_sample_packages_and_help(self):
        out = self.run_script([f'install {q(ROOT / "packages/examples/echo")}',
                              f'install {q(ROOT / "packages/examples/failure")}', "list"])
        self.assertEqual(len(out[-1]["modules"]), 2)
        help_result = subprocess.run([HOST, "--help"], capture_output=True, text=True, timeout=10)
        self.assertEqual(help_result.returncode, 0)
        self.assertIn("No GUI", help_result.stdout)
        bad = subprocess.run([HOST, "--not-supported"], capture_output=True, timeout=10)
        self.assertEqual(bad.returncode, 2)


if __name__ == "__main__":
    unittest.main()
