/* Direct mouse camera for Bloodborne 01.09. Uses a validated camera-function
 * hook and the game's camera angle fields; other builds retain stick emulation. */
#define _GNU_SOURCE
#include "runtime.h"
#include <math.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#define CAMERA_HOOK_RVA 0x143CEAAu
#define CAMERA_HOOK_SIZE 18u
#define CAMERA_FUNC_LO 0x224Au
#define CAMERA_FUNC_HI 0x2C26u
#define CAMERA_PITCH 0x140u
#define CAMERA_YAW 0x144u
#define CAMERA_PITCH_COPY 0x150u
#define CAMERA_LOCKON 0x154u
#define NOP_SITE_COUNT 29u

typedef struct { uintptr_t address; uint8_t original[9]; } NopSite;
static uint8_t *game_image;
static size_t game_image_size;
static uint8_t *camera_cave;
static uintptr_t camera_hook;
static uint8_t hook_original[CAMERA_HOOK_SIZE];
static NopSite nop_sites[NOP_SITE_COUNT];
static size_t nop_count;
static _Atomic uintptr_t camera_base;
static uintptr_t last_camera_base;
static _Atomic uint64_t camera_generation;
static uint64_t consumed_generation;
static float pending_dx, pending_dy;
static uint64_t last_generation_ms;
static uint64_t camera_clock_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec*1000u + (uint64_t)now.tv_nsec/1000000u;
#endif
}
static int hook_ready, hook_failed, hook_installed, nops_installed, camera_angles_ready, was_locked;
static float pitch, yaw;

static const uint8_t hook_signature[CAMERA_HOOK_SIZE] = {
    0xC4,0xC1,0x7A,0x10,0x85,0x40,0x01,0x00,0x00,
    0xC4,0xC1,0x7A,0x10,0x8D,0x50,0x01,0x00,0x00
};
static const uint32_t target_displacements[] = {0x130,0x140,0x144,0x150,0x294};
static const uint8_t store_prefix[] = {0xC4,0xC1,0x7A,0x11};
static const uint8_t nop9[] = {0x66,0x0F,0x1F,0x84,0x00,0,0,0,0};

