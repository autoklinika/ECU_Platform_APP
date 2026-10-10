"""Exercise a real process with a package loaded from disk. Never touches CAN."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE=Path(sys.argv.pop(1)).resolve()
ROOT=Path(__file__).resolve().parents[2]

class MockCli(unittest.TestCase):
    def run_proof(self, directory):
        process=subprocess.run([str(EXE),str(directory)],capture_output=True,text=True,timeout=10)
        self.assertEqual(process.returncode,0,process.stderr)
        return [json.loads(line) for line in process.stdout.splitlines()]
    def test_lifecycle_and_faults(self):
        results=self.run_proof(ROOT/'packages/examples/bench_mock')
        terminal=[r for r in results if r['event']!='accepted']
        self.assertEqual([r['event'] for r in terminal],['connected','sample','sample','sample','timeout','bus_off','recovered','reconnected','shutdown'])
        self.assertTrue(all(r['open_count']==1 for r in terminal[:6]))
        self.assertEqual(terminal[-1]['resource_count'],0)
        self.assertEqual(terminal[-1]['status'],'cancelled')
        self.assertGreater(terminal[-2]['session_id'],terminal[0]['session_id'])
        self.assertEqual(terminal[-1]['send_count'],terminal[-2]['send_count'])
    def test_new_module_and_operations_without_rebuild(self):
        text=(ROOT/'packages/examples/bench_mock/manifest.ecu').read_text()
        text=text.replace('org.ecu.example.bench-mock','independent.second.app')
        text=text.replace('operation.open','operation.begin').replace('operation.read','operation.observe')
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory)/'manifest.ecu').write_text(text)
            self.assertEqual(self.run_proof(directory)[-1]['status'],'cancelled')
    def test_bad_package_does_not_run(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory)/'manifest.ecu').write_text('schema=666\n')
            p=subprocess.run([str(EXE),directory],capture_output=True,text=True,timeout=10)
            self.assertNotEqual(p.returncode,0)
            self.assertEqual(p.stdout,'')
if __name__=='__main__': unittest.main()
