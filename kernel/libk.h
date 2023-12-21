#ifndef _LIBK_H_
#define _LIBK_H_

#include <stdarg.h>

#include "io.h"
#include "stdint.h"

class K {
   public:
    static void snprintf(OutputStream<char>& sink, long maxlen, const char* fmt, ...);
    static void vsnprintf(OutputStream<char>& sink, long maxlen, const char* fmt, va_list arg);
    static int isdigit(int c);
    
    static long strlen(const char* str);
    static bool streq(const char* left, const char* right);
    static int strcmp(const char* left, const char* right, uint32_t maxlen);
    static void strcpy(char* dest, const char* src);

    template <typename T>
    static T min(T v) {
        return v;
    }

    template <typename T, typename... More>
    static T min(T a, More... more) {
        auto rest = min(more...);
        return (a < rest) ? a : rest;
    }
    
    template <typename T>
    static T max(T v) {
        return v;
    }

    template <typename T, typename... More>
    static T max(T a, More... more) {
        auto rest = max(more...);
        return (a > rest) ? a : rest;
    }
};

#endif
