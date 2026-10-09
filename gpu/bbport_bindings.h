// SPDX-License-Identifier: GPL-2.0-or-later
// Shared action ids; key values are SDL scancodes (physical keys).
#pragma once

enum BbKeyAction {
    BB_KEY_MOVE_UP, BB_KEY_MOVE_DOWN, BB_KEY_MOVE_LEFT, BB_KEY_MOVE_RIGHT,
    BB_KEY_CAMERA_UP, BB_KEY_CAMERA_DOWN, BB_KEY_CAMERA_LEFT, BB_KEY_CAMERA_RIGHT,
    BB_KEY_CROSS, BB_KEY_CIRCLE, BB_KEY_SQUARE, BB_KEY_TRIANGLE,
    BB_KEY_L1, BB_KEY_R1, BB_KEY_L2, BB_KEY_R2, BB_KEY_L3, BB_KEY_R3,
    BB_KEY_OPTIONS, BB_KEY_TOUCH_LEFT, BB_KEY_TOUCH_RIGHT,
    BB_KEY_UP, BB_KEY_DOWN, BB_KEY_LEFT, BB_KEY_RIGHT,
    BB_KEY_MENU_UP, BB_KEY_MENU_DOWN, BB_KEY_MENU_LEFT, BB_KEY_MENU_RIGHT,
    BB_KEY_MENU_CONFIRM, BB_KEY_MENU_BACK, BB_KEY_MENU_PREVIOUS, BB_KEY_MENU_NEXT,
    BB_KEY_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif
void bbgpu_keyboard_config(int keys[BB_KEY_COUNT][2]);
int bbgpu_mouse_button_action(int button);
#ifdef __cplusplus
}
#endif
