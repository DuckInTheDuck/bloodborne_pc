/* libScePad on SDL3 gamepads, with a keyboard fallback. SDL events are pumped
 * by the window thread (gpu/shim/window.cpp); here state is only sampled.
 *
 * Default keyboard layout (also works alongside a gamepad; remappable in the overlay):
 *   WASD left stick, arrow keys right stick, Space Cross, LShift Circle,
 *   E Square, Q Triangle, 1 L1, 3 R1, R L2, F R2, Z L3, C R3,
 *   Enter/Escape Options, Tab left touchpad, Backspace right touchpad,
 *   IJKL d-pad (I up, K down, J left, L right). */
#define _GNU_SOURCE
#include "runtime.h"
#include "gpu/bbgpu.h"
#include "gpu/bbport_input.h"
#include "gpu/bbport_bindings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <SDL3/SDL.h>
#include <sys/stat.h>

#define ERR_INVALID_ARG ((int32_t)0x80920001)
#define ERR_INVALID_HANDLE ((int32_t)0x80920003)
#define ERR_ALREADY_OPENED ((int32_t)0x80920004)
#define ERR_NOT_INITIALIZED ((int32_t)0x80920005)
#define PAD_HANDLE 1

enum {
    BTN_L3=0x2, BTN_R3=0x4, BTN_OPTIONS=0x8, BTN_UP=0x10, BTN_RIGHT=0x20, BTN_DOWN=0x40, BTN_LEFT=0x80,
    BTN_L2=0x100, BTN_R2=0x200, BTN_L1=0x400, BTN_R1=0x800, BTN_TRIANGLE=0x1000, BTN_CIRCLE=0x2000,
    BTN_CROSS=0x4000, BTN_SQUARE=0x8000, BTN_TOUCHPAD=0x100000,
};
typedef struct { uint16_t x, y; uint8_t id, reserve[3]; } PadTouch;
typedef struct {
    uint32_t buttons;
    uint8_t left_x, left_y, right_x, right_y;
    uint8_t l2, r2, analog_padding[2];
    float orientation[4], acceleration[3], angular_velocity[3];
    uint8_t touch_count, touch_reserve[3];
    uint32_t touch_held_time;
    PadTouch touches[2];
    uint8_t connected, pad0[3];
    uint64_t timestamp;
    uint8_t extension[16];
    uint8_t connected_count, reserve[2], unique_length, unique[12];
} PadData;
typedef struct {
    float pixel_density; uint16_t resolution_x, resolution_y;
    uint8_t dead_zone_left, dead_zone_right, connection_type, connected_count;
    uint8_t connected, pad[3];
    int32_t device_class;
    uint8_t reserve[8];
} ControllerInfo;
_Static_assert(sizeof(PadData)==120,"OrbisPadData layout");
_Static_assert(sizeof(PadTouch)==8,"OrbisPadTouch layout");
_Static_assert(__builtin_offsetof(PadData,touches)==60,"OrbisPadData touch offset");
_Static_assert(__builtin_offsetof(PadData,timestamp)==80,"OrbisPadData timestamp offset");
_Static_assert(sizeof(ControllerInfo)==28,"OrbisPadControllerInformation layout");

static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static int initialized, opened, sdl_ready;
static SDL_Gamepad *gamepad;
static size_t reads;
static uint8_t connected_count;
static float mouse_dx, mouse_dy;
static uint8_t mouse_buttons;

void runtime_pad_mouse_motion(float dx, float dy) {
    pthread_mutex_lock(&lock);
    if (dx == dx && dy == dy) { /* ignore NaN from a faulty window backend */
        mouse_dx += dx;
        mouse_dy += dy;
    }
    pthread_mutex_unlock(&lock);
}

void runtime_pad_mouse_button(int button, int down) {
    if (button < 0 || button >= 5) return;
    pthread_mutex_lock(&lock);
    if (down) mouse_buttons |= (uint8_t)(1u << button);
    else mouse_buttons &= (uint8_t)~(1u << button);
    pthread_mutex_unlock(&lock);
}

