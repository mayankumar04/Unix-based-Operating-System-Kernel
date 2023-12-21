#include "texteditor.h"
#include "window.h"

namespace TextUI {

    // +++ Render

    Render::Render() {
        x = y = 0;
        display = (uint16_t*) 0xB8000;
        forecolor = 0x0F; 
        backcolor = 0x00;
        buffer = new vector<vector<letter *> *>();
        first_line = true;
    }

    Render::~Render() {}

    void Render::set_backcolor(uint16_t color)
    {
        backcolor = color;
        delete_line();
    }

    void Render::set_forecolor(uint16_t color)
    {
        // char tempcolor = forecolor;
        switch(color)
        {
            case WHITE:
            {
                forecolor = 0x0F;
                break;
            }
            case BLUE:
            {
                forecolor = 0x0B;
                break;
            }
            case PINK:
            {
                forecolor = 0x0D;
                break;
            }
            case YELLOW:
            {
                forecolor = 0x0E;
                break;
            }
            case GREEN:
            {
                forecolor = 0x0A;
                break;
            }
            default:
                break;
        }
        x = 0;
        for(int i = 0; i<80; i++)
        {
            write_character(' ', 0x0F);
            x++;
        }
        x = 0;
        for(int i = 0; i<buffer->get(y)->size(); i++)
        {
            write_character(buffer->get(y)->get(i)->c, buffer->get(y)->get(i)->color);
            x++;
        }    
    }

    void Render::erase_line(int index, uint16_t color)
    {
        int temp = y;
        y = index;
        char tempcolor = forecolor;
        forecolor = color;
        for(int i = 0; i<80; i++)
        {
            write_character(' ', 0x0F);
        }
        y = temp;
        forecolor = tempcolor;
    }

    void Render::handle_color()
    {
        x = 0;
        char *buffer = new char[80];
        char tempcolor = forecolor;
        forecolor = 0x0F;
        memcpy(buffer, "Type to choose a font color! 1: White 2: Blue 3: Pink 4: Yellow 5: Green    ", 72);
        print_message(buffer);
        forecolor = tempcolor;
        delete[] buffer;
    }

    void Render::print_message(char *buffer)
    {
        for(int i = 0; i<80; i++)
        {
            write_character(' ', 0x0F);
            write_character(buffer[i], 0x0F);
            x++;
        }
    }

    void Render::fill_background(uint32_t size, uint32_t color) {
        for (uint32_t i = 0; i < size; i++) 
        {
            display[i] = color; // Creates the window
            write_character(' ', 0x0F);
        }
    }

    uint16_t *Render::write_character(char letter, uint16_t color) {
        uint16_t attribute  = (backcolor << 4) | (color & 0x0F);
        display = (uint16_t *) 0xB8000 + (y * 80 + x);
        *display = letter | (attribute << 8);
        return display;
    }

    void Render::handle_input(char c) {
        if(first_line)
        {
            first_line = false;
            insert_line();
        }

        if(c == 0) return;

        if (c == '\b') {
            handle_backspace();
        }
        else if (c == '\n') {
            y++;
            insert_line();
            x = 4;
            // handle_enter();
        } 
        else if(c == '\t')
        {
            handle_tab();
        }
        else {
            if(x >= 80)
            {
                y++;
                insert_line();
            }
            // print_buffer();
            letter *l = new letter(c, forecolor);
            buffer->get(y)->push_back(l);
            write_character(c, forecolor);
            x++;
        }
    }

    void Render::insert_line()
    {
        int temp_int = y+1;
        vector<letter *> *temp = new vector<letter *>();
        letter *c3 = new letter((char) (temp_int % 10 + 48), 0x0F);
        temp_int /= 10;
        letter *c2 = new letter((char) (temp_int % 10 + 48), 0x0F);
        temp_int /= 10;
        letter *c1 = new letter((char) (temp_int % 10 + 48), 0x0F);
        letter *space = new letter(' ', 0x0F);
        temp->push_back(c1);
        temp->push_back(c2);
        temp->push_back(c3);
        temp->push_back(space);
        buffer->push_back(temp);
        x = 0;
        // char tempcolor = forecolor;
        // forecolor = 0x0F;
        for(int i = 0; i<4; i++)
        {
            write_character(buffer->get(y)->get(i)->c, 0x0F);
            x++;
        }
        // forecolor = tempcolor;
        for(int i = 4; i<80; i++)
        {
            write_character(' ', 0x0F);
            x++;
        }
        x = 4;   
    }

