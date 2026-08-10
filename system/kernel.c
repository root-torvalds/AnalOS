#include "kernel.h"

volatile int has_keyboard_event = 0;
volatile int has_mouse_event = 0;
volatile uint8_t last_scancode = 0;
#ifdef __cplusplus
extern "C" {
#endif
    void screen_switch_to_virtio(void);
    int  init_virtio_gpu(void);
#ifdef __cplusplus
}
#endif

extern int mouse_x;
extern int mouse_y;

void draw_mouse (int x, int y, int a);
void init_screen_driver(BootInfo* info);
void fill_screen(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_taskbar(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int rad, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void swap_buffers(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop);

void init_idt(void);
void init_mouse(void);

void sys_reset(void) {
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    while(1) { __asm__ volatile("hlt"); }
}

void sys_shutdown(void) {
    outw(0xB2, 0x07);
    io_wait();
    outw(0x604, 0x2000);
    io_wait();

    outl(0xCF8, 0x8000F840);
    io_wait();
    uint32_t pmbase = inl(0xCFC) & 0xFFFE;
    io_wait();

    if (pmbase != 0) {
        uint16_t pm1_cnt_port = (uint16_t)(pmbase + 0x04);
        outw(pm1_cnt_port, 0x2000 | 0x1C00);
        io_wait();
    }

    __asm__ __volatile__("cli; hlt");
}

#include "virtio_gpu_cmd.hpp"

#ifdef __cplusplus
extern "C" {
#endif
    void screen_switch_to_virtio(void);
    int  init_virtio_gpu(void);
#ifdef __cplusplus
}
#endif

void __attribute__((ms_abi)) kernel_main(BootInfo* info) {
    if (!info) {
        while(1) { __asm__ __volatile__("hlt"); }
    }

    // Первичная отрисовка в UEFI буфер
    init_screen_driver(info);
    fill_screen(0, 0, 0, 255);
    draw_icon(335, 115);
    swap_buffers(0);
    
    init_idt();
    init_ioapic();
    init_mouse();
    init_ahci();
    init_ext2();
    
    draw_wallpaper(0, 0);
    draw_taskbar(50, 700, 923, 40, 8, 255, 255, 255, 200);
    draw_ico_folder(30, 30);
    swap_buffers(0);

    // Запуск VirtIO GPU
    int res = init_virtio_gpu();
    if (res == 0) {
        int screen_res = virtio_gpu_setup_screen();
        if (screen_res == 0) {
            // Переключаем virtual_framebuffer на os_framebuffer
            screen_switch_to_virtio();

            // Перерисовываем элементы один раз на старте
            draw_wallpaper(0, 0);
            draw_taskbar(50, 700, 923, 40, 8, 255, 255, 255, 200);
            draw_ico_folder(30, 30);
            printf("VirtIO GPU Display Setup: SUCCESS!", 50, 100, 0, 255, 0, 255);
            
            // Твой родной swap_buffers скопирует virtual_framebuffer в real_framebuffer (который теперь равен os_framebuffer)
            swap_buffers(0);
            // Пинаем QEMU вывести готовый кадр
            virtio_gpu_redraw();
        } else {
            if (screen_res == -10) printf("Display Error: QEMU rejected 2D Canvas!", 100, 130, 255, 0, 0, 255);
            else if (screen_res == -11) printf("Display Error: Host rejected RAM binding!", 100, 130, 255, 0, 0, 255);
            else if (screen_res == -12) printf("Display Error: Scanout binding failed!", 100, 130, 255, 0, 0, 255);
            swap_buffers(0);
        }
    } else {
        printf("VirtIO GPU Transport: FAILED!", 100, 130, 255, 0, 0, 255);
        swap_buffers(0);
    }
    
    ext2_create_dir("/", "hello_world");
    ext2_create_file("/hello_world", "hello_world.txt", 0x1A4, "Hello World from Kernel!", 24);

    for (volatile int i = 0; i < 10000000; i++) {
        __asm__ volatile("nop");
    }

    __asm__ volatile("sti");

    while (1) {
        __asm__ __volatile__("hlt");
        if (has_keyboard_event) {
            has_keyboard_event = 0;
            if (last_scancode == 0x01) {
                sys_reset();
            }
        } else if (has_mouse_event) {
            has_mouse_event = 0;
            
            swap_buffers(0);
            draw_mouse(mouse_x, mouse_y, 255);

            if (os_framebuffer != 0) {
                virtio_gpu_redraw();
            }
        }
    }
}
