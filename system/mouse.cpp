extern "C" {
    #include <stdint.h>
    #include "kernel.h"
    
    extern int mouse_x;
    extern int mouse_y;
    extern volatile int has_mouse_event;
}

extern "C" void mouse_handler_incoming_packet(int8_t delta_x, int8_t delta_y) {
    mouse_x += delta_x;
    mouse_y += delta_y;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= 1024) mouse_x = 1023;
    if (mouse_y >= 768) mouse_y = 767;

    has_mouse_event = 1;
}
