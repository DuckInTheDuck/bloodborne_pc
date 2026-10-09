"""Windows launcher integration without starting a game or sending real input."""
import os
from pathlib import Path
import sys
import tempfile
import unittest


@unittest.skipUnless(os.name == 'nt', 'Windows launcher')
class PlayerLauncherTests(unittest.TestCase):
    def test_pages_controls_pacing_and_external_settings(self):
        import tkinter as tk
        from tkinter import ttk, filedialog, messagebox
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
        with tempfile.TemporaryDirectory() as folder:
            old_env = {key: os.environ.get(key) for key in ('BB_DATA_DIR', 'BB_CONFIG')}
            os.environ['BB_DATA_DIR'] = folder
            os.environ['BB_CONFIG'] = str(Path(folder) / 'bbport.ini')
            import bbport_launcher_win as app
            app.CONFIG_DIR = Path(folder) / 'launcher'
            app.CONFIG_FILE = app.CONFIG_DIR / 'settings.json'
            app.Launcher.detect_gpu = lambda self: None
            root = tk.Tk()
            root.withdraw()
            try:
                ui = app.Launcher(root, tk, ttk, filedialog, messagebox)
                root.update_idletasks()
                self.assertEqual(list(ui.nav), ['play','game','graphics','display','controls','advanced','log'])
                self.assertEqual(len(ui.binding_buttons), 50)
                self.assertTrue(ui.var('mouse_enabled','ini').get())
                ui.mouse_sensitivity.set(2.25)
                self.assertAlmostEqual(float(ui.var('mouse_sensitivity_x','ini').get()), 2.25)
                self.assertAlmostEqual(float(ui.var('mouse_sensitivity_y','ini').get()), 2.25)
                ui.var('mouse_separate_axes','app').set(True)
                ui.var('mouse_sensitivity_y','ini').set('1.5')
                self.assertAlmostEqual(float(ui.var('mouse_sensitivity_x','ini').get()), 2.25)
                # An in-game edit after the launcher opened must survive an unrelated launcher edit.
                app.save_ini({'sharpness':'0.75'}, [])
                ui.var('key_options','ini').set('19')
                ui.var('player_fps','app').set('75')
                ui.collect()
                values, _ = app.load_ini()
                self.assertEqual(values['key_options'], '19')
                self.assertEqual(values['sharpness'], '0.75')
                import io
                from types import SimpleNamespace
                log_path = Path(folder) / 'capture.log'
                fake = SimpleNamespace(stdout=io.BytesIO(b'Fault: driver failure\n'), wait=lambda:1)
                ui.read_output(fake, log_path)
                self.assertIn('Fault: driver failure', log_path.read_text(encoding='utf-8'))
                self.assertIn('exited with code 1', log_path.read_text(encoding='utf-8'))
                env = app.game_environment(ui.app)
                self.assertEqual((env['BB_FPS'], env['BB_FPS_LIMIT']), ('uncap','75'))
                ui.app['player_fps'] = '30'
                self.assertEqual(app.game_environment(ui.app)['BB_FPS'], '30')
                ui.app['player_fps'] = 'auto'
                self.assertNotIn('BB_FPS_LIMIT', app.game_environment(ui.app))
                ui.app['player_fps'] = '0'
                self.assertEqual(app.game_environment(ui.app)['BB_FPS_LIMIT'], '0')
            finally:
                root.destroy()
                for key, value in old_env.items():
                    if value is None: os.environ.pop(key, None)
                    else: os.environ[key] = value


if __name__ == '__main__':
    unittest.main()
