#include "kernel.h"

void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);

    __asm__ volatile(
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
        : "memory"
    );
}

void syscall_handler(void) {
    uint64_t syscall_number;
    
    __asm__ volatile("mov %%rax, %0" : "=r"(syscall_number));

    if (syscall_number == 1) {
        printf("Вызвана функция 1: Создание папки!\n", 100, 120, 0, 255, 0, 255);
    } else if (syscall_number == 2) {
        printf("Вызвана функция 2: Создание файла!\n", 100, 140, 0, 255, 0, 255);
    }
}

void init_syscalls(void) {

    uint64_t star_value = ((uint64_t)0x1B << 48) | ((uint64_t)0x38 << 32);
    write_msr(0xC0000081, star_value);

    write_msr(0xC0000082, (uint64_t)syscall_handler);
    write_msr(0xC0000084, 0x200);

    __asm__ volatile(
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        "or $1, %%eax\n"
        "wrmsr\n"
        : : : "eax", "ecx", "edx"
    );
}
