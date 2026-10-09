// SPDX-License-Identifier: GPL-2.0-or-later
#define _GNU_SOURCE
#include <assert.h>
#include "../src/runtime_pad.c"

static int configured[BB_KEY_COUNT][2];
static int actions[5], mouse_on;
static int capture, camera_reset;
void runtime_menu_observe(void) {}
static int menu_test_context;
int runtime_menu_context(void) { return menu_test_context; }
int bbgpu_overlay_captures_input(void) { return capture; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name; return 0;
}
void bbgpu_keyboard_config(int keys[BB_KEY_COUNT][2]) { memcpy(keys,configured,sizeof(configured)); }
int bbgpu_mouse_button_action(int button) { return actions[button]; }
void bbgpu_mouse_config(int *enabled, float *sx, float *sy, float *aspect, int *invert, int *left, int *right) {
    *enabled=mouse_on; *sx=*sy=*aspect=1; *invert=0; *left=actions[0]; *right=actions[1];
}
void runtime_mouse_camera_disable(void) {}
int runtime_mouse_camera_update(float dx,float dy,float sx,float sy,float aspect,int invert,
                               int reset,int8_t rx,int8_t ry) {
    (void)dx; (void)dy; (void)sx; (void)sy; (void)aspect; (void)invert;
    camera_reset=reset; (void)rx; (void)ry; return 1;
}

static PadData neutral(void) {
    PadData d={0}; d.left_x=d.left_y=d.right_x=d.right_y=128; return d;
}
int main(void) {
    bool keys[SDL_SCANCODE_COUNT]={false};
    configured[BB_KEY_OPTIONS][0]=SDL_SCANCODE_RETURN;
    configured[BB_KEY_OPTIONS][1]=SDL_SCANCODE_ESCAPE;
    keys[SDL_SCANCODE_ESCAPE]=true;
    PadData d=neutral(); apply_keyboard(&d,keys); assert(d.buttons==BTN_OPTIONS);
    configured[BB_KEY_OPTIONS][1]=SDL_SCANCODE_P;
    d=neutral(); apply_keyboard(&d,keys); assert(!d.buttons);
    keys[SDL_SCANCODE_P]=true;
    d=neutral(); apply_keyboard(&d,keys); assert(d.buttons==BTN_OPTIONS);
    configured[BB_KEY_CROSS][0]=SDL_SCANCODE_P; // intentional duplicate
    d=neutral(); apply_keyboard(&d,keys); assert(d.buttons==(BTN_OPTIONS|BTN_CROSS));
    memset(keys,0,sizeof(keys));
    configured[BB_KEY_MOVE_LEFT][0]=SDL_SCANCODE_A;
    configured[BB_KEY_MOVE_RIGHT][0]=SDL_SCANCODE_D;
    d=neutral(); d.left_x=200; apply_keyboard(&d,keys); assert(d.left_x==200);
    keys[SDL_SCANCODE_A]=true; apply_keyboard(&d,keys); assert(d.left_x==0);
    keys[SDL_SCANCODE_D]=true; apply_keyboard(&d,keys); assert(d.left_x==128);
    keys[SDL_SCANCODE_A]=false; apply_keyboard(&d,keys); assert(d.left_x==255);
    configured[BB_KEY_MOVE_LEFT][0]=SDL_SCANCODE_COUNT+100;
    configured[BB_KEY_MOVE_LEFT][1]=-5;
    memset(keys,0,sizeof(keys)); d=neutral(); apply_keyboard(&d,keys); assert(d.left_x==128);
    d=neutral(); d.buttons=BTN_L2; d.l2=80; apply_keyboard(&d,keys); assert(d.l2==80);
    configured[BB_KEY_R2][0]=SDL_SCANCODE_F;
    keys[SDL_SCANCODE_F]=true; d=neutral(); apply_keyboard(&d,keys); assert(d.r2==255 && (d.buttons&BTN_R2));
    memset(keys,0,sizeof(keys));
    configured[BB_KEY_MENU_UP][0]=SDL_SCANCODE_W;
    configured[BB_KEY_MENU_BACK][0]=SDL_SCANCODE_ESCAPE;
    configured[BB_KEY_MENU_CONFIRM][0]=SDL_SCANCODE_RETURN;
    menu_test_context=1; d=neutral(); apply_keyboard(&d,keys);
    keys[SDL_SCANCODE_W]=true; d=neutral(); apply_keyboard(&d,keys);
    assert(d.buttons==BTN_UP && d.left_y==128);
    keys[SDL_SCANCODE_W]=false; keys[SDL_SCANCODE_ESCAPE]=true;
    d=neutral(); apply_keyboard(&d,keys); assert(d.buttons==BTN_CIRCLE);
    menu_test_context=0; d=neutral(); apply_keyboard(&d,keys); assert(!d.buttons);
    keys[SDL_SCANCODE_ESCAPE]=false; apply_keyboard(&d,keys);
    configured[BB_KEY_OPTIONS][1]=SDL_SCANCODE_ESCAPE;
    keys[SDL_SCANCODE_ESCAPE]=true; d=neutral(); apply_keyboard(&d,keys); assert(d.buttons==BTN_OPTIONS);
    menu_test_context=2; d=neutral(); apply_keyboard(&d,keys); assert(!d.buttons);
    memset(keys,0,sizeof(keys)); apply_keyboard(&d,keys);
    keys[SDL_SCANCODE_RETURN]=true; d=neutral(); d.buttons=BTN_L2; d.left_x=200;
    apply_keyboard(&d,keys); assert(d.buttons==(BTN_L2|BTN_CROSS) && d.left_x==200);
    memset(keys,0,sizeof(keys)); menu_test_context=0; apply_keyboard(&d,keys);
    configured[BB_KEY_UP][0]=SDL_SCANCODE_UP;
    configured[BB_KEY_MOVE_UP][0]=SDL_SCANCODE_W;
    memset(keys,0,sizeof(keys)); keys[SDL_SCANCODE_UP]=keys[SDL_SCANCODE_W]=true;
    d=neutral(); apply_keyboard(&d,keys);
    assert(d.buttons==BTN_UP && d.left_y==0 && d.right_y==128);
    memset(keys,0,sizeof(keys)); apply_keyboard(&d,keys);
    mouse_on=1; actions[4]=BB_MOUSE_ACTION_R2;
    actions[2]=BB_MOUSE_ACTION_CROSS;
    runtime_pad_mouse_button(4,1); runtime_pad_mouse_button(2,1);
    d=neutral(); apply_mouse(&d); assert((d.buttons&(BTN_R2|BTN_CROSS))==(BTN_R2|BTN_CROSS) && d.r2==255);
    runtime_pad_mouse_reset(); d=neutral(); apply_mouse(&d); assert(!d.buttons);
    actions[2]=BB_MOUSE_ACTION_R3; runtime_pad_mouse_button(2,1);
    d=neutral(); apply_mouse(&d); assert(camera_reset && (d.buttons&BTN_R3));
    runtime_pad_mouse_reset(); d=neutral(); d.buttons=BTN_L2; d.l2=80;
    apply_mouse(&d); assert(d.l2==80);
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
    capture=1; sample_host(&d); assert(!d.buttons && d.left_x==128);
    SDL_Quit();
    puts("PASS: rebinds, secondary/unbound/invalid keys, duplicate actions, controller axes, triggers, five mouse buttons, overlay capture");
}
