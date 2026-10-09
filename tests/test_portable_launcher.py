"""Portable launcher paths survive moving a packaged installation."""
import ast
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PortablePathsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        tree = ast.parse((ROOT / 'launcher/bbport_launcher_win.py').read_text(encoding='utf-8'))
        funcs = [node for node in tree.body if isinstance(node, ast.FunctionDef)
                 and node.name in ('load_json', 'load_app_settings', 'stored_app_settings')]
        self.scope = dict(Path=Path, json=json, PORTABLE=True, PORT_DIR=self.root,
                          CONFIG_FILE=self.root / 'settings.json',
                          APP_DEFAULTS=dict(game_dir=str(self.root / 'game'), user_dir=''))
        exec(compile(ast.Module(body=funcs, type_ignores=[]), '<launcher>', 'exec'), self.scope)

    def test_paths_inside_package_are_stored_relative(self):
        stored = self.scope['stored_app_settings'](dict(game_dir=str(self.root / 'game'),
                                                      user_dir=str(self.root / 'user')))
        self.assertEqual(stored, dict(game_dir='game', user_dir='user'))
        self.scope['CONFIG_FILE'].write_text(json.dumps(stored), encoding='utf-8')
        loaded = self.scope['load_app_settings']()
        self.assertEqual(loaded['game_dir'], str(self.root / 'game'))
        self.assertEqual(loaded['user_dir'], str(self.root / 'user'))

    def test_external_paths_remain_absolute(self):
        external = self.root.parent / 'external-game'
        stored = self.scope['stored_app_settings'](dict(game_dir=str(external), user_dir=''))
        self.assertEqual(stored['game_dir'], str(external))

    def test_stale_path_falls_back_to_local_game(self):
        (self.root / 'game').mkdir()
        (self.root / 'game/eboot.bin').write_bytes(b'test')
        self.scope['CONFIG_FILE'].write_text(json.dumps(dict(game_dir='old-location')), encoding='utf-8')
        self.assertEqual(self.scope['load_app_settings']()['game_dir'], str(self.root / 'game'))

if __name__ == '__main__':
    unittest.main()
