#include "efi.h"
#include "../include/kernel.h"

UINT32* real_framebuffer = 0;

EFI_GRAPHICS_OUTPUT_BLT_PIXEL* virtual_framebuffer = 0;

void init_screen_driver(BootInfo* info) {
    if (info) {
        real_framebuffer = (UINT32*)info->FrameBufferBase;
        virtual_framebuffer = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)info->VirtualFrameBuffer;
    }
}

EFIAPI EFI_GRAPHICS_OUTPUT_BLT_PIXEL get_pixel(UINT32 x, UINT32 y) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL empty_pixel = {0, 0, 0, 0};

    if (!virtual_framebuffer || x >= 1024 || y >= 768) {
        return empty_pixel;
    }

    UINT32 index = y * 1024 + x;

    return virtual_framebuffer[index];
}


EFIAPI void draw_pixel(UINT32 x, UINT32 y, UINT8 r, UINT8 g, UINT8 b, UINT8 alpha) {
    if (!virtual_framebuffer || x >= 1024 || y >= 768 || alpha == 0) return;

    UINT32 index = y * 1024 + x;
    if (alpha == 255) {
        virtual_framebuffer[index].Red = r;
        virtual_framebuffer[index].Green = g;
        virtual_framebuffer[index].Blue = b;
        return;
    }

    UINT8 bg_r = virtual_framebuffer[index].Red;
    UINT8 bg_g = virtual_framebuffer[index].Green;
    UINT8 bg_b = virtual_framebuffer[index].Blue;

    virtual_framebuffer[index].Red   = ((r * alpha) + (bg_r * (255 - alpha))) >> 8;
    virtual_framebuffer[index].Green = ((g * alpha) + (bg_g * (255 - alpha))) >> 8;
    virtual_framebuffer[index].Blue  = ((b * alpha) + (bg_b * (255 - alpha))) >> 8;
}

EFIAPI void draw_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT8 r, UINT8 g, UINT8 b, UINT8 alpha) {
    if (!virtual_framebuffer || x >= 1024 || y >= 768 || w == 0 || h == 0 || alpha == 0) return;
    if (x + w > 1024) w = 1024 - x;
    if (y + h > 768) h = 768 - y;

    for (UINT32 i = 0; i < h; i++) {
        for (UINT32 j = 0; j < w; j++) {
            UINT32 index = (y + i) * 1024 + (x + j);

            if (alpha == 255) {
                virtual_framebuffer[index].Red   = r;
                virtual_framebuffer[index].Green = g;
                virtual_framebuffer[index].Blue  = b;
            } else {
                UINT8 bg_r = virtual_framebuffer[index].Red;
                UINT8 bg_g = virtual_framebuffer[index].Green;
                UINT8 bg_b = virtual_framebuffer[index].Blue;

                virtual_framebuffer[index].Red   = ((r * alpha) + (bg_r * (255 - alpha))) >> 8;
                virtual_framebuffer[index].Green = ((g * alpha) + (bg_g * (255 - alpha))) >> 8;
                virtual_framebuffer[index].Blue  = ((b * alpha) + (bg_b * (255 - alpha))) >> 8;
            }
        }
    }
}

EFIAPI void fill_screen(UINT8 r, UINT8 g, UINT8 b, UINT8 a) {
    draw_rect(0, 0, 1024, 768, r, g, b, a);
}

EFIAPI void draw_ui_element(UINT32 w, UINT32 h, int anchor, UINT8 r, UINT8 g, UINT8 b, UINT8 a) {
    UINT32 final_x = 0;
    UINT32 final_y = 0;

    if (anchor == 0) {
        final_x = (1024 - w) / 2;
        final_y = (768 - h) / 2;
    }
    else if (anchor == 2) {
        final_x = 0;
        final_y = 768 - h;
        w = 1024;
    }

    draw_rect(final_x, final_y, w, h, r, g, b, a);
}

EFIAPI void draw_square(UINT32 size, UINT8 r, UINT8 g, UINT8 b, UINT8 a) {
    draw_ui_element(size, size, 0, r, g, b, a);
}

EFIAPI void draw_alpha_test_bar(UINT32 start_x, UINT32 start_y, UINT32 height) {
    for (UINT32 a = 0; a < 256; a++) {
        UINT32 current_x = start_x + a;
        for (UINT32 current_y = start_y; current_y < (start_y + height); current_y++) {
            draw_pixel(current_x, current_y, 0, 255, 0, (UINT8)a);
        }
    }
}

