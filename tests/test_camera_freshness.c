/* Windows camera-memory ownership regression; no game code is executed. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/runtime_mouse_camera.c"
__thread sigjmp_buf *runtime_fault_recover;
/* This test uses valid memory only; fault recovery is outside its scope. */
int bb_setjmp(bb_jmp_buf buffer) { (void)buffer; return 0; }
int main(void) {
    unsigned char object[0x300] = {0};
    hook_ready=hook_installed=1;
    camera_base=(uintptr_t)object;
    camera_generation=1;
    assert(runtime_mouse_camera_update(10,0,1,1,1,0,0,0,0)==1);
    float value; memcpy(&value,object+CAMERA_YAW,sizeof(value));
    assert(value>0.009f && value<0.011f);
    unsigned char before[sizeof(object)];
    /* Allocator reused the old address; VirtualQuery still considers it writable. */
    memset(object,0xCD,sizeof(object)); memcpy(before,object,sizeof(object));
    assert(runtime_mouse_camera_update(20,10,1,1,1,0,0,0,0)==1);
    assert(!memcmp(before,object,sizeof(object)));
    assert(runtime_mouse_camera_update(0,0,1,1,1,0,1,0,0)==1);
    assert(!memcmp(before,object,sizeof(object)));
    /* A fresh camera at another address resumes direct input. */
    unsigned char next[0x300] = {0};
    camera_base=(uintptr_t)next; camera_generation=2;
    assert(runtime_mouse_camera_update(10,0,1,1,1,0,0,0,0)==1);
    memcpy(&value,next+CAMERA_YAW,sizeof(value));
    assert(value>0.009f && value<0.011f);
    puts("PASS: fresh camera writes, reused-address protection, reset and new-camera resume");
    return 0;
}
