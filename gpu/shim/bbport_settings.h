// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: user settings changed at run time from the in-game menu (bbport_overlay.h) and kept
// in bbport.ini (BB_CONFIG overrides the path). Environment variables override the file at
// start. Readers load the atomics every frame; writers are the menu and Load().

#pragma once

#include <atomic>
#include "../bbport_input.h"
#include "../bbport_bindings.h"
#include <SDL3/SDL_scancode.h>

namespace BbSettings {

enum Upscaler : int { UpscalerOff = 0, UpscalerFsr3 = 1, UpscalerFsr4 = 2, UpscalerFsr411 = 3,
                      UpscalerTaa = 4, UpscalerCount };
/// FSR 4 v07 or FSR 4.1.1: the same inputs, settings and placement in the frame.
inline bool IsFsr4(int upscaler) {
    return upscaler == UpscalerFsr4 || upscaler == UpscalerFsr411;
}
enum Preset : int { NativeAA = 0, Quality, Balanced, Performance, UltraPerformance, PresetCount };
enum DebugView : int { DebugNone = 0, DebugReactive = 1, DebugMotion = 2, DebugViewCount };

/// Game effects switched by the community patches at start (patches.py EFFECTS): ini key,
/// menu label, default (the game's own behaviour).
struct Effect {
    const char* key;
    const char* label;
    bool default_on;
};
inline constexpr Effect Effects[] = {
    {"effect_chromatic_aberration", "Хроматическая аберрация", true},
    {"effect_dof", "Глубина резкости (DoF)", true},
    {"effect_motion_blur", "Размытие в движении", true},
    {"effect_ssao", "Затенение SSAO", true},
    {"effect_game_aa", "Собственное сглаживание игры", true},
    {"effect_dynamic_shadows", "Тени от динамических источников", true},
    {"effect_ssr", "Отражения SSR (не было в игре)", false},
    {"skip_intro", "Пропуск заставок при запуске", false},
    {"debug_camera", "Свободная камера (Cross + L3)", false},
    {"debug_menu", "Debug menu (нужны файлы шрифтов)", false},
    {"cheat_no_death", "Чит: бессмертие (не ниже 1 HP)", false},
    {"cheat_stealth", "Чит: враги не замечают", false},
    {"cheat_silent", "Чит: враги не слышат", false},
    {"cheat_rally_no_decay", "Чит: Rally не угасает", false},
    {"cheat_enemy_control", "Чит: управление врагом (R3 / L3)", false},
    {"tweak_no_rally", "Без Rally (возврата HP)", false},
    {"tweak_camera_distance", "Камера дальше", false},
    {"tweak_no_camera_rotation", "Без автоповорота камеры", false},
    {"tweak_easy_run", "Бег с меньшим наклоном стика", false},
    {"tweak_ragdoll", "Физика тел как в Dark Souls", false},
};
inline constexpr int EffectCount = int(sizeof(Effects) / sizeof(Effects[0]));
/// Live output resolutions: the upscaler's output and the UI host targets.
inline constexpr int OutputWidths[] = {1280, 1920, 2560, 3840, 2560, 3440, 3840, 5120, 3840, 5120};
inline constexpr int OutputHeights[] = {720, 1080, 1440, 2160, 1080, 1440, 1600, 2160, 1080, 1440};
inline constexpr int OutputCount = sizeof(OutputWidths) / sizeof(OutputWidths[0]);
inline constexpr int OutputDefault = 1; ///< 1920x1080, the game's own size

struct KeyBinding {
    const char* key;
    const char* label;
    int primary;
    int secondary = 0;
};
inline constexpr KeyBinding KeyBindings[BB_KEY_COUNT] = {
    {"move_up", "Движение вперёд", SDL_SCANCODE_W},
    {"move_down", "Движение назад", SDL_SCANCODE_S},
    {"move_left", "Движение влево", SDL_SCANCODE_A},
    {"move_right", "Движение вправо", SDL_SCANCODE_D},
    {"camera_up", "Камера вверх", 0},
    {"camera_down", "Камера вниз", 0},
    {"camera_left", "Камера влево", 0},
    {"camera_right", "Камера вправо", 0},
    {"cross", "Действие / подтвердить (Cross)", SDL_SCANCODE_SPACE},
    {"circle", "Уклонение / бег / назад (Circle)", SDL_SCANCODE_LSHIFT},
    {"square", "Использовать предмет (Square)", SDL_SCANCODE_E},
    {"triangle", "Лечение (Triangle)", SDL_SCANCODE_Q},
    {"l1", "Трансформация оружия (L1)", SDL_SCANCODE_1},
    {"r1", "Атака (R1)", SDL_SCANCODE_3},
    {"l2", "Левая рука / огнестрельное (L2)", SDL_SCANCODE_R},
    {"r2", "Сильная атака (R2)", SDL_SCANCODE_F},
    {"l3", "Нажатие левого стика (L3)", SDL_SCANCODE_Z},
    {"r3", "Захват цели / сброс камеры (R3)", SDL_SCANCODE_C},
    {"options", "Меню / инвентарь (Options)", SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE},
    {"touch_left", "Левая часть touchpad", SDL_SCANCODE_TAB},
    {"touch_right", "Правая часть touchpad", SDL_SCANCODE_BACKSPACE},
    {"up", "Крестовина вверх", SDL_SCANCODE_UP},
    {"down", "Крестовина вниз", SDL_SCANCODE_DOWN},
    {"left", "Крестовина влево", SDL_SCANCODE_LEFT},
    {"right", "Крестовина вправо", SDL_SCANCODE_RIGHT},
    {"menu_up", "Меню: вверх", SDL_SCANCODE_W, SDL_SCANCODE_UP},
    {"menu_down", "Меню: вниз", SDL_SCANCODE_S, SDL_SCANCODE_DOWN},
    {"menu_left", "Меню: влево", SDL_SCANCODE_A, SDL_SCANCODE_LEFT},
    {"menu_right", "Меню: вправо", SDL_SCANCODE_D, SDL_SCANCODE_RIGHT},
    {"menu_confirm", "Меню: подтвердить", SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE},
    {"menu_back", "Меню: назад", SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE},
    {"menu_previous", "Меню: предыдущая вкладка (L1)", SDL_SCANCODE_Q, SDL_SCANCODE_1},
    {"menu_next", "Меню: следующая вкладка (R1)", SDL_SCANCODE_E, SDL_SCANCODE_3},
};

struct Values {
    Values() {
        for (int a = 0; a < BB_KEY_COUNT; ++a) {
            keyboard[a][0] = KeyBindings[a].primary;
            keyboard[a][1] = KeyBindings[a].secondary;
        }
    }
    std::atomic<int> keyboard[BB_KEY_COUNT][2]{};
    std::atomic<int> mouse_extra_action[3]{}; // middle, X1, X2

