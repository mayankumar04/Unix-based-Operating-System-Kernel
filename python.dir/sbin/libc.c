#include "libc.h"

int putchar(int c)
{
    char t = (char)c;
    return write(1, &t, 1);
}

int print(const char *p)
{
    char c;
    int count = 0;
    while ((c = *p++) != 0)
    {
        int n = putchar(c);
        if (n < 0)
            return n;
        count++;
    }
    return count + 1;
}

void intToStr(int x, char *buffer)
{
    if (x < 0)
    {
        *buffer++ = '-';
        x *= -1;
    }
    else if (x == 0)
    {
        *buffer++ = '0';
        *buffer++ = '\0';
        return;
    }
    int l = 0;
    int temp = x;
    for (; temp != 0; l++)
    {
        temp /= 10;
    }
    buffer += l;
    *buffer-- = '\0';
    for (; x != 0; x /= 10)
    {
        *buffer-- = (x % 10) + '0';
    }
}

void uintToStr(unsigned int x, char *buffer)
{
    if (x == 0)
    {
        *buffer++ = '0';
        *buffer++ = '\0';
        return;
    }
    int l = 0;
    int temp = x;
    for (; temp != 0; l++)
    {
        temp /= 10;
    }
    buffer += l;
    *buffer-- = '\0';
    for (; x != 0; x /= 10)
    {
        *buffer-- = (x % 10) + '0';
    }
}

void printInt(int x)
{
    char str[12];
    intToStr(x, str);
    print(str);
}

void printUint(unsigned int x)
{
    char str[11];
    uintToStr(x, str);
    print(str);
}

void panic(const char *p) {
    print(p);
    shutdown();
}