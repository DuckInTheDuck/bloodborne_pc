"""Exercise the real windowed EXE -> script -> script pipe chain without a game."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

EXE = os.environ.get('BB_TEST_FROZEN_EXE')


@unittest.skipUnless(os.name == 'nt' and EXE, 'Requires a packaged Windows executable')
class FrozenStreamsTests(unittest.TestCase):
    def run_chain(self, mode):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            child = folder / 'child.py'
            child.write_text("""import argparse,sys
print('Bloodborne\u2122 | \u0440\u0443\u0441\u0441\u043a\u0438\u0439')
print('child stderr',file=sys.stderr)
if sys.argv[1:] == ['fail']:
    argparse.ArgumentParser().exit(1, 'prepare failed: test error\\n')
if sys.argv[1:] == ['close-stderr']:
    sys.stderr.close()
    print('stdout still works')
""", encoding='utf-8')
            parent = folder / 'parent.py'
            parent.write_text(f"""import runpy,sys
from pathlib import Path
helpers=runpy.run_path(str(Path(sys.executable).parent/'run.py'),run_name='helpers')
result=helpers['run_script']({str(child)!r}, *{(['fail'] if mode == 'fail' else ['close-stderr'] if mode == 'close' else [])!r}, capture={mode == 'capture'})
if result is not None: print('captured: '+result.strip())
print('parent complete')
""", encoding='utf-8')
            return subprocess.run([EXE, '--script', str(parent)], stdin=subprocess.DEVNULL,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  creationflags=subprocess.CREATE_NO_WINDOW, timeout=30)

    def test_nested_output_and_utf8_capture(self):
        for mode in ('normal', 'capture', 'close'):
            result = self.run_chain(mode)
            self.assertEqual(result.returncode, 0, result.stdout.decode('utf-8', errors='replace'))
            self.assertIn('Bloodborne\u2122 | \u0440\u0443\u0441\u0441\u043a\u0438\u0439'.encode('utf-8'), result.stdout)
            self.assertIn(b'child stderr', result.stdout)
            self.assertIn(b'parent complete', result.stdout)
            if mode == 'close': self.assertIn(b'stdout still works', result.stdout)

    def test_nested_error_reports_original_error(self):
        result = self.run_chain('fail')
        self.assertEqual(result.returncode, 1)
        self.assertIn(b'prepare failed: test error', result.stdout)
        self.assertNotIn(b'OSError', result.stdout)


if __name__ == '__main__':
    unittest.main()