void runtime_pad_mouse_reset(void) {
    pthread_mutex_lock(&lock);
    mouse_dx = mouse_dy = 0.0f;
    mouse_buttons = 0;
    pthread_mutex_unlock(&lock);
}

static uint32_t mouse_action_button(int action) {
    static const uint32_t buttons[BB_MOUSE_ACTION_COUNT] = {
        0, BTN_R1, BTN_R2, BTN_L1, BTN_L2, BTN_CROSS, BTN_CIRCLE, BTN_SQUARE,
        BTN_TRIANGLE, BTN_R3, BTN_L3,
    };
    return action >= 0 && action < BB_MOUSE_ACTION_COUNT ? buttons[action] : 0;
}

static void apply_mouse(PadData *d) {
    int enabled = 0, invert_y = 0, left_action = 0, right_action = 0;
    float aspect_scale = 1.0f;
    float sensitivity_x = 1.0f, sensitivity_y = 1.0f;
    bbgpu_mouse_config(&enabled, &sensitivity_x, &sensitivity_y, &aspect_scale,
                       &invert_y, &left_action, &right_action);
    if (!enabled) {
        mouse_dx = mouse_dy = 0.0f;
        runtime_mouse_camera_disable();
        return;
    }

    uint32_t pressed = 0;
    for (int b = 0; b < 5; ++b)
        if (mouse_buttons & (1u << b)) pressed |= mouse_action_button(bbgpu_mouse_button_action(b));
    d->buttons |= pressed;
    if (pressed & BTN_L2) d->l2 = 255;
    if (pressed & BTN_R2) d->r2 = 255;

    if (runtime_mouse_camera_update(mouse_dx, mouse_dy, sensitivity_x, sensitivity_y,
                                    aspect_scale, invert_y, (d->buttons & BTN_R3) != 0,
                                    (int8_t)((int)d->right_x - 128),
                                    (int8_t)((int)d->right_y - 128))) {
        mouse_dx = mouse_dy = 0.0f;
        return;
    }

    // Keep up to two pad samples of excess so fast motion is not discarded at the
    // stick limit. This remains virtual-stick input and therefore keeps its speed cap.
    const float xscale = sensitivity_x * 4.0f * aspect_scale;
    const float yscale = sensitivity_y * 4.0f;
    const float max_dx = 127.0f / xscale;
    const float max_dy = 127.0f / yscale;
    const float dx = fmaxf(-max_dx * 2.0f, fminf(mouse_dx, max_dx * 2.0f));
    const float dy = fmaxf(-max_dy * 2.0f, fminf(mouse_dy, max_dy * 2.0f));
    mouse_dx -= dx;
    mouse_dy -= dy;
    mouse_dx = fmaxf(-max_dx, fminf(mouse_dx, max_dx));
    mouse_dy = fmaxf(-max_dy, fminf(mouse_dy, max_dy));
    const int mx = (int)lroundf(dx * xscale);
    int my = (int)lroundf(dy * yscale);
    if (invert_y) my = -my;
    const int rx = (int)d->right_x - 128 + mx;
    const int ry = (int)d->right_y - 128 + my;
    d->right_x = (uint8_t)(rx < -128 ? 0 : rx > 127 ? 255 : rx + 128);
    d->right_y = (uint8_t)(ry < -128 ? 0 : ry > 127 ? 255 : ry + 128);
}

static uint64_t now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000u+(uint64_t)t.tv_nsec/1000u; }
static uint8_t axis(int16_t v) { int x=(v+32768)>>8; return (uint8_t)(x<0 ? 0 : x>255 ? 255 : x); }
static uint8_t trigger(int16_t v) { int x=v>>7; return (uint8_t)(x<0 ? 0 : x>255 ? 255 : x); }
static uint16_t touch_axis(float v, int max) {
    return (uint16_t)(v<=0.0f ? 0 : v>=1.0f ? max : (int)(v*max+0.5f));
}
static void touch_click(PadData *d, int right) {
    d->buttons|=BTN_TOUCHPAD;
    d->touch_count=1;
    d->touches[0]=(PadTouch){.x=right ? 1440 : 480,.y=471,.id=0};
}

