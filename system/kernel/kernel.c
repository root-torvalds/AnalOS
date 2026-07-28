#include "../include/kernel.h"

volatile int has_keyboard_event = 0;
volatile int has_mouse_event = 0;
volatile uint8_t last_scancode = 0;

extern int mouse_x;
extern int mouse_y;

void draw_mouse (int x, int y, int a);
void init_screen_driver(BootInfo* info);
void fill_screen(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_taskbar(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int rad, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void swap_buffers(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop);

void init_idt(void);
void init_mouse(void);

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" :: "a"(data), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" :: "a"(data), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %0, %1" :: "a"(data), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

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

void __attribute__((ms_abi)) kernel_main(BootInfo* info) {
    if (!info) {
        while(1) { __asm__ __volatile__("hlt"); }
    }

    init_screen_driver(info);
    init_idt();
    init_ioapic();
    init_mouse();

    fill_screen(20, 30, 50, 255);
    draw_taskbar(50, 700, 923, 40, 8, 255, 255, 255, 200);

    swap_buffers(0);

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
        }
    }
}
