#include "debug.h"
#include "idt.h"
#include "smp.h"
#include "sys.h"
#include "u8250.h"
#include "stdint.h"

U8250 *uart;
extern TextUI::Render* TextUI::render;

void cpuWriteIoApic(void *ioapicaddr, uint32_t reg, uint32_t value)
{
    uint32_t volatile *ioapic = (uint32_t volatile *)ioapicaddr;
    ioapic[0] = (reg & 0xff);
    ioapic[4] = value;
}

unsigned char kbdus[128] =
        {
                0, 27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 9 */
                '9', '0', '-', '=', '\b',                         /* Backspace */
                '\t',                                             /* Tab */
                'q', 'w', 'e', 'r',                               /* 19 */
                't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     /* Enter key */
                0,                                                /* 29   - Control */
                'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
                '\'', '`', 0,                                     /* Left shift */
                '\\', 'z', 'x', 'c', 'v', 'b', 'n',               /* 49 */
                'm', ',', '.', '/', 0,                            /* Right shift */
                '*',
                0,   /* Alt */
                ' ', /* Space bar */
                0,   /* Caps lock */
                0,   /* 59 - F1 key ... > */
                0, 0, 0, 0, 0, 0, 0, 0,
                0, /* < ... F10 */
                0, /* 69 - Num lock*/
                0, /* Scroll Lock */
                0, /* Home key */
                0, /* Up Arrow */
                0, /* Page Up */
                '-',
                0, /* Left Arrow */
                0,
                0, /* Right Arrow */
                '+',
                0, /* 79 - End key*/
                0, /* Down Arrow */
                0, /* Page Down */
                0, /* Insert Key */
                0, /* Delete Key */
                0, 0, 0,
                0, /* F11 Key */
                0, /* F12 Key */
                0, /* All other keys are undefined */
        };

unsigned char kbdus_shift[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*',    /* 9 */
        '(', ')', '_', '+', '\b',                         /* Backspace */
        '\t',                                             /* Tab */
        'Q', 'W', 'E', 'R',                               /* 19 */
        'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',     /* Enter key */
        0,                                                /* 29   - Control */
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 39 */
        '\"', '~', 0,                                     /* Left shift */
        '|', 'Z', 'X', 'C', 'V', 'B', 'N',                /* 49 */
        'M', '<', '>', '?', 0,                            /* Right shift */
        '*',
        0,   /* Alt */
        ' ', /* Space bar */
        0,   /* Caps lock */
        0,   /* 59 - F1 key ... > */
        0, 0, 0, 0, 0, 0, 0, 0,
        0,   /* < ... F10 */
        0,   /* 69 - Num lock*/
        0,   /* Scroll Lock */
        0,   /* Home key */
        128, /* Up Arrow */
        0,   /* Page Up */
        '-',
        129, /* Left Arrow */
        0,
        130, /* Right Arrow */
        '+',
        0,   /* 79 - End key*/
        131, /* Down Arrow */
        0,   /* Page Down */
        0,   /* Insert Key */
        0,   /* Delete Key */
        0, 0, 0,
        0, /* F11 Key */
        0, /* F12 Key */
        0, /* All other keys are undefined */
};

class CharNode
{
public:
    char data;
    CharNode *next;

    CharNode(char c) : data(c), next(nullptr) {}
};

bool shiftPressed = false;
bool capsLockActive = false; // Track the state of Caps Lock
bool altPressed = false;
bool waiting_for_color = false;
static bool extendedCode = false;

extern "C" void keyboard_interrupt_handler()
{
    unsigned char scancode = inb(0x60); // Read from the keyboard's data buffer

    // Check for extended scan code prefix

    if (extendedCode)
    {
        // Handle extended keys (like arrow keys)
        // I need to work with whatever team needs arrow keys to figure out what should happen in these cases
        // char ascii = kbdus[scancode];
        switch (scancode)
        {
            case 0x48: // Arrow Up
            {
                TextUI::render->handle_arrow_up();
                break;
            }
            case 0x50: // Arrow Down
            {
                TextUI::render->handle_arrow_down();
                break;
            }
            case 0x4B: // Arrow Left
            {
                TextUI::render->handle_arrow_left();
                break;
            }
            case 0x4D: // Arrow Right
            {
                TextUI::render->handle_arrow_right();
                break; 
            }
            default: // Add cases for other extended keys if needed
                break;
        }
        extendedCode = false; // Reset the flag after handling the extended key
    }
    else if (scancode == 0xE0)
    {
        // Set flag if we receive an extended key prefix
        extendedCode = true;
    }
    else
    {
        // Check if the key was just pressed (and not released)
        if (scancode == 0x3A)
        {
            capsLockActive = !capsLockActive;
        }

        if (scancode == 0x38)
        { // alt press
            //Debug::printf("gets to the alt\n");
            altPressed = true;
        }
        else if (scancode == 0xB8)
        { // alt release
            //Debug::printf("relleased\n");
            altPressed = false;
        }

        if (scancode == 0x2A || scancode == 0x36)
        { // Shift key press
            shiftPressed = true;
        }
        else if (scancode == 0xAA || scancode == 0xB6)
        { // Shift key release
            shiftPressed = false;
        }

        if (!(scancode & 0x80))
        {
            //Debug::printf("Active TUI %d\n", UserFileIO::get_active_tui());
            if(altPressed && kbdus[scancode] == 'c')
            {
                TextUI::render->handle_color();
                waiting_for_color = true;
            }
            else if(waiting_for_color)
            {
                TextUI::render->set_forecolor(scancode);
                waiting_for_color = false;
            }
            else
            {
                char ascii;
                if (shiftPressed != capsLockActive)
                { // If either Shift or Caps Lock is active (but not both)
                    ascii = kbdus_shift[scancode];
                }
                else
                {
                    ascii = kbdus[scancode];
                }
                ((TerminalFile *)UserFileIO::terminal->ptr)->bb.put(ascii);
                TextUI::render->handle_input(ascii);
                //Debug::printf("Acitve TUI %d\n", UserFileIO::get_active_tui());
                //((TUIFile *)(UserFileIO::get_active_tui().uf->ptr))->data_bb.put(ascii);
            }
        }
    }
    SMP::eoi_reg.set(0);
    outb(0x20, 0x20);
}
void keyboard_init() {
    uart = new U8250();
    Debug::printf("| Keyboard driver init sequence activated.\n");

    void *ioApicBase = (void *)kConfig.ioAPIC;

    uint32_t irq_id = 1;

    uint32_t lowBits = (33 | (0 << 16));

    uint32_t redirectionEntry = 0x10 + (irq_id * 2);

    cpuWriteIoApic(ioApicBase, redirectionEntry, lowBits);

    IDT::interrupt(33, (uint32_t)keyboard_interrupt_handler_);
}

// char getKey() {
//     CharNode* node = keyQueue.remove();
//     if (node) {
//         char result = node->data;
//         delete node; 
//         return result;
//     }
//     return 0; // Return 0 if no key is available
// }

char getChar()
{
    char c = uart->get();
    //char *buffer = new char[1];
    //buffer[0] = c;
    //UserFileIO::get_active_tui().read(1, (void *) buffer);

    return c;
}