    void Render::delete_line()
    {
        buffer->delete_back();
        x = 4;
        for(int i = 0; i<4; i++)
        {
            x--;
            write_character(' ', 0x0F);
        }
        y--;
        x = buffer->get(y)->size();
    }

    void Render::increment_x() 
    {
        x++;
        if(x >= 80)
        {
            x = 0;
            y++;
        }
    }

    void Render::decrement_x() 
    {
        if(x < 4 && y == 0) return;
        else if(x <= 4)
        {
            y--;
            x = buffer->get(y)->size();
        }
        else 
        {
            x--;
        }
    }

    void Render::increment_y() 
    {
        y++;
        // Debug::printf("y: %d\n", y);
        // if(y==25)
        // {
        //     fill_background(80 * 25, 0x0000);
        //     update_page();
        // }
    }

    void Render::update_page()
    {
        y = 0;
        int y2 = y+1;
        x = 0;
        while(y<25)
        {
            while(x<80)
            {
                if(buffer->size() >= y2)
                {
                    if(buffer->get(y2)->size()>=x)
                    {
                        letter *l = buffer->get(y2)->get(x);
                        write_character(l->c, l->color);
                    }
                }
                else
                {
                    write_character(' ', 0x0F);
                }
                x++;
            }
            y++;
            y2++;
        }
    }

    void Render::decrement_y() 
    {
        if(y == 0) return;
        else
        {
            y--;
        }
    }

    void Render::handle_backspace() 
    {
        if(x <= 4 && y == 0)
        {
            write_character(' ', 0x0F);
            return;
        }
        if(x <= 4){
            delete_line();
        } 
        else
        {
            decrement_x();
            write_character(' ', 0x0F);
            buffer->get(y)->delete_back();
        }
    }

    void Render::handle_enter() 
    {
        if(!first_line)
        {
            increment_y();
        }

        if(y == 25)
        {
            Debug::printf("making it here\n");
            fill_background(80 * 25, 0x0000);
            update_page();
        }
        else
        {
            if(y >= buffer->size())
            {
                insert_line();
            }
            for(int i = 0; i< 4; i++)
            {
                write_character(buffer->get(y)->get(i)->c, 0x0F);
            }
        }

    }

    void Render::print_buffer() {
        for(int i = 0; i < buffer->size(); ++i){
            for(int j = 0; j < buffer->get(i)->size(); ++j){
                Debug::printf("%c ", buffer->get(i)->get(j));
            }
            Debug::printf("\n");
        }
    }

    void Render::handle_tab() {
        for(int i = 0; i<4; ++i)
        {
            write_character(' ', 0x0F);
            increment_x();
        }
    }

    void Render::handle_arrow_down() {
        increment_y();
        // update_cursor(x, y);
    }

    void Render::handle_arrow_up() {
        decrement_y();
        // update_cursor(x, y);
    }

    void Render::handle_arrow_left() {
        decrement_x();
        // update_cursor(x, y);
    }

    void Render::handle_arrow_right() {
        increment_x();
        // update_cursor(x, y);
    }

    void Render::update_cursor()
    {
        // Debug::printf("Updating cursor to %d, %d\n", x, y);
        uint16_t pos = y * 80 + x;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t) (pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
    }

    // +++ TextUI

    Render *render;

    void init() {
        render = new Render();
        render->fill_background(80 * 25, 0x0000); // white background
        char* message = new char[80];
        memcpy(message, "Welcome to the Text Editor! Press any key to continue...", 55);
        render->print_message(message);
    }
    
}