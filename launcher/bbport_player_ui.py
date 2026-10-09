# SPDX-License-Identifier: GPL-2.0-or-later
"""Player-facing pages for the Windows launcher; settings remain shared with the port."""
import ctypes

FPS_CHOICES = [('auto', ('Auto (monitor refresh, max 120)', 'Авто (частота монитора, до 120)')),
               *[(str(n), (f'{n} FPS',)) for n in (30, 60, 75, 90, 120)],
               ('0', ('Unlimited', 'Без ограничения'))]
WINDOW_CHOICES = [('windowed', ('Windowed', 'Окно')),
                  ('borderless', ('Borderless fullscreen', 'Полный экран без рамки'))]
MOUSE_CHOICES = [(str(n), (label,)) for n, label in enumerate(
    ('—', 'R1 / Attack', 'R2 / Strong attack', 'L1 / Transform weapon', 'L2 / Left hand',
     'Cross / Confirm', 'Circle / Back', 'Square / Item', 'Triangle / Heal',
     'R3 / Lock on', 'L3'))]


def install(cls, context):
    globals().update({k: v for k, v in context.items() if k not in ('__name__', '__file__')})
    original_advanced = cls.build_advanced
    original_collect = cls.collect
    original_status = cls.refresh_status

    def fps_row(self, parent):
        self.row(parent, _('Frame rate', 'Частота кадров'),
                 self.choice(parent, 'player_fps', 'app', FPS_CHOICES, 30))

    def build_graphics(self):
        f = self.scrolled_page('graphics', _('Graphics', 'Графика'),
                               _('Image quality and game effects.', 'Качество изображения и эффекты игры.'))
        for key, title, choices in [('upscaler', _('Anti-aliasing / upscaling', 'Сглаживание / масштабирование'), UPSCALERS),
                                    ('preset', _('Quality', 'Качество'), PRESETS),
                                    ('model_lod', _('Model detail', 'Детализация моделей'), LODS)]:
            self.row(f, title, self.choice(f, key, 'ini', choices))
        self.check(f, 'sharpen', 'ini', _('Sharpening', 'Повышение резкости'))
        self.row(f, _('Sharpness', 'Резкость'), self.ttk.Scale(f, from_=0, to=2,
                 variable=self.var('sharpness', 'ini'), length=300))
        self.check(f, 'show_fps', 'ini', _('FPS counter', 'Счётчик FPS'))
        self.section(f, _('Effects', 'Эффекты'))
        self.version_warning(f)
        for key, title, _on in EFFECTS:
            self.check(f, key, 'ini', _(*title))

    def build_display(self):
        f = self.scrolled_page('display', _('Display', 'Экран'),
                               _('Applied at launch.', 'Применяется при запуске.'))
        self.row(f, _('Resolution', 'Разрешение'), self.choice(f, 'output_res', 'ini', OUTPUTS))
        self.fps_row(f)
        self.row(f, _('Window mode', 'Режим окна'), self.choice(f, 'window_mode', 'app', WINDOW_CHOICES))
        self.check(f, 'hdr', 'app', 'HDR', _('Requires HDR enabled in Windows.', 'Нужен включённый HDR в Windows.'))
        self.note(f, _('The automatic cap uses monitor refresh, up to 120 FPS. Higher rates can affect game timing.',
                       'Автоматический лимит учитывает частоту монитора, до 120 FPS. Более высокий FPS может нарушать тайминги игры.'))

    def build_game(self):
        f = self.scrolled_page('game', _('Game', 'Игра'), _('Files, saves and language.', 'Файлы, сохранения и язык.'))
        self.folder(f, 'game_dir', _('Game folder', 'Папка игры'),
                    _('Choose the folder containing eboot.bin', 'Выберите папку с eboot.bin'),
                    on_change=self.game_changed)
        self.folder(f, 'user_dir', _('Saves folder', 'Папка сохранений'),
                    _('Choose saves folder', 'Выберите папку сохранений'),
                    _('Empty: {}', 'Пусто: {}').format(DATA_DIR / 'user'), on_change=self.refresh_status)
        self.row(f, _('Language', 'Язык'), self.choice(f, 'language', 'app', LANGUAGES))
        self.check(f, 'skip_intro', 'ini', _('Skip intro', 'Пропуск заставок'))

    def build_controls(self):
        f = self.scrolled_page('controls', _('Controls', 'Управление'),
                               _('Shared with the in-game controls menu.', 'Общие настройки с меню управления в игре.'))
        self.check(f, 'mouse_enabled', 'ini', _('Mouse camera', 'Управление камерой мышью'))
        sx = self.var('mouse_sensitivity_x', 'ini')
        sy = self.var('mouse_sensitivity_y', 'ini')
        separate = self.var('mouse_separate_axes', 'app')
        if abs(float(sx.get()) - float(sy.get())) > 0.001:
            separate.set(True)
        self.mouse_sensitivity = self.tk.DoubleVar(value=float(sx.get()))
        self.sensitivity_sync = False

        def slider(var):
            holder = self.ttk.Frame(f)
            self.ttk.Scale(holder, from_=0.1, to=5, length=270, variable=var).pack(side='left')
            value = self.ttk.Label(holder, width=6, foreground=TEXT, background=PANEL)
            value.pack(side='left', padx=12)
            def refresh(*_a):
                value.configure(text=f'{float(var.get()):.2f}')
            var.trace_add('write', refresh)
            refresh()
            return holder

        def common_changed(*_a):
            if self.sensitivity_sync or separate.get(): return
            self.sensitivity_sync = True
            sx.set(f'{self.mouse_sensitivity.get():.3f}')
            sy.set(f'{self.mouse_sensitivity.get():.3f}')
            self.sensitivity_sync = False

        self.mouse_sensitivity.trace_add('write', common_changed)
        common_row = self.next_row(f)
        self.row(f, _('Sensitivity', 'Чувствительность'), slider(self.mouse_sensitivity))
        self.check(f, 'mouse_separate_axes', 'app', _('Separate X/Y sensitivity', 'Настраивать X/Y раздельно'))
        axis_rows = []
        for axis in ('x', 'y'):
            axis_rows.append(self.next_row(f))
            self.row(f, _('Sensitivity ', 'Чувствительность ') + axis.upper(),
                     slider(self.var('mouse_sensitivity_'+axis, 'ini')))
        common_widgets = f.grid_slaves(row=common_row)
        axis_widgets = [w for r in axis_rows for w in f.grid_slaves(row=r)]

        def mode_changed(*_a):
            for w in common_widgets:
                w.grid_remove() if separate.get() else w.grid()
            for w in axis_widgets:
                w.grid() if separate.get() else w.grid_remove()
            if not separate.get():
                self.mouse_sensitivity.set(float(sx.get()))
        separate.trace_add('write', mode_changed)
        mode_changed()
        self.check(f, 'mouse_invert_y', 'ini', _('Invert Y', 'Инвертировать Y'))
        self.check(f, 'mouse_aspect_compensation', 'ini', _('Aspect compensation', 'Учёт пропорций экрана'))
        for key, title in [('mouse_left_action', _('Left button', 'Левая кнопка')),
                           ('mouse_right_action', _('Right button', 'Правая кнопка')),
                           ('mouse_middle_action', _('Middle button', 'Средняя кнопка')),
                           ('mouse_x1_action', 'X1'), ('mouse_x2_action', 'X2')]:
            self.row(f, title, self.choice(f, key, 'ini', MOUSE_CHOICES))
        self.section(f, _('Keyboard', 'Клавиатура'))
        self.note(f, _('Click a binding, then press a key. Insert opens the port menu and is reserved.',
                       'Нажмите назначение, затем клавишу. Insert зарезервирован для меню порта.'))
        from bbport_control_data import BINDINGS, KEY_NAMES, VK_CODES
        self.binding_buttons = {}
        for action, label, primary, secondary in BINDINGS:
            if action.startswith('menu_'):
                continue  # Native focus detection is disabled after gameplay regression.
            holder = self.ttk.Frame(f)
            for slot in ('', '_secondary'):
                key = 'key_'+action+slot
                var = self.var(key, 'ini')
                button = self.ttk.Button(holder, width=16)
                def refresh(*_a, button=button, var=var):
                    button.configure(text=KEY_NAMES.get(int(var.get() or 0), str(var.get())))
                var.trace_add('write', refresh)
                refresh()
                self.binding_buttons[key] = button
                def capture(key=key, button=button):
                    button.configure(text=_('Press a key…', 'Нажмите клавишу…'))
                    self.capture_binding = key
                    button.focus_set()
                button.configure(command=capture)
                button.pack(side='left', padx=(0, 5))
                self.ttk.Button(holder, text='×', width=3,
                                command=lambda var=var: var.set('0')).pack(side='left', padx=(0, 10))
            self.row(f, label, holder)
        def keypress(event):
            key = getattr(self, 'capture_binding', None)
            if not key: return
            code = VK_CODES.get(event.keycode)
            if event.keysym in ('Shift_R', 'Control_R', 'Alt_R'): code = {'Shift_R':229,'Control_R':228,'Alt_R':230}[event.keysym]
            if code is None or code == 73: return 'break'
            self.vars[key].set(str(code))
            self.capture_binding = None
            return 'break'
        self.root.bind('<KeyPress>', keypress, add='+')
        def reset():
            for action, _label, primary, secondary in BINDINGS:
                self.var('key_'+action, 'ini').set(str(primary))
                self.var('key_'+action+'_secondary', 'ini').set(str(secondary))
        self.ttk.Button(f, text=_('Reset keyboard bindings', 'Сбросить клавиши'), command=reset).grid(
            row=self.next_row(f), column=1, sticky='w', pady=12)

    def build_advanced(self):
        original_advanced(self)
        f = self.pages['advanced'].inner
        self.section(f, _('Rendering', 'Рендер'))
        self.row(f, _('Live resolution changes', 'Смена разрешения без перезапуска'), self.choice(f, 'live_resolution', 'ini', LIVE))
        self.check(f, 'object_motion', 'ini', _('Reduce trails on moving objects', 'Уменьшать шлейфы движущихся объектов'),
                   _('Improves temporal reconstruction; GPU cost depends on the scene.',
                     'Улучшает временное сглаживание; нагрузка на GPU зависит от сцены.'))
        self.row(f, _('Presentation mode', 'Режим показа кадров'), self.choice(f, 'present_mode', 'app', PRESENT_MODES))
        self.row(f, _('Frames queued ahead', 'Кадров впереди GPU'), self.choice(f, 'frames_ahead', 'app', FRAMES_AHEAD),
                 _('More buffering may improve throughput but increases input latency.',
                   'Большая очередь может повысить производительность, но увеличивает задержку управления.'))
        self.row(f, _('Player name', 'Имя игрока'), self.ttk.Entry(f, textvariable=self.var('player_name', 'app')))
        for key, title, _on in EXTRAS:
            if key != 'skip_intro': self.check(f, key, 'ini', _(*title))
        self.section(f, 'FSR 4')
        self.fsr4_label = self.ttk.Label(f, wraplength=self.px(600))
        self.fsr4_label.grid(row=self.next_row(f), column=0, columnspan=2, sticky='w')
        self.fsr4_button = self.ttk.Button(f, text=_('Restore missing assets', 'Восстановить недостающие файлы'), command=self.download_fsr4)
        self.fsr4_button.grid(row=self.next_row(f), column=1, sticky='w')
        self.fsr4_progress = self.ttk.Progressbar(f, maximum=len(fsr4_files()), length=280)
        self.fsr4_progress.grid(row=self.next_row(f), column=1, sticky='w')
        self.refresh_fsr4()
        self.ttk.Button(f, text=_('Cheats and tweaks', 'Читы и модификаторы'), command=lambda:self.show('cheats')).grid(row=self.next_row(f),column=1,sticky='w')
        self.ttk.Button(f, text=_('Mods and patches', 'Моды и патчи'), command=lambda:self.show('mods')).grid(row=self.next_row(f),column=1,sticky='w')

    def collect(self):
        live, lines = load_ini()
        original_collect(self)
        # Preserve changes made in the in-game menu while the launcher stayed open.
        for key, value in self.ini.items():
            if value != self.loaded_ini.get(key):
                live[key] = value
        save_ini(live, lines)
        self.ini, self.ini_lines = load_ini()
        self.loaded_ini = dict(self.ini)
        for key, var in self.vars.items():
            if var.store == 'ini':
                value = self.ini[key]
                var.set(value == '1' if key in INI_FLAGS else value)

    def refresh_status(self):
        original_status(self)
        self.checks['gpu'].pack_forget()
        self.checks['fsr4'].pack_forget()
        user = Path(self.var('user_dir', 'app').get() or DATA_DIR / 'user')
        existing = user
        while not existing.exists() and existing != existing.parent:
            existing = existing.parent
        if not os.access(existing, os.W_OK):
            self.checks['saves'].configure(text=_('Saves folder is not writable: {}',
                'Нет доступа для записи сохранений: {}').format(user), foreground='#d36b5c')
            if not self.process: self.play_button.configure(state='disabled')

    def summary(self):
        fps = dict(FPS_CHOICES).get(self.var('player_fps', 'app').get(), ('?',))
        return f'{_(*fps)} · {self.var("upscaler", "ini").get().upper()} · {self.var("output_res", "ini").get()}'

    for fn in (fps_row, build_graphics, build_display, build_game, build_controls, build_advanced, collect, refresh_status, summary):
        setattr(cls, fn.__name__, fn)
