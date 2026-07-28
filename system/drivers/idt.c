#include <stdint.h>
#include "../include/idt.h"

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" :: "a"(data), "Nd"(port));
}

void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

struct IDTEntry {
    uint16_t offset_low;
    uint16_t segment_selector;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct IDTEntry idt[256];

extern void dummy_handler_asm(void);
extern void mouse_handler_asm(void);
extern void keyboard_handler_asm(void);

void init_idt() {
    uint64_t dummy_address = (uint64_t)dummy_handler_asm;

    for(int i = 0; i < 256; i++) {
        idt[i].offset_low  = (uint16_t)(dummy_address & 0xFFFF);
        idt[i].offset_mid  = (uint16_t)((dummy_address >> 16) & 0xFFFF);
        idt[i].offset_high = (uint32_t)((dummy_address >> 32) & 0xFFFFFFFF);
        idt[i].segment_selector = 0x38;
        idt[i].ist = 0;
        idt[i].attributes = 0x8E;
        idt[i].reserved = 0;
    }

    uint64_t address = (uint64_t)keyboard_handler_asm;
    idt[33].offset_low  = (uint16_t)(address & 0xFFFF);
    idt[33].offset_mid  = (uint16_t)((address >> 16) & 0xFFFF);
    idt[33].offset_high = (uint32_t)((address >> 32) & 0xFFFFFFFF);
    idt[33].segment_selector = 0x38;
    idt[33].ist = 0;
    idt[33].attributes = 0x8E;
    idt[33].reserved = 0;

    uint64_t mouse_address = (uint64_t)mouse_handler_asm;
    idt[0x2C].offset_low  = (uint16_t)(mouse_address & 0xFFFF);
    idt[0x2C].offset_mid  = (uint16_t)((mouse_address >> 16) & 0xFFFF);
    idt[0x2C].offset_high = (uint32_t)((mouse_address >> 32) & 0xFFFFFFFF);
    idt[0x2C].segment_selector = 0x38;
    idt[0x2C].ist = 0;
    idt[0x2C].attributes = 0x8E;
    idt[0x2C].reserved = 0;

    struct IDTPointer idtr;
    idtr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idtr.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" :: "m"(idtr));
}

void init_ioapic(void) {
    outb(0x20, 0x11);
    io_wait();
    outb(0x21, 0x20);
    io_wait();
    outb(0x21, 0x04);
    io_wait();
    outb(0x21, 0x01);
    io_wait();

    outb(0xA0, 0x11);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    outb(0x21, ~0x06); // Включаем IRQ1 (клавиатура) и IRQ2 (каскад на Slave PIC)
    io_wait();
    outb(0xA1, ~0x10); // Включаем IRQ12 (мышь) на Slave PIC
    io_wait();
}
