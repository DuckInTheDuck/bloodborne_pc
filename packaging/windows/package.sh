#!/usr/bin/env bash
# Builds dist/bbport-windows/ (and dist/bbport-windows.zip): Bloodborne.exe (the launcher, frozen
# with PyInstaller so players need no Python), bb-probe.exe with the MSYS2 CLANG64 DLLs it needs,
# the preparation scripts and run.py. Run from an MSYS2 CLANG64 shell after `bash build.sh`.
# Freezing uses a Windows Python 3.10+ (python.org; WINPYTHON overrides) and a private venv in
# out/pyenv with PyInstaller. FSR 4 assets in fsr4_shaders/ are included when present.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
source ./msys2-env.sh
[[ -f out/bb-probe.exe && -f out/bb-gpu-capabilities.exe && -f out/bb-play.exe ]] || { echo 'Build first: bash build.sh' >&2; exit 1; }

# A Windows Python (not MSYS2's) for PyInstaller.
python=${WINPYTHON:-}
if [[ -z $python ]]; then
    for candidate in /c/Python3*/python.exe "${LOCALAPPDATA:-/c/Users/$USER/AppData/Local}"/Programs/Python/Python3*/python.exe; do
        [[ -x $candidate ]] && python=$candidate
    done
fi
[[ -n $python ]] || { echo 'Need a Windows Python 3 (python.org) or WINPYTHON=path\to\python.exe' >&2; exit 1; }
# Windows Python needs USERPROFILE (some MSYS2 shells start without it).
export USERPROFILE=${USERPROFILE:-$(cygpath -w "/c/Users/$(id -un)")}
if [[ ! -x out/pyenv/Scripts/python.exe ]]; then
    "$python" -m venv out/pyenv
fi
out/pyenv/Scripts/python.exe -m pip install -q --disable-pip-version-check pyinstaller
# The scripts run inside Bloodborne.exe (--script): the standard modules they import come along.
hidden=()
for module in argparse base64 collections hashlib json re shutil struct tempfile xml.etree.ElementTree \
              urllib.request ctypes.wintypes; do
    hidden+=(--hidden-import "$module")
done
out/pyenv/Scripts/python.exe -m PyInstaller --noconfirm --clean --log-level WARN --windowed \
    --name Bloodborne --icon "$(cygpath -w "$PWD/launcher/bloodborne.ico")" --distpath out/pyi-dist \
    --workpath out/pyi-work --specpath out/pyi-work --paths "$(cygpath -w "$PWD/scripts")" "${hidden[@]}" \
    "$(cygpath -w "$PWD/launcher/bbport_launcher_win.py")"

# The package is assembled in a fresh staging folder and zipped from there; dist/bbport-windows
# (a playable copy that may hold saves and settings) is only refreshed afterwards.
dest=out/stage/bbport-windows
# Validate the staging target before removing it; never touch a playable installation.
stage_absolute=$(cygpath -am "$PWD/out/stage")
[[ "$stage_absolute" == "$(cygpath -am "$PWD")/out/stage" ]] || exit 1
powershell -NoProfile -Command "if (Test-Path -LiteralPath '$stage_absolute') { Remove-Item -LiteralPath '$stage_absolute' -Recurse -Force }"
mkdir -p "$dest/bin" "$dest/launcher" "$dest/game"
printf 'Custom local build: do not replace with upstream auto-updates.\n' > "$dest/local-build.txt"
printf 'Portable installation: settings and saves live in user/.\n' > "$dest/portable.txt"
printf '@echo off\r\ncd /d "%%~dp0"\r\nstart "" "%%~dp0Bloodborne.exe" --launcher\r\n' > "$dest/Settings.cmd"
printf 'Place your own decrypted game files here: eboot.bin, sce_sys/, dvdroot_ps4/.\nOr select an existing game folder in Settings.cmd.\n' > "$dest/game/README.txt"
cp -r out/pyi-dist/Bloodborne/. "$dest/"
llvm-strip -o "$dest/Play Bloodborne.exe" out/bb-play.exe
cp launcher/bloodborne.ico launcher/bloodborne.png "$dest/launcher/"
# The executables without debug information (out/ keeps the symbols for crash reports).
for exe in bb-probe.exe bb-gpu-capabilities.exe; do
    llvm-strip --strip-debug -o "$dest/bin/$exe" "out/$exe"
done
# Every DLL the executables load from the CLANG64 tree (SDL3, FFmpeg, Vulkan loader, ...).
ldd "$dest/bin/bb-probe.exe" "$dest/bin/bb-gpu-capabilities.exe" |
    awk '/\/clang64\/bin\// {print $3}' | sort -u | while read -r dll; do
        cp -u "$dll" "$dest/bin/"
    done
# Resolve imports recursively, including DLLs ldd found outside CLANG64.
out/pyenv/Scripts/python.exe packaging/windows/native_dependencies.py \
    --destination "$(cygpath -am "$dest/bin")" --source "$(cygpath -am "$msys2_root/clang64/bin")"
cp -r scripts patches "$dest/"
cp run.py LICENSE README.md packaging/windows/README-Windows.txt "$dest/"
cp packaging/windows/START-RU.txt "$dest/НАЧАТЬ.txt"
cp packaging/windows/START-RU.txt "$dest/game/КУДА-ПОЛОЖИТЬ-ИГРУ.txt"
assets=fsr4_shaders
if [[ ! -d "$assets" && -d dist/bbport-windows/fsr4_shaders ]]; then assets=dist/bbport-windows/fsr4_shaders; fi
if [[ -d "$assets" ]]; then
    [[ -f "$assets/LICENSE-FSR4-v07.txt" ]] || { echo 'FSR4 assets need their license notice' >&2; exit 1; }
    cp -r "$assets" "$dest/fsr4_shaders"
fi
if [[ -n ${BB_PACKAGE_PROFILE_DIR:-} ]]; then
    out/pyenv/Scripts/python.exe packaging/windows/portable_profile.py \
        --source "$(cygpath -am "$BB_PACKAGE_PROFILE_DIR")" --destination "$(cygpath -am "$dest")"
fi
# Development experiments are not part of the player distribution.
rm -f "$dest/scripts/pc_prompt_trial.py" "$dest/scripts/pc_prompt_gpu_trial.py" "$dest/scripts/inspect_pc_prompts.py"
find "$dest" -name __pycache__ -prune -exec rm -r {} +
mkdir -p dist
rm -f dist/bbport-windows.zip
(cd out/stage && powershell -NoProfile -Command \
    "Compress-Archive -Path bbport-windows -DestinationPath ../../dist/bbport-windows.zip")

# Refresh dist/bbport-windows, keeping what players create there (saves, settings, mods), and
# only while nothing runs from it: deleting a running launcher's files breaks it.
play=dist/bbport-windows
running=$(powershell -NoProfile -Command \
    "@(Get-Process | Where-Object { \$_.Path -like '$(cygpath -w "$PWD/$play")\\*' }).Count" | tr -d '\r')
if [[ -n ${BB_PACKAGE_PROFILE_DIR:-} ]]; then
    echo 'Portable profile package ready; local playable installation preserved.'
elif [[ ${running:-0} != 0 ]]; then
    echo "dist/bbport-windows is in use ($running processes): not refreshed; the zip is ready." >&2
else
    mkdir -p "$play"
    # Overlay program files only: preserve game files, preparation output and user data.
    cp -r "$dest/." "$play/"
fi
du -sh "$dest" dist/bbport-windows.zip
