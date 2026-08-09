#include <stdint.h>

#pragma once

void write_msr(uint32_t msr, uint64_t value);
void syscall_handler(void);
void init_syscalls(void);