// The caller supplies a keyboard snapshot; also used by the input regression test.
static bool keyboard_blocked[SDL_SCANCODE_COUNT];
static int keyboard_previous_menu=-1;
static void apply_keyboard(PadData *d, const bool *k) {
    if (k) {
        const int in_menu=runtime_menu_context()!=0;
        if (keyboard_previous_menu>=0 && in_menu!=keyboard_previous_menu)
            for (int i=0;i<SDL_SCANCODE_COUNT;i++) keyboard_blocked[i]=k[i];
        keyboard_previous_menu=in_menu;
        for (int i=0;i<SDL_SCANCODE_COUNT;i++) if (!k[i]) keyboard_blocked[i]=false;
        int keys[BB_KEY_COUNT][2];
        bbgpu_keyboard_config(keys);
        bool held[BB_KEY_COUNT] = {false};
        for (int a = 0; a < BB_KEY_COUNT; ++a) {
            for (int slot = 0; slot < 2; ++slot) {
                const int scancode = keys[a][slot];
                if (scancode > 0 && scancode < SDL_SCANCODE_COUNT && k[scancode] && !keyboard_blocked[scancode]) held[a] = true;
            }
        }
        static const uint32_t buttons[BB_KEY_COUNT] = {
            [BB_KEY_CROSS]=BTN_CROSS, [BB_KEY_CIRCLE]=BTN_CIRCLE,
            [BB_KEY_SQUARE]=BTN_SQUARE, [BB_KEY_TRIANGLE]=BTN_TRIANGLE,
            [BB_KEY_L1]=BTN_L1, [BB_KEY_R1]=BTN_R1, [BB_KEY_L2]=BTN_L2,
            [BB_KEY_R2]=BTN_R2, [BB_KEY_L3]=BTN_L3, [BB_KEY_R3]=BTN_R3,
            [BB_KEY_OPTIONS]=BTN_OPTIONS, [BB_KEY_UP]=BTN_UP, [BB_KEY_DOWN]=BTN_DOWN,
            [BB_KEY_LEFT]=BTN_LEFT, [BB_KEY_RIGHT]=BTN_RIGHT,
        };
        if (in_menu) {
            static const uint32_t menu_buttons[8]={BTN_UP,BTN_DOWN,BTN_LEFT,BTN_RIGHT,BTN_CROSS,BTN_CIRCLE,BTN_L1,BTN_R1};
            for (int a=0;a<8;a++) if (held[BB_KEY_MENU_UP+a]) d->buttons|=menu_buttons[a];
            // Keep the original d-pad bindings as optional alternatives.
            for (int a=BB_KEY_UP;a<=BB_KEY_RIGHT;a++) if (held[a]) d->buttons|=buttons[a];
            return;
        }
        for (int a = 0; a < BB_KEY_MENU_UP; ++a) if (held[a]) d->buttons |= buttons[a];
        if (held[BB_KEY_TOUCH_LEFT]) touch_click(d,0);
        if (held[BB_KEY_TOUCH_RIGHT]) touch_click(d,1);
        // Keyboard axes override the controller only while one of their keys is held.
        if (held[BB_KEY_MOVE_LEFT] || held[BB_KEY_MOVE_RIGHT])
            d->left_x = held[BB_KEY_MOVE_LEFT] == held[BB_KEY_MOVE_RIGHT] ? 128 : held[BB_KEY_MOVE_LEFT] ? 0 : 255;
        if (held[BB_KEY_MOVE_UP] || held[BB_KEY_MOVE_DOWN])
            d->left_y = held[BB_KEY_MOVE_UP] == held[BB_KEY_MOVE_DOWN] ? 128 : held[BB_KEY_MOVE_UP] ? 0 : 255;
        if (held[BB_KEY_CAMERA_LEFT] || held[BB_KEY_CAMERA_RIGHT])
            d->right_x = held[BB_KEY_CAMERA_LEFT] == held[BB_KEY_CAMERA_RIGHT] ? 128 : held[BB_KEY_CAMERA_LEFT] ? 0 : 255;
        if (held[BB_KEY_CAMERA_UP] || held[BB_KEY_CAMERA_DOWN])
            d->right_y = held[BB_KEY_CAMERA_UP] == held[BB_KEY_CAMERA_DOWN] ? 128 : held[BB_KEY_CAMERA_UP] ? 0 : 255;
        if (held[BB_KEY_L2]) d->l2=255;
        if (held[BB_KEY_R2]) d->r2=255;
    }
}

