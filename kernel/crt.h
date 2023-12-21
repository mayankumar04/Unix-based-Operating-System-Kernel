#ifndef _crt_h_
#define _crt_h_

extern "C" void __cxa_atexit();

struct CRT {
    static void init();
    static void fini();
};

#endif
