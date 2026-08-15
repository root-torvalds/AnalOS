#include <stdint.h>
#include "kernel.h"

extern volatile int has_keyboard_event;
extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

int mouse_last_x = 512;
int mouse_last_y = 384;
int mouse_cycle = 0;
int mouse_x = 512;
int mouse_y = 384;

uint8_t mouse_bytes[4];

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

    volatile uint32_t clean = 1000;
    while ((inb(0x64) & 1) && --clean) {
        inb(0x60);
        io_wait();
    }

    // Чистая коммерческая инициализация контроллера 8042
    mouse_wait_write();
    outb(0x64, 0xA8); // Включить мышь

    mouse_wait_write();
    outb(0x64, 0x20); // Запрос Command Byte
    mouse_wait_read();
    uint8_t comp_status = inb(0x60);

    comp_status |= 0x02;  // Включить прерывания IRQ12 от мыши
    comp_status &= ~0x20; // Выключить блокировку мыши

    mouse_wait_write();
    outb(0x64, 0x60); // Запись Command Byte
    mouse_wait_write();
    outb(0x60, comp_status);

    // Сброс самой мыши к дефолту
    mouse_write(0xF6);
    mouse_read();

    // Разрешаем мыши слать относительные пакеты
    mouse_write(0xF4);
    mouse_read();

    mouse_cycle = 0;
    mouse_x = 512;
    mouse_y = 384;
}

void mouse_handler_c() {
    uint8_t status = inb(0x64);

    // Проверяем, что в буфере i8042 реально есть данные от мыши (Бит 0 = 1, Бит 5 = 1)
    if ((status & 0x01) == 0 || (status & 0x20) == 0) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    uint8_t data = inb(0x60);

    // АППАРАТНАЯ СИНХРОНИЗАЦИЯ ПОТОКА: 
    // Байт 0 пакета мыши ОБЯЗАН иметь бит 3 равным 1 (маска 0x08).
    // Если его нет — поток сместился (прилетел 4-й байт колесика). Сбрасываем цикл в 0!
    if (mouse_cycle == 0 && (data & 0x08) == 0) {
        mouse_cycle = 0; // Насильно выравниваем фазу чтения портов
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    mouse_bytes[mouse_cycle] = data;
    mouse_cycle++;

    // Как только накопили честную триаду байт
    if (mouse_cycle == 3) {
        mouse_cycle = 0; // Сразу готовы к следующей посылке

        // ЭТАЛОННАЯ МАТЕМАТИКА PS/2: Явное приведение без лишних условий
        // Каст (int8_t) аппаратно превращает беззнаковый 255 в чистый -1 на x86_64
        int32_t move_x = (int8_t)mouse_bytes[1];
        int32_t move_y = (int8_t)mouse_bytes[2];

        // Накапливаем дельты в глобальные переменные координат
        mouse_x += move_x;
        mouse_y -= move_y; // Инверсия оси Y по спецификации экранов

        // Жесткие ограничители разрешения фреймбуфера ОС AnalOS (1024x768)
        if (mouse_x < 0)    mouse_x = 0;
        if (mouse_y < 0)    mouse_y = 0;
        if (mouse_x > 1023) mouse_x = 1023;
        if (mouse_y > 767)  mouse_y = 767;

        // Выставляем системный флаг события для главного цикла kernel.c
        has_mouse_event = 1;

        mouse_last_x = mouse_x;
        mouse_last_y = mouse_y;
    }

    // Посылаем сигнал завершения прерывания (EOI) в оба контроллера прерываний 8259 PIC
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
