#include "libk.h"

#include "debug.h"

int K::isdigit(int c) {
    return (c >= '0') && (c <= '9');
}

long K::strlen(const char* str) {
    long n = 0;
    while (*str++ != 0) n++;
    return n;
}

bool K::streq(const char* a, const char* b) {
    int i = 0;

    while (true) {
        char x = a[i];
        char y = b[i];
        if (x != y) return false;
        if (x == 0) return true;
        i++;
    }
}

int K::strcmp(const char* left, const char* right, uint32_t maxlen) {
    for (uint32_t i = 0; i < maxlen; i++) {
        char l = left[i];
        char r = right[i];
        if (l != r) return left[i] - right[i];
        if (l == 0) return 0;
    }
    return 0;
}

void K::strcpy(char* dest, const char* src) {
    while (*src != 0) {
        *dest++ = *src++;
    }
    *dest = 0;
}

extern "C" void __cxa_pure_virtual() {
    Debug::panic("__cxa_pure_virtual called\n");
}
