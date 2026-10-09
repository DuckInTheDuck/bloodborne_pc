// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <SDL3/SDL_scancode.h>
#include "../gpu/shim/bbport_settings.cpp"

int main() {
#ifdef _WIN32
    _putenv_s("BB_CONFIG", "out/controls-test.ini");
#else
    setenv("BB_CONFIG", "out/controls-test.ini", 1);
#endif
    using namespace BbSettings;
    auto& v = Get();
    assert(v.keyboard[BB_KEY_OPTIONS][0] == SDL_SCANCODE_RETURN);
    assert(v.keyboard[BB_KEY_OPTIONS][1] == SDL_SCANCODE_ESCAPE);
    assert(v.keyboard[BB_KEY_CAMERA_UP][0] == 0);
    assert(v.keyboard[BB_KEY_CAMERA_DOWN][0] == 0);
    assert(v.keyboard[BB_KEY_UP][0] == SDL_SCANCODE_UP);
    assert(v.keyboard[BB_KEY_DOWN][0] == SDL_SCANCODE_DOWN);
    assert(v.keyboard[BB_KEY_LEFT][0] == SDL_SCANCODE_LEFT);
    assert(v.keyboard[BB_KEY_RIGHT][0] == SDL_SCANCODE_RIGHT);
    Set(v,"key_options","19");
    Set(v,"key_options_secondary","0");
    Set(v,"key_move_up","512"); // invalid
    Set(v,"key_cross","73"); // Insert reserved
    assert(v.keyboard[BB_KEY_MOVE_UP][0] == 0 && v.keyboard[BB_KEY_CROSS][0] == 0);
    Set(v,"mouse_x2_action",std::to_string(BB_MOUSE_ACTION_R3));
    for (int r = 0; r < OutputCount; ++r) {
        Set(v, "output_res", std::to_string(OutputWidths[r])+"x"+std::to_string(OutputHeights[r]));
        assert(v.output_res == r);
    }
    Save();
    v.keyboard[BB_KEY_OPTIONS][0]=0;
    v.keyboard[BB_KEY_OPTIONS][1]=SDL_SCANCODE_X;
    v.mouse_extra_action[2]=0;
    Load();
    int keys[BB_KEY_COUNT][2]; bbgpu_keyboard_config(keys);
    assert(keys[BB_KEY_OPTIONS][0]==SDL_SCANCODE_P && keys[BB_KEY_OPTIONS][1]==0);
    assert(bbgpu_mouse_button_action(4)==BB_MOUSE_ACTION_R3);
    assert(bbgpu_mouse_button_action(10)==BB_MOUSE_ACTION_NONE);
    assert(keys[BB_KEY_MOVE_DOWN][0]==SDL_SCANCODE_S);
    std::remove("out/controls-test.ini");
    std::puts("PASS: default bindings, invalid/reserved keys, unbound secondary, keyboard/mouse save-load round trip");
}