/* Opens the first gamepad SDL knows about; called under lock. */
static SDL_Gamepad *current_gamepad(void) {
    if (!sdl_ready) sdl_ready = SDL_WasInit(SDL_INIT_GAMEPAD) ? 1 : SDL_InitSubSystem(SDL_INIT_GAMEPAD) ? 1 : -1;
    if (sdl_ready<0) return NULL;
    if (gamepad && !SDL_GamepadConnected(gamepad)) { SDL_CloseGamepad(gamepad); gamepad=NULL; }
    if (!gamepad) {
        int count=0;
        SDL_JoystickID *ids=SDL_GetGamepads(&count);
        if (ids && count>0) {
            gamepad=SDL_OpenGamepad(ids[0]);
            if (gamepad) { ++connected_count; printf("Runtime: gamepad connected: %s\n",SDL_GetGamepadName(gamepad)); }
        }
        SDL_free(ids);
    }
    return gamepad;
}
static void sample_host(PadData *d) {
    runtime_menu_observe();
    memset(d,0,sizeof(*d));
    d->left_x=d->left_y=d->right_x=d->right_y=128;
    d->orientation[3]=1.0f;
    d->connected=1; d->connected_count=connected_count ? connected_count : 1;
    d->timestamp=now_us();
    SDL_Gamepad *g=current_gamepad();
    if (bbgpu_overlay_captures_input()) return; /* settings menu open: neutral input */
    const bool *k=SDL_WasInit(SDL_INIT_VIDEO) ? SDL_GetKeyboardState(NULL) : NULL;
    if (g) {
        static const struct { SDL_GamepadButton sdl; uint32_t ps; } map[]={
            {SDL_GAMEPAD_BUTTON_SOUTH,BTN_CROSS}, {SDL_GAMEPAD_BUTTON_EAST,BTN_CIRCLE},
            {SDL_GAMEPAD_BUTTON_WEST,BTN_SQUARE}, {SDL_GAMEPAD_BUTTON_NORTH,BTN_TRIANGLE},
            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,BTN_L1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,BTN_R1},
            {SDL_GAMEPAD_BUTTON_LEFT_STICK,BTN_L3}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK,BTN_R3},
            {SDL_GAMEPAD_BUTTON_START,BTN_OPTIONS}, {SDL_GAMEPAD_BUTTON_BACK,BTN_TOUCHPAD},
            {SDL_GAMEPAD_BUTTON_TOUCHPAD,BTN_TOUCHPAD},
            {SDL_GAMEPAD_BUTTON_DPAD_UP,BTN_UP}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN,BTN_DOWN},
            {SDL_GAMEPAD_BUTTON_DPAD_LEFT,BTN_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT,BTN_RIGHT},
        };
        for (size_t i=0;i<sizeof(map)/sizeof(*map);++i) if (SDL_GetGamepadButton(g,map[i].sdl)) d->buttons|=map[i].ps;
        d->left_x=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFTX)); d->left_y=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFTY));
        d->right_x=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_RIGHTX)); d->right_y=axis(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_RIGHTY));
        d->l2=trigger(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)); d->r2=trigger(SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        if (d->l2>30) d->buttons|=BTN_L2;
        if (d->r2>30) d->buttons|=BTN_R2;
        if (SDL_GetNumGamepadTouchpads(g)>0) {
            const int fingers=SDL_GetNumGamepadTouchpadFingers(g,0);
            for (int finger=0;finger<fingers && d->touch_count<2;++finger) {
                bool down=false;
                float x=0, y=0;
                if (SDL_GetGamepadTouchpadFinger(g,0,finger,&down,&x,&y,NULL) && down) {
                    d->touches[d->touch_count++]=(PadTouch){.x=touch_axis(x,1919),
                        .y=touch_axis(y,942),.id=(uint8_t)finger};
                }
            }
        }
        // Back/Select on pads without a touch surface is a left-side click.
        if ((d->buttons & BTN_TOUCHPAD) && !d->touch_count) touch_click(d,0);
    }
    apply_keyboard(d, k);
    if (runtime_menu_context()) {
        mouse_dx=mouse_dy=0;
        mouse_buttons=0;
        runtime_mouse_camera_disable();
    } else apply_mouse(d);
}

