import ast
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('portable_profile', ROOT / 'packaging/windows/portable_profile.py')
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


class ProfileTests(unittest.TestCase):
    def test_only_settings_transfer_and_resolution_detects_on_first_run(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'source'; target = Path(folder) / 'package'
            (source / 'user/launcher').mkdir(parents=True); target.mkdir()
            (source / 'bbport.ini').write_text('upscaler=fsr4\noutput_res=2560x1080\nkey_cross=44\nmouse_sensitivity_x=2.223\n')
            (source / 'user/launcher/settings.json').write_text(json.dumps(dict(
                game_dir='F:/private/game', extra_env='BB_PC_PROMPT_DIR=private',
                present_mode='Fifo', window_mode='borderless', frame_stats=True)))
            (source / 'user/save.bin').write_bytes(b'private save')
            profile.seed(source, target)
            self.assertEqual((target/'bbport.ini').read_text(),
                             'upscaler=fsr4\nkey_cross=44\nmouse_sensitivity_x=2.223\n')
            settings = json.loads((target/'user/launcher/settings.json').read_text())
            self.assertEqual(settings['game_dir'], 'game')
            self.assertEqual(settings['extra_env'], '')
            self.assertFalse(settings['frame_stats'])
            self.assertEqual(settings['window_mode'], 'borderless')
            self.assertEqual(settings['present_mode'], 'Fifo')
            self.assertFalse((target/'user/save.bin').exists())

    def test_desktop_selection(self):
        tree = ast.parse((ROOT/'launcher/bbport_launcher_win.py').read_text(encoding='utf-8'))
        fn = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'desktop_output')
        scope = dict(OUTPUTS=[(s, s) for s in ['1280x720', '1920x1080', '2560x1440',
                                             '3840x2160', '2560x1080', '3440x1440']])
        exec(compile(ast.Module(body=[fn], type_ignores=[]), '<desktop>', 'exec'), scope)
        select = scope['desktop_output']
        self.assertEqual(select(1920,1080), '1920x1080')
        self.assertEqual(select(2560,1440), '2560x1440')
        self.assertEqual(select(2560,1080), '2560x1080')
        self.assertEqual(select(1366,768), '1280x720')
        self.assertEqual(select(2560,1600), '2560x1440')


if __name__ == '__main__':
    unittest.main()
