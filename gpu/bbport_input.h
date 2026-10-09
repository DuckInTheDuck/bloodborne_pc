// Small C ABI shared by the SDL window, host pad sampler and bbport settings.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum BbMouseAction {
    BB_MOUSE_ACTION_NONE = 0,
    BB_MOUSE_ACTION_R1,
    BB_MOUSE_ACTION_R2,
    BB_MOUSE_ACTION_L1,
    BB_MOUSE_ACTION_L2,
    BB_MOUSE_ACTION_CROSS,
    BB_MOUSE_ACTION_CIRCLE,
    BB_MOUSE_ACTION_SQUARE,
    BB_MOUSE_ACTION_TRIANGLE,
    BB_MOUSE_ACTION_R3,
    BB_MOUSE_ACTION_L3,
    BB_MOUSE_ACTION_COUNT,
};

// Button ids: 0=left, 1=right, 2=middle, 3=X1, 4=X2. Motion accumulates until a pad read.
void runtime_pad_mouse_motion(float dx, float dy);
void runtime_pad_mouse_button(int button, int down);
void runtime_pad_mouse_reset(void);

// Reads live in-game settings. Integer action values use BbMouseAction above.
void bbgpu_mouse_config(int* enabled, float* sensitivity_x, float* sensitivity_y,
                        float* aspect_scale, int* invert_y, int* left_action,
                        int* right_action);

#ifdef __cplusplus
}
#endif
