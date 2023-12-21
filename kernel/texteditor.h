#include "stdint.h"
#include "atomic.h"
#include "vector.h"

namespace TextUI {

    enum : uint16_t {
        WHITE = 0x02,
        BLUE = 0x03,
        PINK = 0x4,
        YELLOW = 0x5,
        GREEN = 0x6
    };

    class Render {
        struct letter {
            char c;
            uint16_t color;
            letter(char c, uint16_t color) : c(c), color(color) {}
        };

        uint16_t *display;
        char forecolor, backcolor;
        int x, y;
        vector<vector<letter *> *> *buffer;
        bool first_line;
        public:
            Render();
            virtual ~Render();
            virtual void fill_background(uint32_t size, uint32_t color);
            virtual uint16_t *write_character(char c, uint16_t color);
            virtual void set_forecolor(uint16_t color);
            virtual void set_backcolor(uint16_t color);
            virtual void increment_x();
            virtual void decrement_x();
            virtual void increment_y();
            virtual void decrement_y();
            virtual void handle_input(char c);
            virtual void handle_backspace();
            virtual void handle_enter();
            virtual void handle_tab();
            virtual void handle_arrow_left();
            virtual void handle_arrow_right();
            virtual void handle_arrow_up();
            virtual void handle_arrow_down();
            virtual void insert_line();
            virtual void delete_line();
            virtual void print_buffer();
            virtual void update_cursor();
            virtual void handle_color();
            virtual void print_message(char *buffer);
            virtual void erase_line(int y, uint16_t color);
            virtual void update_page();
            // virtual void handle_page_down();
    };

    extern void init();
    extern Render* render;
    
};