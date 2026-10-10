"""Narrow source checks plus mutation tests; not a compiler/sandbox proof."""
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]


def violations(text, sdk=False, host=False):
    errors = []
    includes = re.findall(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', text, re.M)
    for header in includes:
        if header.startswith(("ecu/core", "linux/", "sys/", "net/", "Qt", "QML")) or header.lower() in {"windows.h", "winsock2.h", "unistd.h", "dlfcn.h", "pthread.h"}:
            errors.append("hardware/CORE/GUI include")
        if sdk and header.startswith("ecu/"):
            errors.append("SDK depends on implementation")
        if host and header in {"filesystem", "fstream", "thread", "mutex", "atomic", "future"}:
            errors.append("host catalog crosses IO/execution boundary")
    # Mask comments and literal contents before looking for execution primitives.
    code = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', ' ', text, flags=re.S)
    if re.search(r'\b(?:system|popen|fork|socket|dlopen|LoadLibrary[AW]?|CreateProcess[AW]?|execve|execvp)\s*\(', code):
        errors.append("external execution or device API")
    return errors


class BoundaryTests(unittest.TestCase):
    def test_actual_sources(self):
        for folder, sdk, host in [("src/app_sdk", True, False), ("src/app_host", False, True), ("apps/host", False, False)]:
            sources = [p for p in (ROOT / folder).rglob("*") if p.suffix in {".cpp", ".hpp", ".h"}]
            self.assertTrue(sources)
            for source in sources:
                with self.subTest(source=source.relative_to(ROOT)):
                    self.assertEqual(violations(source.read_text(), sdk, host), [])

    def test_detects_import_and_execution_regressions(self):
        for source in ['#include "ecu/core_v2/transport/i_can_driver.hpp"',
                       '#include <linux/can.h>', '#include <sys/socket.h>',
                       '#include <QtQuick/QQuickItem>', 'std::system(command);',
                       'socket(1, 2, 3);', 'dlopen(path, flags);', 'CreateProcessW(args);']:
            with self.subTest(source=source):
                self.assertTrue(violations(source))
        self.assertTrue(violations('#include "ecu/app/host/host.hpp"', sdk=True))
        self.assertTrue(violations('#include <filesystem>', host=True))
        self.assertTrue(violations('#include <thread>', host=True))
        self.assertEqual(violations('// socket(...)\nconst char* hint = "system(command)";'), [])

    def test_link_boundaries_are_explicit(self):
        sdk = (ROOT / 'src/app_sdk/CMakeLists.txt').read_text()
        host = (ROOT / 'src/app_host/CMakeLists.txt').read_text()
        cli = (ROOT / 'apps/host/CMakeLists.txt').read_text()
        self.assertNotIn('target_link_libraries', sdk)
        self.assertIn('target_link_libraries(ecu_app_host_lib PUBLIC ECU::app_sdk)', host)
        self.assertIn('target_link_libraries(ecu_app_host PRIVATE ECU::app_host)', cli)
        for text in [sdk, host, cli]:
            self.assertNotRegex(text, r'ECU::core|Qt[56]|SocketCAN|ws2_32|dlopen|FetchContent|find_package')

    def test_host_has_no_package_or_oem_identity(self):
        for source in (ROOT / 'src/app_host').rglob('*.cpp'):
            text = source.read_text()
            self.assertNotRegex(text, r'org\.ecu\.example|DAF|SAC|EGR|VGT|demo\.echo|demo\.fail')


if __name__ == '__main__':
    unittest.main()