static void emit(uint8_t **p, const void *data, size_t n) { memcpy(*p,data,n); *p+=n; }
static void emit_u64(uint8_t **p, uintptr_t value) { emit(p,&value,sizeof(value)); }
static void *allocate_cave(void) {
#ifdef _WIN32
    return VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
#else
    void *p=mmap(NULL,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    return p==MAP_FAILED ? NULL : p;
#endif
}
static int write_code(void *address, const void *bytes, size_t size) {
#ifdef _WIN32
    DWORD old_protect=0, ignored=0;
    if (!VirtualProtect(address,size,PAGE_EXECUTE_READWRITE,&old_protect)) return 0;
    memcpy(address,bytes,size);
    FlushInstructionCache(GetCurrentProcess(),address,size);
    return VirtualProtect(address,size,old_protect,&ignored)!=0;
#else
    const long page=sysconf(_SC_PAGESIZE);
    uintptr_t begin=(uintptr_t)address & ~((uintptr_t)page-1u);
    uintptr_t end=((uintptr_t)address+size+(uintptr_t)page-1u)&~((uintptr_t)page-1u);
    if (mprotect((void*)begin,end-begin,PROT_READ|PROT_WRITE|PROT_EXEC)) return 0;
    memcpy(address,bytes,size);
    __builtin___clear_cache((char*)address,(char*)address+size);
    return mprotect((void*)begin,end-begin,PROT_READ|PROT_EXEC)==0;
#endif
}
static int patch_jump(uintptr_t site, uintptr_t destination, size_t length) {
    uint8_t patch[CAMERA_HOOK_SIZE];
    if (length<CAMERA_HOOK_SIZE) return 0;
    memset(patch,0x90,length);
    patch[0]=0xFF; patch[1]=0x25;
    memset(patch+2,0,4);
    memcpy(patch+6,&destination,sizeof(destination));
    return write_code((void*)site,patch,length);
}
static int prepare_hook(void) {
    if (hook_ready) return 1;
    if (hook_failed) return 0;
    if (!game_image || game_image_size<CAMERA_HOOK_RVA+CAMERA_HOOK_SIZE) { hook_failed=1; return 0; }
    camera_hook=(uintptr_t)(game_image+CAMERA_HOOK_RVA);
    if (memcmp((void*)camera_hook,hook_signature,sizeof(hook_signature))) { hook_failed=1; return 0; }
    const uintptr_t start=camera_hook-CAMERA_FUNC_LO;
    const size_t span=CAMERA_FUNC_LO+CAMERA_FUNC_HI;
    if (start<(uintptr_t)game_image || start+span>(uintptr_t)game_image+game_image_size) return 0;
    nop_count=0;
    for (size_t i=0;i+9<=span;i++) {
        const uint8_t *p=(const uint8_t*)(start+i);
        if (memcmp(p,store_prefix,sizeof(store_prefix)) || (p[4]&0xC7)!=0x85) continue;
        uint32_t disp; memcpy(&disp,p+5,sizeof(disp));
        int wanted=0;
        for (size_t j=0;j<sizeof(target_displacements)/sizeof(target_displacements[0]);j++)
            if (disp==target_displacements[j]) { wanted=1; break; }
        if (!wanted || ((uintptr_t)p>=camera_hook && (uintptr_t)p<camera_hook+CAMERA_HOOK_SIZE)) continue;
        if (nop_count>=NOP_SITE_COUNT) return 0;
        nop_sites[nop_count].address=(uintptr_t)p;
        memcpy(nop_sites[nop_count].original,p,9);
        nop_count++;
    }
    if (nop_count!=NOP_SITE_COUNT) { hook_failed=1; return 0; }
    memcpy(hook_original,(void*)camera_hook,sizeof(hook_original));
    camera_cave=allocate_cave();
    if (!camera_cave) { hook_failed=1; return 0; }
    uint8_t *p=camera_cave;
    emit(&p,hook_signature,sizeof(hook_signature));
    const uint8_t set_mode[]={0x41,0xC7,0x85,0x30,0x01,0x00,0x00};
    const float mode=0.8f;
    emit(&p,set_mode,sizeof(set_mode)); emit(&p,&mode,sizeof(mode));
    // The displaced instructions do not modify RAX. Preserve it across our store.
    const uint8_t push_rax=0x50; emit(&p,&push_rax,1);
    const uint8_t mov_rax_imm[]={0x48,0xB8}; emit(&p,mov_rax_imm,sizeof(mov_rax_imm));
    emit_u64(&p,(uintptr_t)&camera_base);
    const uint8_t save_r13[]={0x4C,0x89,0x28}; emit(&p,save_r13,sizeof(save_r13));
    // Publish a fresh camera invocation without changing guest flags/registers.
    const uint8_t push_flags=0x9C; emit(&p,&push_flags,1);
    emit(&p,mov_rax_imm,sizeof(mov_rax_imm));
    emit_u64(&p,(uintptr_t)&camera_generation);
    const uint8_t advance_generation[]={0xF0,0x48,0xFF,0x00};
    emit(&p,advance_generation,sizeof(advance_generation));
    const uint8_t pop_flags=0x9D; emit(&p,&pop_flags,1);
    const uint8_t pop_rax=0x58; emit(&p,&pop_rax,1);
    const uint8_t jmp_indirect[]={0xFF,0x25,0,0,0,0}; emit(&p,jmp_indirect,sizeof(jmp_indirect));
    emit_u64(&p,camera_hook+CAMERA_HOOK_SIZE);
#ifdef _WIN32
    DWORD old_protect=0;
    if (!VirtualProtect(camera_cave,4096,PAGE_EXECUTE_READ,&old_protect)) return 0;
    FlushInstructionCache(GetCurrentProcess(),camera_cave,(SIZE_T)(p-camera_cave));
#else
    __builtin___clear_cache((char*)camera_cave,(char*)p);
    if (mprotect(camera_cave,4096,PROT_READ|PROT_EXEC)) return 0;
#endif
    hook_ready=1;
    return 1;
}
static int set_nops(int enabled) {
    if (!!enabled==nops_installed) return 1;
    for (size_t i=0;i<nop_count;i++) {
        const void *bytes=enabled ? nop9 : nop_sites[i].original;
        if (!write_code((void*)nop_sites[i].address,bytes,9)) return 0;
    }
    nops_installed=!!enabled;
    return 1;
}
static int set_hook(int enabled) {
    if (!!enabled==hook_installed) return 1;
    if (enabled) {
        if (!patch_jump(camera_hook,(uintptr_t)camera_cave,CAMERA_HOOK_SIZE)) return 0;
    } else {
        if (!write_code((void*)camera_hook,hook_original,sizeof(hook_original))) return 0;
        hook_installed=0;
        if (!set_nops(0)) return 0;
        camera_base=0; last_camera_base=0; camera_angles_ready=0; was_locked=0;
        consumed_generation=camera_generation; pending_dx=pending_dy=0;
        return 1;
    }
    hook_installed=1;
    puts("Native mouse camera: Bloodborne 01.09 camera hook active");
    return 1;
}

void runtime_mouse_camera_bind(void *image, size_t image_size) {
    game_image=(uint8_t*)image;
    game_image_size=image_size;
}

void runtime_mouse_camera_disable(void) {
    if (hook_installed) set_hook(0);
}

static int camera_range_readable(uintptr_t base) {
    if (!base || base>UINTPTR_MAX-0x160u) return 0;
#ifdef _WIN32
    const uintptr_t fields[]={base+CAMERA_PITCH,base+CAMERA_YAW,
                              base+CAMERA_PITCH_COPY,base+CAMERA_LOCKON};
    for (size_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((const void*)fields[i],&info,sizeof(info)) || info.State!=MEM_COMMIT ||
            (info.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return 0;
        const DWORD protection=info.Protect&0xFFu;
        if (protection==PAGE_EXECUTE || protection==PAGE_READONLY || protection==PAGE_EXECUTE_READ)
            return 0;
        if (fields[i]+sizeof(float)>(uintptr_t)info.BaseAddress+info.RegionSize) return 0;
    }
    return 1;
#else
    return runtime_memory_is_mapped(base+CAMERA_PITCH,sizeof(float)) &&
           runtime_memory_is_mapped(base+CAMERA_YAW,sizeof(float)) &&
           runtime_memory_is_mapped(base+CAMERA_PITCH_COPY,sizeof(float)) &&
           runtime_memory_is_mapped(base+CAMERA_LOCKON,sizeof(float));
#endif
}

int runtime_mouse_camera_update(float dx, float dy, float sensitivity_x, float sensitivity_y,
                               float aspect_scale, int invert_y, int reset_camera,
                               int8_t right_x, int8_t right_y) {
    if (!prepare_hook()) {
        if (hook_failed==1) {
            hook_failed=2;
            puts("Native mouse camera: unsupported game code; falling back to right-stick emulation");
        }
        return 0;
    }
    if (!set_hook(1)) return 0;
    const uint64_t now_ms=camera_clock_ms();
    if (now_ms-last_generation_ms>100u) { pending_dx=pending_dy=0; camera_angles_ready=0; }
    pending_dx+=dx; pending_dy+=dy;
    const uint64_t generation=camera_generation;
    // A mapped address is not an ownership check: loading can recycle the camera
    // allocation. Never keep writing unless the game invoked its camera again.
    if (generation==consumed_generation) {
        if (reset_camera || now_ms-last_generation_ms>100u) set_nops(0);
        return 1;
    }
    consumed_generation=generation;
    last_generation_ms=now_ms;
    uintptr_t base=camera_base;
    if (!base) { pending_dx=pending_dy=0; set_nops(0); return 0; }
    if (!camera_range_readable(base)) {
        camera_base=0;
        camera_angles_ready=0;
        pending_dx=pending_dy=0;
        set_nops(0);
        return 0;
    }
    if (base!=last_camera_base) {
        pending_dx=dx; pending_dy=dy;
        last_camera_base=base;
        camera_angles_ready=0;
        was_locked=0;
    }
    // A loading transition can release a camera after VirtualQuery succeeded.
    // Use the runtime's speculative-memory recovery instead of crashing the process.
    sigjmp_buf recover;
    sigjmp_buf *previous_recover=runtime_fault_recover;
    if (sigsetjmp(recover,1)) {
        runtime_fault_recover=previous_recover;
        camera_base=0; last_camera_base=0; camera_angles_ready=0;
        set_nops(0);
        return 0;
    }
    runtime_fault_recover=&recover;
    float lockon=0.0f;
    memcpy(&lockon,(const void*)(base+CAMERA_LOCKON),sizeof(lockon));
    if (lockon==1.0f || reset_camera) {
        runtime_fault_recover=previous_recover;
        set_nops(0);
        pending_dx=pending_dy=0;
        was_locked=(lockon==1.0f);
        camera_angles_ready=0;
        return 0; /* preserve Bloodborne's native lock-on and camera reset behavior */
    }
    if (was_locked) { camera_angles_ready=0; was_locked=0; }
    if (!set_nops(1)) { runtime_fault_recover=previous_recover; return 0; }
    float current_pitch,current_yaw;
    memcpy(&current_pitch,(const void*)(base+CAMERA_PITCH),sizeof(current_pitch));
    memcpy(&current_yaw,(const void*)(base+CAMERA_YAW),sizeof(current_yaw));
    if (!isfinite(current_pitch) || !isfinite(current_yaw)) {
        runtime_fault_recover=previous_recover;
        camera_angles_ready=0;
        pending_dx=pending_dy=0;
        set_nops(0);
        return 0;
    }
    if (!camera_angles_ready || !isfinite(pitch) || !isfinite(yaw)) {
        pitch=current_pitch; yaw=current_yaw; camera_angles_ready=1;
    }
    dx=pending_dx; dy=pending_dy; pending_dx=pending_dy=0;
    if (invert_y) dy=-dy;
    pitch+=dy*0.001f*sensitivity_y;
    yaw+=dx*0.001f*sensitivity_x*aspect_scale;
    const float pitch_min=-40.0f*0.01745329252f, pitch_max=70.0f*0.01745329252f;
    if (pitch<pitch_min) pitch=pitch_min;
    if (pitch>pitch_max) pitch=pitch_max;
    /* Keep right-stick camera operation available while native mouse is enabled. */
    if (right_x || right_y) {
        yaw+=(float)right_x*(0.04f/127.0f);
        pitch+=(float)right_y*(0.04f/127.0f);
    }
    pitch=fmaxf(pitch_min,fminf(pitch,pitch_max));
    yaw=remainderf(yaw,6.28318530718f);
    memcpy((void*)(base+CAMERA_PITCH),&pitch,sizeof(pitch));
    memcpy((void*)(base+CAMERA_YAW),&yaw,sizeof(yaw));
    memcpy((void*)(base+CAMERA_PITCH_COPY),&pitch,sizeof(pitch));
    runtime_fault_recover=previous_recover;
    return 1;
}
