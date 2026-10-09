// SPDX-License-Identifier: GPL-2.0-or-later
#include "bbport_settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <string_view>

namespace BbSettings {

namespace {

const char* Path() {
    const char* env = std::getenv("BB_CONFIG");
    return env && env[0] ? env : "bbport.ini";
}

float Clamp(float v, float lo, float hi) {
    return std::clamp(v, lo, hi);
}

void Set(Values& v, const std::string& key, const std::string& value) {
    const float f = float(std::atof(value.c_str()));
    const int i = std::atoi(value.c_str());
    if (key == "upscaler") {
        for (int u = 0; u < UpscalerCount; ++u) {
            if (value == UpscalerName(u)) {
                v.upscaler = u;
            }
        }
    } else if (key == "preset") {
        v.preset = std::clamp(i, 0, PresetCount - 1);
    } else if (key == "sharpen") {
        v.sharpen = i != 0;
    } else if (key == "sharpness") {
        v.sharpness = Clamp(f, 0.0f, 2.0f);
    } else if (key == "jitter") {
        v.jitter = i != 0;
    } else if (key == "reactive") {
        v.reactive = i != 0;
    } else if (key == "object_motion") {
        v.object_motion = i != 0;
    } else if (key == "reactive_scale") {
        v.reactive_scale = Clamp(f, 0.0f, 16.0f);
    } else if (key == "reactive_threshold") {
        v.reactive_threshold = Clamp(f, 0.0f, 1.0f);
    } else if (key == "reactive_max") {
        v.reactive_max = Clamp(f, 0.0f, 1.0f);
    } else if (key == "debug_view") {
        v.debug_view = std::clamp(i, 0, DebugViewCount - 1);
    } else if (key == "show_fps") {
        v.show_fps = i != 0;
    } else if (key == "mouse_enabled") {
        v.mouse_enabled = i != 0;
    } else if (key == "mouse_sensitivity_x") {
        v.mouse_sensitivity_x = std::isfinite(f) ? Clamp(f, 0.1f, 5.0f) : 1.0f;
    } else if (key == "mouse_sensitivity_y") {
        v.mouse_sensitivity_y = std::isfinite(f) ? Clamp(f, 0.1f, 5.0f) : 1.0f;
    } else if (key == "mouse_sensitivity") { // migrate the former shared slider
        const float sensitivity = std::isfinite(f) ? Clamp(f, 0.1f, 5.0f) : 1.0f;
        v.mouse_sensitivity_x = sensitivity;
        v.mouse_sensitivity_y = sensitivity;
    } else if (key == "mouse_aspect_compensation") {
        v.mouse_aspect_compensation = i != 0;
    } else if (key == "mouse_invert_y") {
        v.mouse_invert_y = i != 0;
    } else if (key == "mouse_left_action") {
        v.mouse_left_action = std::clamp(i, 0, BB_MOUSE_ACTION_COUNT - 1);
    } else if (key == "mouse_right_action") {
        v.mouse_right_action = std::clamp(i, 0, BB_MOUSE_ACTION_COUNT - 1);
    } else if (key == "fsr4_auto_exposure") {
        v.fsr4_auto_exposure = i != 0;
    } else if (key == "fsr4_invert_jitter") {
        v.fsr4_invert_jitter = i != 0;
    } else if (key == "model_lod") {
        v.model_lod = std::clamp(i, -2, 2);
    } else if (key == "live_resolution") {
        v.live_resolution = value == "auto" ? -1 : std::clamp(i, 0, 1);
    } else if (key == "output_res") {
        for (int r = 0; r < OutputCount; ++r) {
            if (value == std::to_string(OutputWidths[r]) + "x" + std::to_string(OutputHeights[r])) {
                v.output_res = r;
            }
        }
    } else {
        for (int a = 0; a < BB_KEY_COUNT; ++a) {
            for (int slot = 0; slot < 2; ++slot) {
                if (key == std::string("key_") + KeyBindings[a].key + (slot ? "_secondary" : "")) {
                    v.keyboard[a][slot] = i > 0 && i < SDL_SCANCODE_COUNT && i != SDL_SCANCODE_INSERT ? i : 0;
                }
            }
        }
        static constexpr const char* extra[] = {"mouse_middle_action", "mouse_x1_action", "mouse_x2_action"};
        for (int b = 0; b < 3; ++b) {
            if (key == extra[b]) v.mouse_extra_action[b] = std::clamp(i, 0, BB_MOUSE_ACTION_COUNT - 1);
        }
        for (int e = 0; e < EffectCount; ++e) {
            if (key == Effects[e].key) {
                v.effects[e] = i != 0;
            }
        }
    }
}

} // namespace

extern "C" void bbgpu_mouse_config(int* enabled, float* sensitivity_x, float* sensitivity_y,
                                   float* aspect_scale, int* invert_y,
                                   int* left_action, int* right_action) {
    const auto& v = Get();
    if (enabled) *enabled = v.mouse_enabled.load();
    if (sensitivity_x) *sensitivity_x = v.mouse_sensitivity_x.load();
    if (sensitivity_y) *sensitivity_y = v.mouse_sensitivity_y.load();
    if (aspect_scale) {
        const int output = std::clamp(v.output_res.load(), 0, OutputCount - 1);
        *aspect_scale = v.mouse_aspect_compensation.load()
            ? (float(OutputWidths[output]) / OutputHeights[output]) / (16.0f / 9.0f)
            : 1.0f;
    }
    if (invert_y) *invert_y = v.mouse_invert_y.load();
    if (left_action) *left_action = v.mouse_left_action.load();
    if (right_action) *right_action = v.mouse_right_action.load();
}

extern "C" void bbgpu_keyboard_config(int keys[BB_KEY_COUNT][2]) {
    const auto& v = Get();
    for (int a = 0; a < BB_KEY_COUNT; ++a)
        for (int slot = 0; slot < 2; ++slot) keys[a][slot] = v.keyboard[a][slot].load();
}

extern "C" int bbgpu_mouse_button_action(int button) {
    const auto& v = Get();
    if (button == 0) return v.mouse_left_action.load();
    if (button == 1) return v.mouse_right_action.load();
    return button >= 2 && button < 5 ? v.mouse_extra_action[button - 2].load() : 0;
}

Values& Get() {
    static Values values;
    return values;
}

void Load() {
    auto& v = Get();
    for (int e = 0; e < EffectCount; ++e) {
        v.effects[e] = Effects[e].default_on;
    }
    if (FILE* file = std::fopen(Path(), "r")) {
        char line[256];
        while (std::fgets(line, sizeof(line), file)) {
            std::string text{line};
            text.erase(text.find_last_not_of(" \t\r\n") + 1);
            const auto eq = text.find('=');
            if (text.empty() || text[0] == '#' || eq == std::string::npos) {
                continue;
            }
            Set(v, text.substr(0, eq), text.substr(eq + 1));
        }
        std::fclose(file);
        std::printf("Settings: %s\n", Path());
    }
    // Environment overrides (scripts, A/B tests).
    if (const char* env = std::getenv("BB_UPSCALER")) {
        v.upscaler = UpscalerOff;
        for (int u = 0; u < UpscalerCount; ++u) {
            if (std::strcmp(env, UpscalerName(u)) == 0) v.upscaler = u;
        }
    }
    const std::pair<const char*, const char*> env_keys[] = {
        {"BB_FSR_SHARPNESS", "sharpness"},        {"BB_JITTER", "jitter"},
        {"BB_REACTIVE", "reactive"},              {"BB_REACTIVE_SCALE", "reactive_scale"},
        {"BB_REACTIVE_THRESHOLD", "reactive_threshold"}, {"BB_REACTIVE_MAX", "reactive_max"},
        {"BB_UPSCALE_PRESET", "preset"},            {"BB_OBJECT_MOTION", "object_motion"},
    };
    for (const auto& [env, key] : env_keys) {
        if (const char* value = std::getenv(env)) {
            Set(v, key, value);
        }
    }
    v.startup_preset = v.preset;
    v.startup_upscaler = v.upscaler;
    v.startup_object_motion = v.object_motion;
    for (int e = 0; e < EffectCount; ++e) {
        v.startup_effects[e] = v.effects[e];
    }
    v.startup_model_lod = v.model_lod;
    v.startup_output_res = v.output_res;
    v.startup_live_resolution = v.live_resolution;
}

void ConfigureUpscalerSupport(bool fsr4, bool fsr411) {
    auto& v = Get();
    v.fsr4_supported = fsr4;
    v.fsr411_supported = fsr4 && fsr411;
    const int requested = v.upscaler;
    if ((requested == UpscalerFsr4 && !v.fsr4_supported) ||
        (requested == UpscalerFsr411 && !v.fsr411_supported)) {
        v.fsr4_problem = "GPU does not support the selected FSR 4 shaders; using FSR 3.1";
        std::printf("Upscaler: %s unsupported on this GPU; falling back to FSR 3.1 before the first frame\n",
                    UpscalerName(requested));
        v.upscaler = UpscalerFsr3;
    }
}

bool FixedRenderSession() {
    const char* size = std::getenv("BB_RENDER_RES");
    return size && size[0];
}

int RenderPreset() {
    const auto& v = Get();
    return FixedRenderSession() ? v.startup_preset :
        v.upscaler == UpscalerTaa ? NativeAA : v.preset.load();
}

bool ResolutionNeedsRestart() {
    const auto& v = Get();
    const int output = v.output_res.load();
    const int startup_output = v.startup_output_res;
    const bool aspect_changed =
        OutputWidths[output] * OutputHeights[startup_output] !=
        OutputWidths[startup_output] * OutputHeights[output];
    if (aspect_changed) return true;
    // TAA needs the live path (native guest targets): run.sh selects it on restart.
    return FixedRenderSession() &&
        (v.preset != v.startup_preset || v.output_res != v.startup_output_res ||
         (v.upscaler == UpscalerOff) != (v.startup_upscaler == UpscalerOff) ||
         (v.upscaler == UpscalerTaa) != (v.startup_upscaler == UpscalerTaa));
}

void Save() {
    const auto& v = Get();
    FILE* file = std::fopen(Path(), "w");
    if (!file) {
        std::printf("Settings: cannot write %s\n", Path());
        return;
    }
    std::fprintf(file,
                 "# bbport settings (in-game menu: Insert / L3+R3)\n"
                 "upscaler=%s\npreset=%d\nsharpen=%d\nsharpness=%.2f\njitter=%d\n"
                 "reactive=%d\nobject_motion=%d\nreactive_scale=%.2f\nreactive_threshold=%.2f\nreactive_max=%.2f\n"
                 "debug_view=%d\nshow_fps=%d\nfsr4_auto_exposure=%d\nfsr4_invert_jitter=%d\n"
                 "mouse_enabled=%d\nmouse_sensitivity_x=%.2f\nmouse_sensitivity_y=%.2f\n"
                 "mouse_aspect_compensation=%d\nmouse_invert_y=%d\n"
                 "mouse_left_action=%d\nmouse_right_action=%d\n",
                 UpscalerName(v.upscaler), v.preset.load(), int(v.sharpen.load()),
                 v.sharpness.load(), int(v.jitter.load()), int(v.reactive.load()),
                 int(v.object_motion.load()),
                 v.reactive_scale.load(), v.reactive_threshold.load(), v.reactive_max.load(),
                 v.debug_view.load(), int(v.show_fps.load()),
                 int(v.fsr4_auto_exposure.load()), int(v.fsr4_invert_jitter.load()),
                 int(v.mouse_enabled.load()), v.mouse_sensitivity_x.load(),
                 v.mouse_sensitivity_y.load(), int(v.mouse_aspect_compensation.load()),
                 int(v.mouse_invert_y.load()), v.mouse_left_action.load(),
                 v.mouse_right_action.load());
    for (int a = 0; a < BB_KEY_COUNT; ++a) {
        std::fprintf(file, "key_%s=%d\nkey_%s_secondary=%d\n", KeyBindings[a].key,
                     v.keyboard[a][0].load(), KeyBindings[a].key, v.keyboard[a][1].load());
    }
    std::fprintf(file, "mouse_middle_action=%d\nmouse_x1_action=%d\nmouse_x2_action=%d\n",
                 v.mouse_extra_action[0].load(), v.mouse_extra_action[1].load(), v.mouse_extra_action[2].load());
    // Read by patches.py at start.
    for (int e = 0; e < EffectCount; ++e) {
        std::fprintf(file, "%s=%d\n", Effects[e].key, int(v.effects[e].load()));
    }
    std::fprintf(file, "model_lod=%d\noutput_res=%dx%d\n", v.model_lod.load(),
                 OutputWidths[v.output_res], OutputHeights[v.output_res]);
    // Read by run.sh at start.
    std::fprintf(file, "live_resolution=%s\n", v.live_resolution < 0 ? "auto"
                                                  : v.live_resolution ? "1" : "0");
    std::fclose(file);
}

float PresetScale(int preset) {
    static constexpr float scales[PresetCount] = {1.0f, 1.5f, 1.7f, 2.0f, 3.0f};
    return scales[std::clamp(preset, 0, PresetCount - 1)];
}

const char* PresetName(int preset) {
    static constexpr const char* names[PresetCount] = {"Native AA", "Quality", "Balanced",
                                                       "Performance", "Ultra Performance"};
    return names[std::clamp(preset, 0, PresetCount - 1)];
}

const char* UpscalerName(int upscaler) {
    static constexpr const char* names[UpscalerCount] = {"off", "fsr3", "fsr4", "fsr411", "taa"};
    return names[std::clamp(upscaler, 0, UpscalerCount - 1)];
}

} // namespace BbSettings
