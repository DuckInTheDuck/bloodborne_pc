"""Collect a small local dump of our game child if its window stops responding.

The launcher reads the game from a separate process. It never terminates it.
Only one dump per launch is collected; game files and saves are untouched.
"""
import ctypes
from ctypes import wintypes as w
from datetime import datetime
import os
from pathlib import Path
import threading
import time


def descendants(parents, root):
    found = {root}
    while True:
        more = {pid for pid, parent in parents.items() if parent in found}
        if more <= found:
            return found
        found.update(more)


class WindowsApi:
    def __init__(self):
        self.k = ctypes.WinDLL('kernel32', use_last_error=True)
        self.u = ctypes.WinDLL('user32', use_last_error=True)
        self.d = ctypes.WinDLL('dbghelp', use_last_error=True)
        self.callback = ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
        class Entry(ctypes.Structure):
            _fields_ = [('size', w.DWORD), ('usage', w.DWORD), ('pid', w.DWORD),
                        ('heap', ctypes.c_size_t), ('module', w.DWORD),
                        ('threads', w.DWORD), ('parent', w.DWORD),
                        ('priority', w.LONG), ('flags', w.DWORD), ('exe', w.WCHAR*260)]
        self.Entry = Entry
        declarations = [
            (self.k.CreateToolhelp32Snapshot, [w.DWORD, w.DWORD], w.HANDLE),
            (self.k.Process32FirstW, [w.HANDLE, ctypes.POINTER(Entry)], w.BOOL),
            (self.k.Process32NextW, [w.HANDLE, ctypes.POINTER(Entry)], w.BOOL),
            (self.k.CloseHandle, [w.HANDLE], w.BOOL),
            (self.k.OpenProcess, [w.DWORD, w.BOOL, w.DWORD], w.HANDLE),
            (self.k.QueryFullProcessImageNameW, [w.HANDLE, w.DWORD, w.LPWSTR, ctypes.POINTER(w.DWORD)], w.BOOL),
            (self.k.CreateFileW, [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.c_void_p, w.DWORD, w.DWORD, w.HANDLE], w.HANDLE),
            (self.u.EnumWindows, [self.callback, w.LPARAM], w.BOOL),
            (self.u.GetWindowThreadProcessId, [w.HWND, ctypes.POINTER(w.DWORD)], w.DWORD),
            (self.u.IsHungAppWindow, [w.HWND], w.BOOL),
            (self.d.MiniDumpWriteDump, [w.HANDLE, w.DWORD, w.HANDLE, w.DWORD,
                                      ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p], w.BOOL),
        ]
        for fn, args, result in declarations:
            fn.argtypes, fn.restype = args, result

    def hung_game(self, root, executable):
        snapshot = self.k.CreateToolhelp32Snapshot(2, 0)
        if snapshot == ctypes.c_void_p(-1).value:
            return None
        parents = {}
        try:
            entry = self.Entry(); entry.size = ctypes.sizeof(entry)
            more = self.k.Process32FirstW(snapshot, ctypes.byref(entry))
            while more:
                parents[entry.pid] = entry.parent
                more = self.k.Process32NextW(snapshot, ctypes.byref(entry))
        finally:
            self.k.CloseHandle(snapshot)
        allowed = descendants(parents, root)
        candidates = set()
        @self.callback
        def visit(hwnd, _):
            pid = w.DWORD()
            self.u.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value in allowed and self.u.IsHungAppWindow(hwnd):
                candidates.add(pid.value)
            return True
        self.u.EnumWindows(visit, 0)
        for pid in candidates:
            process = self.k.OpenProcess(0x1000, False, pid)
            if not process:
                continue
            try:
                path = ctypes.create_unicode_buffer(32768); size = w.DWORD(len(path))
                if self.k.QueryFullProcessImageNameW(process, 0, path, ctypes.byref(size)):
                    if os.path.normcase(os.path.abspath(path.value)) == os.path.normcase(os.path.abspath(executable)):
                        return pid
            finally:
                self.k.CloseHandle(process)
        return None

    def dump(self, pid, path):
        process = self.k.OpenProcess(0x410, False, pid)  # Query information + read memory
        if not process:
            raise ctypes.WinError(ctypes.get_last_error())
        file = None
        try:
            file = self.k.CreateFileW(str(path), 0x40000000, 0, None, 1, 0x80, None)
            if file == ctypes.c_void_p(-1).value:
                file = None
                raise ctypes.WinError(ctypes.get_last_error())
            # Normal stacks/modules plus thread timing/state; no full-memory dump.
            if not self.d.MiniDumpWriteDump(process, pid, file, 0x1000, None, None, None):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            if file is not None:
                self.k.CloseHandle(file)
            self.k.CloseHandle(process)


def monitor(process, executable, folder, report, api=None, sleep=time.sleep):
    try:
        api = api or WindowsApi()
        previous = None
        streak = 0
        while process.poll() is None:
            sleep(2)
            if process.poll() is not None:
                break
            pid = api.hung_game(process.pid, executable)
            streak = streak+1 if pid is not None and pid == previous else (1 if pid else 0)
            previous = pid
            if streak < 3:
                continue
            folder = Path(folder); folder.mkdir(parents=True, exist_ok=True)
            path = folder / ('hang-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.dmp')
            api.dump(pid, path)
            report(f'Hang diagnostics: saved {path}\n')
            return
    except Exception as error:
        report(f'Hang diagnostics: {error}\n')


def start(process, executable, folder, report):
    if os.name == 'nt':
        threading.Thread(target=monitor, args=(process, executable, folder, report), daemon=True).start()