/* BB_PAD_FILE=<file>: scripted input for automated runs. The file holds whitespace-separated
 * tokens, re-read when it changes: button names (cross circle square triangle l1 r1 l2 r2 l3 r3
 * options touchpad touchpad_left touchpad_right up down left right) are held while listed;
 * touchpad defaults to a left-side click; lx= ly= rx= ry= (0..255) override
 * the sticks. An empty file releases everything. */
static struct { uint32_t buttons; int stick[4]; int touch_side; } injected={0,{-1,-1,-1,-1},-1};
static int replay_armed;      /* 1 while a BB_PAD_REPLAY recording plays, 2 once it ended */
static uint64_t replay_start; /* 0: (re)start at the next sample */
static void read_inject(void) {
    static const char *path; static int checked; static uint64_t last_check; static struct timespec mtime;
    if (!checked) { path=getenv("BB_PAD_FILE"); checked=1; }
    if (!path || !*path) return;
    uint64_t now=now_us();
    if (now-last_check<20000) return;
    last_check=now;
    struct stat st;
    if (stat(path,&st)!=0) return;
#ifdef _WIN32
    /* Whole-second mtimes: the size tells two edits within a second apart. */
    if (st.st_mtime==mtime.tv_sec && st.st_size==mtime.tv_nsec) return;
    mtime=(struct timespec){st.st_mtime,(long)st.st_size};
#else
    if (st.st_mtim.tv_sec==mtime.tv_sec && st.st_mtim.tv_nsec==mtime.tv_nsec) return;
    mtime=st.st_mtim;
#endif
    FILE *f=fopen(path,"r");
    if (!f) return;
    static const struct { const char *name; uint32_t ps; } names[]={
        {"cross",BTN_CROSS}, {"circle",BTN_CIRCLE}, {"square",BTN_SQUARE}, {"triangle",BTN_TRIANGLE},
        {"l1",BTN_L1}, {"r1",BTN_R1}, {"l2",BTN_L2}, {"r2",BTN_R2}, {"l3",BTN_L3}, {"r3",BTN_R3},
        {"options",BTN_OPTIONS}, {"touchpad",BTN_TOUCHPAD},
        {"up",BTN_UP}, {"down",BTN_DOWN}, {"left",BTN_LEFT}, {"right",BTN_RIGHT},
    };
    static const char *sticks[]={"lx=","ly=","rx=","ry="};
    injected.buttons=0;
    injected.touch_side=-1;
    for (int i=0;i<4;++i) injected.stick[i]=-1;
    char token[64];
    while (fscanf(f,"%63s",token)==1) {
        if (!strcmp(token,"replay") && replay_armed!=1) { replay_armed=1; replay_start=0; } /* BB_PAD_REPLAY */
        if (!strcmp(token,"touchpad_left") || !strcmp(token,"touchpad_right")) {
            injected.buttons|=BTN_TOUCHPAD;
            injected.touch_side=!strcmp(token,"touchpad_right");
        }
        for (size_t i=0;i<sizeof(names)/sizeof(*names);++i) if (!strcmp(token,names[i].name)) injected.buttons|=names[i].ps;
        for (int i=0;i<4;++i) if (!strncmp(token,sticks[i],3)) { int v=atoi(token+3); injected.stick[i]=v<0 ? 0 : v>255 ? 255 : v; }
    }
    fclose(f);
    printf("Runtime: pad file: buttons 0x%x sticks %d %d %d %d\n",injected.buttons,
           injected.stick[0],injected.stick[1],injected.stick[2],injected.stick[3]);
}
/* BB_PAD_RECORD=<file>: F9 starts and stops recording the pad state (gamepad or keyboard) with
 * the time since F9; BB_PAD_REPLAY=<file> plays such a recording back, started by the token
 * "replay" in BB_PAD_FILE (scripted tests repeat a route the player ran once). Lines: ms buttons
 * lx ly rx ry l2 r2, written when the state changes. */
