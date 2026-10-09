"""Copy the PE import closure from CLANG64, independent of this PC's DLL search path.

Requires pefile in the packaging venv (installed with PyInstaller).
Windows and API-set DLLs are provided by the player's operating system.
"""
import argparse
from collections import deque
from pathlib import Path
import shutil
import pefile


def imports(path):
    pe = pefile.PE(str(path), fast_load=True)
    try:
        pe.parse_data_directories(directories=[1, 13])
        return [entry.dll.decode('ascii') for entry in
                getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []) +
                getattr(pe, 'DIRECTORY_ENTRY_DELAY_IMPORT', [])]
    finally:
        pe.close()


def collect(destination, source, system):
    available = {p.name.casefold(): p for p in source.glob('*.dll')}
    bundled = {p.name.casefold(): p for p in destination.iterdir() if p.is_file()}
    queue = deque(p for p in bundled.values() if p.suffix.lower() in ('.exe', '.dll'))
    visited, copied = set(), []
    while queue:
        path = queue.popleft()
        if path.name.casefold() in visited:
            continue
        visited.add(path.name.casefold())
        for name in imports(path):
            key = name.casefold()
            if key in bundled:
                queue.append(bundled[key])
            elif key in available:
                target = destination / available[key].name
                shutil.copy2(available[key], target)
                bundled[key] = target
                queue.append(target)
                copied.append(target.name)
            elif key.startswith(('api-ms-', 'ext-ms-')) or (system / name).is_file():
                continue
            else:
                raise ValueError(f'Unresolved dependency {name} imported by {path.name}')
    return copied


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--system', type=Path, default=Path('C:/Windows/System32'))
    args = parser.parse_args()
    print('PE dependencies added:', collect(args.destination, args.source, args.system))