    std::atomic<int> upscaler{UpscalerFsr3};
    std::atomic<int> preset{NativeAA};
    std::atomic<bool> sharpen{true};
    std::atomic<float> sharpness{0.3f};
    std::atomic<bool> jitter{true};
    std::atomic<bool> reactive{false};
    std::atomic<bool> object_motion{true};
    std::atomic<float> reactive_scale{1.0f};
    std::atomic<float> reactive_threshold{0.2f};
    std::atomic<float> reactive_max{0.9f};
    std::atomic<int> debug_view{DebugNone};
    std::atomic<bool> show_fps{false};
    // Mouse input is an optional layer on the virtual pad; defaults preserve gamepad behavior.
    std::atomic<bool> mouse_enabled{true}, mouse_invert_y{false}, mouse_aspect_compensation{true};
    std::atomic<float> mouse_sensitivity_x{1.0f}, mouse_sensitivity_y{1.0f};
    std::atomic<int> mouse_left_action{BB_MOUSE_ACTION_R1};
    std::atomic<int> mouse_right_action{BB_MOUSE_ACTION_L2};
    // FSR 4 checks (menu): the provider's auto exposure, the jitter sign it is given.
    std::atomic<bool> fsr4_auto_exposure{true};
    std::atomic<bool> fsr4_invert_jitter{false};
    std::atomic<int> active_render_width{1920}, active_render_height{1080};
    /// Applied at start (patches.py); the menu shows when a restart is needed.
    std::atomic<bool> effects[EffectCount]{};
    std::atomic<int> model_lod{0}; ///< -2 highest .. 2 lowest, 0 the game's
    std::atomic<int> output_res{OutputDefault}; ///< index into OutputWidths
    /// Live resolution and preset changes (run.sh): 0 off by default (startup patch, fastest
    /// on the Steam Deck and older GPUs), -1 auto (strong discrete GPUs), 1 on. On restart.
    std::atomic<int> live_resolution{0};
    /// Why FSR 4 cannot run (assets, device features), or null. Set by the renderer.
    std::atomic<const char*> fsr4_problem{nullptr};
    std::atomic<bool> fsr4_supported{false}, fsr411_supported{false};

    /// Startup settings for the explicit BB_RENDER_RES compatibility patch only.
    int startup_preset = NativeAA;
    int startup_upscaler = UpscalerFsr3;
    bool startup_object_motion = true;
    bool startup_effects[EffectCount]{};
    int startup_model_lod = 0;
    int startup_output_res = OutputDefault;
    int startup_live_resolution = 0;
};

Values& Get();

/// Reads the file, then the environment overrides. Called once at start.
void Load();
/// Checks the loaded choice before the first frame; unsupported FSR 4 uses FSR 3.1.
void ConfigureUpscalerSupport(bool fsr4, bool fsr411);
/// Startup-patched scene dimensions cannot change until run.sh prepares a new image.
bool FixedRenderSession();
int RenderPreset();
bool ResolutionNeedsRestart();
/// Writes the file (menu changes).
void Save();

/// Render resolution divisor of a preset (1.0 native, 1.5 quality, ...).
float PresetScale(int preset);
const char* PresetName(int preset);
const char* UpscalerName(int upscaler);

} // namespace BbSettings
