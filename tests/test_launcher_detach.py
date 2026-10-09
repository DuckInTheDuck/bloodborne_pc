"""A detached launcher must keep its output pipe reader alive (no game involved)."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DetachTests(unittest.TestCase):
    def test_late_child_output_survives_parent_window_close(self):
        with tempfile.TemporaryDirectory() as folder:
            parent = Path(folder) / 'parent.py'
            log = Path(folder) / 'run.log'
            parent.write_text(f'''import sys,queue,threading,subprocess
from types import SimpleNamespace
sys.path.insert(0,{str(ROOT/'launcher')!r})
from bbport_launcher_win import Launcher
ui=Launcher.__new__(Launcher)
ui.ui_closed=threading.Event()
ui.output=queue.Queue()
ui.root=SimpleNamespace(destroy=lambda:None)
child=subprocess.Popen([sys.executable,'-u','-c',"import time; time.sleep(0.3); print('late patches output')"],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
reader=threading.Thread(target=ui.read_output,args=(child,{str(log)!r}),daemon=False)
reader.start()
ui.detach_window()
ui.queue_output('discard after close')
assert ui.output.empty()
# Exit the main thread without join: the reader must retain the inherited pipe.
''', encoding='utf-8')
            result = subprocess.run([sys.executable, str(parent)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = log.read_text(encoding='utf-8')
            self.assertIn('late patches output', text)
            self.assertIn('process exited with code 0', text)


if __name__ == '__main__':
    unittest.main()
