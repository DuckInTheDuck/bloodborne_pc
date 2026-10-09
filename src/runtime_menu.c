/* Read-only native menu reconnaissance for Bloodborne 01.09.
 * This records candidates; it deliberately does not classify them as menu
 * focus or alter pad input until their meaning is verified in gameplay. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MENU_GATE_RVA 0x17621D5u
#define MENU_MANAGER_RVA 0x5562878u
#define MENU_SAMPLE_SIZE 0x1731u
#define GFX_MANAGER_RVA 0x5580E38u
#define GFX_CONSTRUCTOR_RVA 0x1BAF88Du

static const unsigned char gate_signature[] = {
    0x48,0x8B,0x05,0x9C,0x06,0xE0,0x03,
    0x80,0xB8,0x30,0x17,0x00,0x00,0x00
};
static const size_t field_offsets[] = {0x184,0x198,0x254,0x276,0x1730};
static unsigned char *menu_image;
static size_t menu_image_size;
static int ready, reported, have_previous;
static uintptr_t previous_manager;
static unsigned char previous[sizeof(field_offsets)/sizeof(field_offsets[0])];
static uint64_t last_sample_ms;
static const unsigned char gfx_signature[] = {0x48,0x89,0xD8,0x48,0x89,0x05,0xA1,0x15,0x9D,0x03};
typedef struct {
    uintptr_t manager, modal;
    uint64_t counts[6];
    uintptr_t tables[6];
    uint32_t blocked;
} GfxSnapshot;
static GfxSnapshot previous_gfx;
static int have_gfx;
static int menu_context;

static int classify(const GfxSnapshot *s) {
    if (!s->manager || s->tables[0]!=0x539C110u || !s->counts[0] || s->counts[0]>9) return 0;
    if (s->tables[1]==0x539C980u) return 2;
    if (s->tables[1]==0x539C680u && s->counts[1]>=2 && s->counts[1]<=9 && s->tables[2]) return 1;
    return 0;
}

/* Screen identity candidates are diagnostic only: the user's gameplay test
 * disproved their input-focus meaning. Never switch controls on these values. */
int runtime_menu_context(void) { return 0; }

void runtime_menu_bind(void *image, size_t size) {
    menu_image=image;
    menu_image_size=size;
    ready=reported=have_previous=have_gfx=0;
    last_sample_ms=0;
    menu_context=0;
}

static uint64_t menu_clock_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
#endif
}

static int readable(uintptr_t address, size_t size) {
    if (!address || size>UINTPTR_MAX-address) return 0;
#ifdef _WIN32
    const uintptr_t end=address+size;
    while (address<end) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((void*)address,&info,sizeof(info)) ||
            info.State!=MEM_COMMIT || (info.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return 0;
        const uintptr_t next=(uintptr_t)info.BaseAddress+info.RegionSize;
        if (next<=address) return 0;
        address=next;
    }
    return 1;
#else
    return runtime_memory_is_mapped(address,size);
#endif
}

void runtime_menu_observe(void) {
    const char *trace=getenv("BB_MENU_TRACE");
    if (!trace || trace[0]!='1') return;
    if (!menu_image) return;
    if (!ready) {
        if (menu_image_size<GFX_MANAGER_RVA+sizeof(uintptr_t) ||
            memcmp(menu_image+GFX_CONSTRUCTOR_RVA,gfx_signature,sizeof(gfx_signature)) ||
            menu_image_size<MENU_MANAGER_RVA+sizeof(uintptr_t) ||
            menu_image_size<MENU_GATE_RVA+sizeof(gate_signature) ||
            memcmp(menu_image+MENU_GATE_RVA,gate_signature,sizeof(gate_signature))) {
            menu_image=NULL;
            puts("Menu trace: unsupported native menu code; disabled");
            return;
        }
        ready=1;
        puts("Menu trace: native GFx screen stack detected (diagnostic only)");
    }
    const uint64_t now=menu_clock_ms();
    if (now-last_sample_ms<5) return;
    last_sample_ms=now;
    uintptr_t manager=0;
    memcpy(&manager,menu_image+MENU_MANAGER_RVA,sizeof(manager));
    unsigned char fields[sizeof(previous)]={0};
    GfxSnapshot gfx={0};
    sigjmp_buf recover;
    sigjmp_buf *prior=runtime_fault_recover;
    if (sigsetjmp(recover,1)) {
        runtime_fault_recover=prior;
        if (!reported) { puts("Menu trace: native data temporarily unavailable"); reported=1; }
        have_previous=have_gfx=0;
        menu_context=0;
        return;
    }
    runtime_fault_recover=&recover;
    if (manager && readable(manager,MENU_SAMPLE_SIZE)) {
        for (size_t i=0;i<sizeof(fields);i++)
            memcpy(fields+i,(void*)(manager+field_offsets[i]),1);
    } else manager=0;
    if (manager) memcpy(&gfx.blocked,(void*)(manager+0x2e4),sizeof(gfx.blocked));
    memcpy(&gfx.manager,menu_image+GFX_MANAGER_RVA,sizeof(gfx.manager));
    if (gfx.manager && readable(gfx.manager,0xb30)) {
        memcpy(&gfx.modal,(void*)(gfx.manager+0xa78),sizeof(gfx.modal));
        uintptr_t node=gfx.manager;
        for (size_t depth=0;depth<6;depth++) {
            if (!readable(node,0xa0)) break;
            uintptr_t table=0;
            memcpy(&table,(void*)node,sizeof(table));
            if (table<(uintptr_t)menu_image || table-(uintptr_t)menu_image>=menu_image_size) break;
            gfx.tables[depth]=table-(uintptr_t)menu_image;
            uint64_t count=0;
            memcpy(&count,(void*)(node+0x98),sizeof(count));
            gfx.counts[depth]=count;
            /* The native stack reads its top at base+8+(count-1)*8.
             * Never follow a candidate count beyond the inline storage. */
            if (!count || count>9) break;
            uintptr_t next=0;
            memcpy(&next,(void*)(node+0x50+(count-1)*8),sizeof(next));
            if (!next || next==node) break;
            node=next;
        }
    } else gfx.manager=0;
    runtime_fault_recover=prior;
    reported=0;
    const int next_context=classify(&gfx);
    if (next_context!=menu_context) {
        printf("Menu candidate: %d (unverified input focus)\n",next_context);
        menu_context=next_context;
    }
    if (trace && trace[0]=='1' && (!have_previous || manager!=previous_manager || memcmp(fields,previous,sizeof(fields)))) {
        printf("Menu trace: t=%llu manager=%llx f184=%u f198=%u f254=%u f276=%u pad_gate=%u\n",
               (unsigned long long)now,(unsigned long long)manager,
               fields[0],fields[1],fields[2],fields[3],fields[4]);

    }
    if (trace && trace[0]=='1' && (!have_gfx || memcmp(&gfx,&previous_gfx,sizeof(gfx)))) {
        printf("Gfx menu trace: t=%llu manager=%llx modal=%llx blocked=%u stack=",
               (unsigned long long)now,(unsigned long long)gfx.manager,
               (unsigned long long)gfx.modal,gfx.blocked);
        for (size_t depth=0;depth<6 && gfx.tables[depth];depth++)
            printf("%s%llx:%llu",depth ? "/" : "",
                   (unsigned long long)gfx.tables[depth],(unsigned long long)gfx.counts[depth]);
        putchar('\n');

    }
    memcpy(previous,fields,sizeof(fields));
    previous_manager=manager;
    have_previous=1;
    previous_gfx=gfx;
    have_gfx=1;
}
