# Bloodborne PC — DuckInTheDuck fork

**English** · [Русский](README.ru.md)

A Windows-focused fork of [Supermedo/bloodborne_pc](https://github.com/Supermedo/bloodborne_pc),
built on [deadinside28's bbport](https://github.com/deadinside28/bloodborne_pc).
The original game's x86-64 code executes natively; a compatibility runtime supplies PS4
system services and a renderer derived from [shadPS4](https://github.com/shadps4-emu/shadPS4)
translates graphics to Vulkan.

**No game files or saves are included.** Supply your own decrypted Bloodborne dump:
**CUSA03173, version 1.09**. This unofficial project is not affiliated with Sony or FromSoftware.

## Features

- Ultrawide output, including 2560×1080, with corrected gameplay camera aspect ratio.
- Native mouse camera input for version 1.09, without right-stick speed saturation.
- Live keyboard and mouse rebinding, including mouse buttons and wheel bindings.
- Overall sensitivity, optional separate X/Y sensitivity, inversion and aspect compensation.
- Arrow-key menu navigation while WASD remains available for gameplay.
- Simplified Windows launcher with graphics, display, game and control settings; advanced options are grouped separately.
- Native-resolution TAA, FSR 3.1/4, graphics effects and frame-rate controls.
- Portable packaging with bundled DLLs, a game-folder placeholder and first-run desktop-resolution detection.
- Persistent settings, launch logging, and fixes for window transitions, frame pacing and preparation.

This branch does **not** include newer upstream DLSS or online-play additions.
In-game prompts still show controller symbols. Original menus use keyboard or gamepad
navigation; pointer selection is not implemented. Some overlays remain 16:9.

## Playing

When a release archive is available, use [this fork's Releases](https://github.com/DuckInTheDuck/bloodborne_pc/releases).

1. Extract the complete portable package.
2. Put your game files in `game/`, or select an existing game directory in the launcher.
   The selected folder must directly contain `eboot.bin`, `dvdroot_ps4/`, `sce_module/` and `sce_sys/`.
3. Start `Bloodborne.exe`, select your resolution, upscaler and FPS limit, then press **Play**.

A packaged build requires no Python, MSYS2 or compiler installation.
Use 64-bit Windows 10/11 and a current Vulkan 1.3-capable graphics driver.
FSR 4 additionally requires compatible Vulkan shader features and its shader assets.
Desktop resolution is detected on first configuration; subsequent explicit choices persist.

**Default controls:** WASD moves, mouse controls the camera, arrows navigate menus,
Space confirms and Left Shift returns. Rebind actions in the launcher's Controls page
or the in-game settings overlay (**Insert**). The game's original menu does not pause gameplay.

Portable settings and saves are stored under `user/`; preparation data lives in `out/`.
For troubleshooting, attach `user/last_run.log` and CPU/GPU/driver information to
[an issue in this fork](https://github.com/DuckInTheDuck/bloodborne_pc/issues).
Back up `user/` before removing the installation.

## Building

```bash
git clone --recursive https://github.com/DuckInTheDuck/bloodborne_pc.git
cd bloodborne_pc
```

Follow [Windows build and packaging instructions](packaging/windows/README.md).
Build with `bash build.sh`; package with `bash packaging/windows/package.sh`.
FSR-Vulkan uses the pinned upstream submodule plus patches in `gpu/patches/fsr-vulkan/`.
Packaging checks native DLL imports recursively to avoid dependencies on the build machine.
Linux sources remain available, but recent validation focuses on Windows.
Archived upstream documentation: [English](docs/upstream-README.md), [Russian](docs/upstream-README.ru.md).

## Status and credits

Experimental software. The portable build has been tested by the maintainer on multiple PCs;
a full playthrough and every hardware configuration have not been verified.
Compatibility and performance may vary by driver, scene and settings.

- Original native runtime and Linux port: **deadinside28**.
- Windows port and original launcher: **Supermedo**.
- This fork's PC controls, ultrawide, launcher and packaging changes: **DuckInTheDuck**, developed with assistance from **OpenAI Codex (GPT-6)**.
- Renderer and shader compiler: **shadPS4 contributors**.
- Upscaling: **AMD FidelityFX / FireBurn FSR-Vulkan**.
- Community patches retain their original credits in `patches/Bloodborne.xml`.

GNU GPL v2 or later; see [LICENSE](LICENSE). Third-party components retain their licenses.