typedef struct { uint32_t ms, buttons; uint8_t axes[4], l2, r2; } PadSample;
static FILE *record_file;
static uint64_t record_start;
static PadSample record_last;
static void record_sample(const PadData *d) {
    static const char *path; static int checked, f9_was_down;
    if (!checked) { path=getenv("BB_PAD_RECORD"); checked=1; }
    if (!path || !*path || !sdl_ready) return;
    const bool *k=SDL_GetKeyboardState(NULL);
    const int f9=k && k[SDL_SCANCODE_F9];
    if (f9 && !f9_was_down) {
        if (record_file) {
            fclose(record_file); record_file=NULL;
            printf("Runtime: pad recording stopped (%s)\n",path);
        } else if ((record_file=fopen(path,"w"))) {
            record_start=now_us();
            memset(&record_last,0xff,sizeof(record_last));
            printf("Runtime: pad recording started (%s, F9 stops)\n",path);
        }
    }
    f9_was_down=f9;
    if (!record_file) return;
    PadSample s={(uint32_t)((now_us()-record_start)/1000),d->buttons,
                 {d->left_x,d->left_y,d->right_x,d->right_y},d->l2,d->r2};
    if (s.buttons==record_last.buttons && !memcmp(s.axes,record_last.axes,4) &&
        s.l2==record_last.l2 && s.r2==record_last.r2) return;
    record_last=s;
    fprintf(record_file,"%u %u %u %u %u %u %u %u\n",s.ms,s.buttons,s.axes[0],s.axes[1],s.axes[2],
            s.axes[3],s.l2,s.r2);
    fflush(record_file);
}
static PadSample *replay; static size_t replay_count, replay_next;
static void replay_sample(PadData *d) {
    if (!replay_armed) return;
    if (!replay_start) {
        static int loaded;
        if (!loaded) {
            loaded=1;
            const char *path=getenv("BB_PAD_REPLAY");
            FILE *f=path ? fopen(path,"r") : NULL;
            PadSample s; unsigned v[8]; size_t cap=0;
            while (f && fscanf(f,"%u %u %u %u %u %u %u %u",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7])==8) {
                s=(PadSample){v[0],v[1],{(uint8_t)v[2],(uint8_t)v[3],(uint8_t)v[4],(uint8_t)v[5]},(uint8_t)v[6],(uint8_t)v[7]};
                if (replay_count==cap && !(replay=realloc(replay,(cap=cap ? cap*2 : 1024)*sizeof(*replay)))) break;
                replay[replay_count++]=s;
            }
            if (f) fclose(f);
            printf("Runtime: pad replay of %zu samples from %s\n",replay_count,path ? path : "(unset)");
        }
        replay_start=now_us();
        replay_next=0;
    }
    const uint32_t ms=(uint32_t)((now_us()-replay_start)/1000);
    while (replay_next<replay_count && replay[replay_next].ms<=ms) ++replay_next;
    if (!replay_next) return;
    if (replay_next==replay_count && ms>replay[replay_count-1].ms+500) {
        if (replay_armed==1) { puts("Runtime: pad replay finished"); replay_armed=2; }
        return;
    }
    const PadSample *s=&replay[replay_next-1];
    d->buttons=s->buttons;
    d->left_x=s->axes[0]; d->left_y=s->axes[1]; d->right_x=s->axes[2]; d->right_y=s->axes[3];
    d->l2=s->l2; d->r2=s->r2;
}
/* After the menu or the text dialog closes, buttons still held (the Cross that accepted a
 * name) stay hidden until released: the game would take them as a new press. */
