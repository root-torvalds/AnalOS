#include <stdbool.h>

void init_ahci(void);
void ahci_read_sector(uint64_t lba, void* target_buffer);
bool ahci_write_sector(uint64_t lba, const void* source_buffer);
void print_hex(uint32_t val, unsigned int x, unsigned int y);
void debug_ahci_status(void);
void test_ahci_write_x(void);
uint32_t test_ahci_read_x(void);