EFIAPI void draw_circle(UINT32 center_x, UINT32 center_y, UINT32 rad, UINT8 r, UINT8 g, UINT8 b, UINT8 a) {
    if (rad == 0) return;

    UINT32 start_y = (center_y >= rad) ? (center_y - rad) : 0;
    UINT32 end_y   = center_y + rad;
    if (end_y >= 768) end_y = 767;

    UINT32 start_x_bound = (center_x >= rad) ? (center_x - rad) : 0;
    UINT32 end_x_bound   = center_x + rad;
    if (end_x_bound >= 1024) end_x_bound = 1023;

    UINT32 rad_scaled = rad * 256;
    UINT32 rad_sq_scaled = rad_scaled * rad_scaled;

    for (UINT32 cur_y = start_y; cur_y <= end_y; cur_y++) {
        UINT32 dy = (cur_y > center_y) ? (cur_y - center_y) : (center_y - cur_y);
        UINT32 dy_scaled = dy * 256;
        UINT32 dy_sq = dy_scaled * dy_scaled;

        for (UINT32 cur_x = start_x_bound; cur_x <= end_x_bound; cur_x++) {
            UINT32 dx = (cur_x > center_x) ? (cur_x - center_x) : (center_x - cur_x);
            UINT32 dx_scaled = dx * 256;

            UINT32 dist_sq = (dx_scaled * dx_scaled) + dy_sq;

            if (dist_sq <= rad_sq_scaled) {
                UINT32 dist = 0;
                while ((dist + 1) * (dist + 1) <= dist_sq) {
                    dist++;
                }

                UINT32 inner_edge = rad_scaled - 256;

                if (dist <= inner_edge) {
                    draw_pixel(cur_x, cur_y, r, g, b, a);
                } else {
                    UINT32 edge_dist = dist - inner_edge;
                    UINT32 alpha_factor = 256 - edge_dist;

                    UINT32 final_alpha = (a * alpha_factor) >> 8;

                    if (final_alpha > 0) {
                        draw_pixel(cur_x, cur_y, r, g, b, (UINT8)final_alpha);
                    }
                }
            }
        }
    }
}

EFIAPI void draw_taskbar(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 rad, UINT8 r, UINT8 g, UINT8 b, UINT8 a) {
    if (w == 0 || h == 0 || a == 0) return;

    if (rad * 2 > w) rad = w / 2;
    if (rad * 2 > h) rad = h / 2;

    if (rad == 0) {
        draw_rect(x, y, w, h, r, g, b, a);
        return;
    }

    draw_rect(x + rad, y, w - (rad * 2), h, r, g, b, a);
    draw_rect(x, y + rad, rad, h - (rad * 2), r, g, b, a);
    draw_rect((x + w) - rad, y + rad, rad, h - (rad * 2), r, g, b, a);

    UINT32 rad_scaled = rad * 256;
    UINT32 rad_sq_scaled = rad_scaled * rad_scaled;
    UINT32 inner_edge = rad_scaled - 256;

    struct Corner {
        UINT32 cx;
        UINT32 cy;
        int dx_sign;
        int dy_sign;
    } corners[4] = {
        { x + rad - 1,     y + rad - 1,     -1, -1 },
        { x + w - rad,     y + rad - 1,      1, -1 },
        { x + rad - 1,     y + h - rad,     -1,  1 },
        { x + w - rad,     y + h - rad,      1,  1 }
    };

    for (int i = 0; i < 4; i++) {
        UINT32 cx = corners[i].cx;
        UINT32 cy = corners[i].cy;

        for (UINT32 h_coord = 0; h_coord < rad; h_coord++) {
            UINT32 dy = h_coord;
            UINT32 dy_scaled = dy * 256;
            UINT32 dy_sq = dy_scaled * dy_scaled;

            UINT32 screen_y = (corners[i].dy_sign == -1) ? (cy - dy) : (cy + dy);

            for (UINT32 w_coord = 0; w_coord < rad; w_coord++) {
                UINT32 dx = w_coord;
                UINT32 dx_scaled = dx * 256;
                UINT32 dist_sq = (dx_scaled * dx_scaled) + dy_sq;

                if (dist_sq > rad_sq_scaled) continue;

                UINT32 screen_x = (corners[i].dx_sign == -1) ? (cx - dx) : (cx + dx);

                UINT32 dist = 0;
                while ((dist + 1) * (dist + 1) <= dist_sq) { dist++; }

                if (dist <= inner_edge) {
                    draw_pixel(screen_x, screen_y, r, g, b, a);
                } else {
                    UINT32 edge_dist = dist - inner_edge;
                    UINT32 alpha_factor = 256 - edge_dist;
                    UINT32 final_alpha = (a * alpha_factor) >> 8;

                    draw_pixel(screen_x, screen_y, r, g, b, (UINT8)final_alpha);
                }
            }
        }
    }
}

EFIAPI void swap_buffers(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    (void)gop;
    if (!real_framebuffer || !virtual_framebuffer) return;

    UINT32* src = (UINT32*)virtual_framebuffer;
    for (UINT32 i = 0; i < 1024 * 768; i++) {
        UINT32 color = src[i];
        UINT8 r = (color >> 16) & 0xFF;
        UINT8 g = (color >> 8) & 0xFF;
        UINT8 b = color & 0xFF;

        real_framebuffer[i] = (r << 16) | (g << 8) | b;
    }
}
