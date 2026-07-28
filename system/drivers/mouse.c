#include <stdint.h>
#include "../include/mouse.h"

// Внешние переменные ядра
extern volatile int has_keyboard_event;
extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

int mouse_last_x = 512;
int mouse_last_y = 384;
int mouse_cycle = 0;
int mouse_x = 512;
int mouse_y = 384;

// Выделяем 4 байта под пакет (защита от IntelliMouse режима)
uint8_t mouse_bytes[4];

extern void draw_mouse(int x, int y, int a);
void save_mouse_bg (int mouse_last_x, int mouse_last_y);
void undraw_mouse (int last_x, int last_y);

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" :: "a"(data), "Nd"(port));
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

void mouse_wait_write() {
    volatile uint32_t timeout = 1000000;
    while ((inb(0x64) & 2) != 0 && --timeout) {
        io_wait();
    }
}

void mouse_wait_read() {
    volatile uint32_t timeout = 1000000;
    while ((inb(0x64) & 1) == 0 && --timeout) {
        io_wait();
    }
}

void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, data);
}

uint8_t mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

void init_mouse() {
    uint8_t init_status = inb(0x64);
    if (init_status == 0xFF) {
        return;
    }

    // Очищаем буфер
    volatile uint32_t clean = 1000;
    while ((inb(0x64) & 1) && --clean) {
        inb(0x60);
        io_wait();
    }

    // 2. Включаем порт мыши
    mouse_wait_write();
    outb(0x64, 0xA8);

    // 3. Читаем Command Byte
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();

    // Переиспользовали или создали одну переменную без конфликтов конфликта
    uint8_t comp_status = inb(0x60);

    // Разрешаем прерывания IRQ 12 и активируем порт
    comp_status |= 0x02;
    comp_status &= ~0x20;

    // Записываем Command Byte обратно
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, comp_status);

    // 4. Настройка самой мыши
    mouse_write(0xF6); // Set Defaults
    mouse_read();      // ACK

    mouse_write(0xF4); // Enable Data Reporting
    mouse_read();      // ACK

    mouse_cycle = 0;

    save_mouse_bg(mouse_x, mouse_y);
}

void mouse_handler_c() {

    uint8_t status = inb(0x64);

    if ((status & 0x01) == 0 || (status & 0x20) == 0) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    uint8_t data = inb(0x60);

    if (mouse_cycle == 0 && !(data & 0x08)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    mouse_bytes[mouse_cycle] = data;
    mouse_cycle++;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        has_mouse_event = 1;

        int32_t move_x = mouse_bytes[1];
        int32_t move_y = mouse_bytes[2];

        if (mouse_bytes[0] & 0x10) {
            move_x |= 0xFFFFFF00;
        }
        if (mouse_bytes[0] & 0x20) {
            move_y |= 0xFFFFFF00;
        }

        if (move_x > 255 || move_x < -255) move_x = 0;
        if (move_y > 255 || move_y < -255) move_y = 0;

        mouse_x += move_x;
        mouse_y -= move_y;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1024 - 32) mouse_x = 1024 - 32;
        if (mouse_y > 768 - 32)  mouse_y = 768 - 32;
        draw_mouse(mouse_x, mouse_y, 255);
        mouse_last_x = mouse_x;
        mouse_last_y = mouse_y;
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
