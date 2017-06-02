#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#ifndef PAYLOAD_PATH
#include "payload_bin.h"
#endif
#include "hid.h"
#include "brahma.h"

extern u32 __ctru_heap_size;
void __system_initSyscalls(void);
void __system_allocateHeaps(void);
void __system_initArgv(void);
void __appInit(void);
void (*__system_retAddr)(void);
Result __sync_init(void);

void  __libctru_init(void (*retAddr)(void))
{
     __ctru_heap_size = 24*1024*1024;

    // Store the return address
    __system_retAddr = envIsHomebrew() ? retAddr : NULL;

    // Initialize the synchronization subsystem
    __sync_init();

    // Initialize newlib support system calls
    __system_initSyscalls();

    // Allocate application and linear heaps
    __system_allocateHeaps();

    // Build argc/argv if present
    __system_initArgv();
}

void error_show(const char *format, ...) {
    consoleInit(GFX_BOTTOM, NULL);
    
    printf("* BRAHMA2LOADER *\n\n");
    va_list va;
    va_start(va, format);
    vprintf(format, va);
    va_end(va);
    
    wait_any_key();
}

void gfx_cycle() {
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

s32 voodoo_load(const char* path, u32 offset, u32 msize, u32 voodoo) {
    u32 res = 0;
    u32 lorem_ipsum = (voodoo>>4)&0x3;
    u32 boot_delay = ((voodoo>>2)&0x3) * 50;
    u32 magic_fix = voodoo & (1<<1);
    u32 load_svc = voodoo & (1<<0);
    
    // load services
    if (load_svc) {
        srvInit();
        aptInit();
        hidInit();
        acInit();
        ptmuInit();
    }
    
    // magic fix
    if (magic_fix) {
        // offset potential issues caused by homebrew that just ran
        // aptOpenSession();
        APT_SetAppCpuTimeLimit(0);
        // aptCloseSession();
    }
    
    // lorem ipsum console init / exit
    if (lorem_ipsum) {
        u32 s = 0;
        for (s = 0; s < 2; s++) {
            if (!((lorem_ipsum>>s)&0x1)) continue;
            consoleInit( (s==0) ? GFX_BOTTOM : GFX_TOP, NULL );
            printf("Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.");
            consoleClear();
            gfx_cycle();
        }
        gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
        gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
        gfx_cycle();
    }
    
    // bootfix delay
    if (boot_delay) {
        while (aptMainLoop() && boot_delay-- > 0) {
            gfx_cycle();
        }
    }
    
    // actually load the payload, for once
    if (path && path[0] != '\0') {
        res = load_arm9_payload(path, offset, msize);
        if (!res) error_show("[+] payload: %s@%X\n[+] maximum size: %X\n[!] Loading failed!", path, offset, msize);
    }
    #ifndef PAYLOAD_PATH
    else {
        res = load_arm9_payload_from_mem(payload_bin, payload_bin_size);
        if (!res) error_show("[+] payload from mem\n[!] Loading failed!");
    }
    #endif
    
    // unload services
    if (load_svc) {
        ptmuExit();
        acExit();
        hidExit();
        aptExit();
        srvExit();
    }
    
    return res;
}

s32 main (int argc, char **argv) {
    u32 offset = 0;
    u32 msize = 0;
    u32 voodoo = 0;
       
    // Initialize GFX services
    gfxInitDefault();
    gfxSwapBuffers();

    if (brahma_init()) {
        char path[128] = { 0 };
        // Get payload parameters
        #ifdef VOODOO
        voodoo = VOODOO;
        #endif
        #ifndef PAYLOAD_MEM
        #ifdef PAYLOAD_PATH
        strncpy(path, PAYLOAD_PATH, 128);
        #ifdef PAYLOAD_OFFSET
        offset = PAYLOAD_OFFSET;
        #endif
        #ifdef PAYLOAD_MAXSIZE
        msize = PAYLOAD_MAXSIZE;
        #endif
        #else
        if (argc > 1) {
            s32 res = sscanf(argv[1], "%127s %X %X %X", path, &offset, &msize, &voodoo);
            if (res < 4) voodoo = 0;
            if (res < 3) msize = 0;
            if (res < 2) offset = 0;
            if (res < 1) path[0] = '\0';
        }
        #endif
        #endif
        if (voodoo_load(path, offset, msize, voodoo)) {
            gfxExit();
            firm_reboot();
        }
        brahma_exit();
    } else error_show("[!] Not enough memory");

    // Deinitialize GFX
    gfxExit();
    
    return 0;
}
