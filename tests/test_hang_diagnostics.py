"""Diagnostics tests do not start or interact with Bloodborne."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
from bbport_hang_diagnostics import descendants, monitor, WindowsApi


class HangTests(unittest.TestCase):
    def test_descendants_only(self):
        self.assertEqual(descendants({2:1, 3:2, 4:9, 1:8}, 1), {1,2,3})

    def test_transient_hang_does_not_dump_and_sustained_dumps_once(self):
        class Process:
            pid=123
            def poll(self): return None
        class Api:
            states=iter([None, 5, None, 5, 5, 5])
            dumps=[]
            def hung_game(self, root, executable): return next(self.states)
            def dump(self, pid, path): self.dumps.append((pid,path))
        api=Api(); messages=[]
        with tempfile.TemporaryDirectory() as folder:
            monitor(Process(), 'bb-probe.exe', folder, messages.append, api, lambda _:None)
        self.assertEqual(len(api.dumps),1)
        self.assertEqual(api.dumps[0][0],5)
        self.assertIn('saved',messages[0])

    @unittest.skipUnless(os.name == 'nt', 'Windows API')
    def test_small_dump_of_test_child(self):
        child=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'])
        try:
            with tempfile.TemporaryDirectory() as folder:
                path=Path(folder)/'test.dmp'
                WindowsApi().dump(child.pid,path)
                self.assertEqual(path.read_bytes()[:4],b'MDMP')
        finally:
            child.terminate(); child.wait()
