/* Windows regression: the menu observer reads only, and refuses other builds. */
#include <assert.h>
#include <stdlib.h>
#include "../src/runtime_menu.c"
__thread sigjmp_buf *runtime_fault_recover;
/* All test addresses are valid. Exception recovery is not tested here. */
int bb_setjmp(bb_jmp_buf buffer) { (void)buffer; return 0; }
int main(void) {
#ifdef _WIN32
    _putenv_s("BB_MENU_TRACE", "1");
#else
    setenv("BB_MENU_TRACE", "1", 1);
#endif
    GfxSnapshot screen={0};
    screen.manager=1; screen.tables[0]=0x539c110; screen.counts[0]=6;
    screen.tables[1]=0x539c980;
    assert(classify(&screen)==2);
    menu_context=2;
    assert(runtime_menu_context()==0); /* Unverified candidates cannot drive input. */
    screen.tables[1]=0x539c680; screen.counts[1]=1; screen.tables[2]=0x5399fb0;
    assert(classify(&screen)==0);
    screen.counts[1]=2; screen.tables[2]=0x539bcf0;
    assert(classify(&screen)==1);
    screen.counts[1]=1000; assert(classify(&screen)==0);
    screen.tables[1]=0x539c460; assert(classify(&screen)==0);
    unsigned char tiny[64]={0};
    runtime_menu_bind(tiny,sizeof(tiny));
    runtime_menu_observe();
    assert(!menu_image && !ready);
    size_t size=GFX_MANAGER_RVA+sizeof(uintptr_t);
    unsigned char *image=calloc(1,size);
    assert(image);
    unsigned char object[MENU_SAMPLE_SIZE]={0}, before[MENU_SAMPLE_SIZE];
    uintptr_t manager=(uintptr_t)object;
    memcpy(image+MENU_GATE_RVA,gate_signature,sizeof(gate_signature));
    memcpy(image+GFX_CONSTRUCTOR_RVA,gfx_signature,sizeof(gfx_signature));
    memcpy(image+MENU_MANAGER_RVA,&manager,sizeof(manager));
    object[0x1730]=1;
    memcpy(before,object,sizeof(before));
    runtime_menu_bind(image,size);
    runtime_menu_observe();
    assert(ready && have_previous && previous_manager==manager && previous[4]==1);
    assert(!memcmp(object,before,sizeof(object)));
    object[0x1730]=0; last_sample_ms=0;
    runtime_menu_observe();
    assert(previous[4]==0);
    memset(image+MENU_MANAGER_RVA,0,sizeof(manager)); last_sample_ms=0;
    runtime_menu_observe();
    assert(!previous_manager);
    image[MENU_GATE_RVA]^=1;
    runtime_menu_bind(image,size);
    runtime_menu_observe();
    assert(!menu_image && !ready);
    image[MENU_GATE_RVA]^=1;
    uintptr_t root=(uintptr_t)object, table=(uintptr_t)image+0x539c110;
    memcpy(image+GFX_MANAGER_RVA,&root,sizeof(root));
    memcpy(object,&table,sizeof(table));
    uint64_t count=1;
    memcpy(object+0x98,&count,sizeof(count));
    unsigned char child[0xa0]={0};
    uintptr_t child_address=(uintptr_t)child, child_table=(uintptr_t)image+0x5300000;
    memcpy(object+0x50,&child_address,sizeof(child_address));
    memcpy(child,&child_table,sizeof(child_table));
    memcpy(before,object,sizeof(before));
    runtime_menu_bind(image,size);
    runtime_menu_observe();
    assert(have_gfx && previous_gfx.tables[0]==0x539c110);
    assert(previous_gfx.counts[0]==1 && previous_gfx.tables[1]==0x5300000);
    assert(!memcmp(object,before,sizeof(object)));
    count=1000; memcpy(object+0x98,&count,sizeof(count)); last_sample_ms=0;
    runtime_menu_observe();
    assert(previous_gfx.counts[0]==1000 && !previous_gfx.tables[1]);
    image[GFX_CONSTRUCTOR_RVA]^=1;
    runtime_menu_bind(image,size); runtime_menu_observe();
    assert(!menu_image && !ready);
    free(image);
    puts("PASS: version guard, native flag changes, absent manager, read-only observation");
}
