extern "C" {
    #include <stdint.h>
    #include "../include/kernel.h"

    extern EFI_GRAPHICS_OUTPUT_BLT_PIXEL* virtual_framebuffer;
    extern UINT32* real_framebuffer;

    EFIAPI EFI_GRAPHICS_OUTPUT_BLT_PIXEL get_pixel(UINT32 x, UINT32 y);
}

extern "C" {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL mouse_bg[16][16] {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
    };
}

extern "C" void save_mouse_bg (int mouse_last_x, int mouse_last_y) {
    for (int i = 0; i < 16; i++) {
        for (int o = 0; o < 16; o++) {
            mouse_bg[o][i] = get_pixel(mouse_last_x + o, mouse_last_y + i);
        }
    }
}

extern "C" void undraw_mouse (int last_x, int last_y) {
    for (int i = 0; i < 16; i++) {
        for (int o = 0; o < 16; o++) {
            EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel = mouse_bg[i][o];
            draw_pixel(last_x + o, last_y + i, pixel.Red, pixel.Green, pixel.Blue, pixel.Reserved);
        }
    }
}

extern "C" void draw_mouse (int x, int y, int a) {
    static const char cursor16[16][16] = {
     {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0},
     {1,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0},
     {1,2,1,0,1,2,2,1,0,0,0,0,0,0,0,0},
     {1,1,0,0,1,2,2,1,0,0,0,0,0,0,0,0},
     {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0},
     {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0},
     {0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0}
    };
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL* backup_buffer = virtual_framebuffer;
    virtual_framebuffer = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)real_framebuffer;

    for (int y_offset = 0; y_offset < 16; y_offset++) {
        for (int x_offset = 0; x_offset < 16; x_offset++) {
            if (cursor16[y_offset][x_offset] == 1) {
                draw_pixel(x + x_offset, y + y_offset, 0, 0, 0, a);
            } else if (cursor16[y_offset][x_offset] == 2) {
                draw_pixel(x + x_offset, y + y_offset, 255, 255, 255, a);
            }
        }
    }
    virtual_framebuffer = backup_buffer;
}
