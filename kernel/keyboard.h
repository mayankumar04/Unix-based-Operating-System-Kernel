#ifndef KEYBOARD_H
#define KEYBOARD_H
// Function declaations
#include "u8250.h"

void keyboard_interrupt_handler();
void keyboard_init();
char getChar();
char getKey();
// extern volatile bool keyPressed;
#endif // KEYBOARD_H