static int hold_after_capture;
static void sample(PadData *d) {
    sample_host(d);
    if (bbgpu_overlay_captures_input()) { hold_after_capture=1; return; }
    record_sample(d);
    read_inject();
    replay_sample(d);
    d->buttons|=injected.buttons;
    if (injected.touch_side>=0) touch_click(d,injected.touch_side);
    else if ((d->buttons & BTN_TOUCHPAD) && !d->touch_count) touch_click(d,0);
    if (injected.buttons & BTN_L2) d->l2=255;
    if (injected.buttons & BTN_R2) d->r2=255;
    uint8_t *axes[4]={&d->left_x,&d->left_y,&d->right_x,&d->right_y};
    for (int i=0;i<4;++i) if (injected.stick[i]>=0) *axes[i]=(uint8_t)injected.stick[i];
    if (hold_after_capture) {
        if (d->buttons) d->buttons=0;
        else hold_after_capture=0;
    }
}

static ABI int32_t pad_init(void) { pthread_mutex_lock(&lock); initialized=1; pthread_mutex_unlock(&lock); return 0; }
static ABI int32_t pad_open(int32_t user, int32_t type, int32_t index, const void *param) {
    (void)param;
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (user!=1) return ERR_INVALID_ARG;
    if (type!=0 && type!=2) return ERR_INVALID_ARG; /* standard / special port */
    if (index) return ERR_INVALID_ARG;
    pthread_mutex_lock(&lock);
    int already=opened; opened=1;
    pthread_mutex_unlock(&lock);
    if (already) return ERR_ALREADY_OPENED;
    puts("Runtime: pad opened for user 1 (SDL gamepad or keyboard)");
    return PAD_HANDLE;
}
static ABI int32_t pad_close(int32_t handle) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    opened=0; return 0;
}
static ABI int32_t pad_read_state(int32_t handle, PadData *data) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!data) return ERR_INVALID_ARG;
    pthread_mutex_lock(&lock);
    sample(data); ++reads;
    pthread_mutex_unlock(&lock);
    return 0;
}
/* Buffered read: the port samples once per call, so one entry is returned. */
static ABI int32_t pad_read(int32_t handle, PadData *data, int32_t count) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!data || count<1 || count>64) return ERR_INVALID_ARG;
    pad_read_state(handle,data);
    return 1;
}
static ABI int32_t pad_info(int32_t handle, ControllerInfo *info) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!info) return ERR_INVALID_ARG;
    memset(info,0,sizeof(*info));
    info->pixel_density=44.86f; info->resolution_x=1920; info->resolution_y=943;
    info->dead_zone_left=info->dead_zone_right=2;
    info->connection_type=0; info->connected=1; info->device_class=0;
    pthread_mutex_lock(&lock);
    current_gamepad();
    info->connected_count=connected_count ? connected_count : 1;
    pthread_mutex_unlock(&lock);
    return 0;
}
static ABI int32_t pad_vibration(int32_t handle, const uint8_t *param) {
    if (handle!=PAD_HANDLE || !opened) return ERR_INVALID_HANDLE;
    if (!param) return ERR_INVALID_ARG;
    pthread_mutex_lock(&lock);
    SDL_Gamepad *g=current_gamepad();
    if (g) SDL_RumbleGamepad(g,(uint16_t)(param[0]*257),(uint16_t)(param[1]*257),1000);
    pthread_mutex_unlock(&lock);
    return 0;
}
static ABI int32_t pad_ok_handle(int32_t handle) { return handle==PAD_HANDLE && opened ? 0 : ERR_INVALID_HANDLE; }
static ABI int32_t pad_ok_handle_flag(int32_t handle, uint8_t flag) { (void)flag; return pad_ok_handle(handle); }

static const RuntimeExport exports[]={
    {"scePadInit",pad_init}, {"scePadOpen",pad_open}, {"scePadClose",pad_close},
    {"scePadReadState",pad_read_state}, {"scePadRead",pad_read},
    {"scePadGetControllerInformation",pad_info}, {"scePadSetVibration",pad_vibration},
    {"scePadResetOrientation",pad_ok_handle},
    {"scePadSetAngularVelocityDeadbandState",pad_ok_handle_flag}, {"scePadSetTiltCorrectionState",pad_ok_handle_flag},
    {"scePadSetMotionSensorState",pad_ok_handle_flag},
};
uintptr_t runtime_pad_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_pad_report(void) { printf("Runtime: pad reads=%zu, gamepad=%s\n",reads,gamepad ? SDL_GetGamepadName(gamepad) : "none"); }
