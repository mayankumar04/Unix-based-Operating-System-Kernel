#ifndef _U8250_H_
#define _U8250_H_
#include "io.h"
#include "machine.h"

class U8250 : public OutputStream<char> {
public:
    U8250() { }
    virtual void put(char ch);
    virtual char get();
};

#endif