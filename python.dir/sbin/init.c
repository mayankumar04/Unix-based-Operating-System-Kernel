#include "libc.h"

int main(int argc, char **argv) {
    write(1, "Hello, World!\n", 14);
    execl("/mypy/python32", 0);

    shutdown();
    return 0;
}
