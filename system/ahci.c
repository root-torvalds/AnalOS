#include <stdint.h>
#include <stdbool.h>
#include "kernel.h" // Для вызова printf

#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_READ_DMA_EXT  0x25
#define FIS_TYPE_REG_H2D      0x27

uint64_t ahci_abar_address;

// 1. Структуры регистров HBA, полностью скопированные из статьи OSDev
typedef struct {
    uint32_t clb;       
    uint32_t clbu;      
    uint32_t fb;        
    uint32_t fbu;       
    uint32_t is;        
    uint32_t ie;        
    uint32_t cmd;       
    uint32_t reserved0; 
    uint32_t tfd;       
    uint32_t visa;      
    uint32_t ssts;      
    uint32_t sctl;      
    uint32_t serr;      
    uint32_t sact;      
    uint32_t ci;        
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed)) AHCI_Port;

typedef struct {
    uint32_t cap;       
    uint32_t ghc;       
    uint32_t is;        
    uint32_t pi;        
    uint32_t vs;        
    uint32_t bcl;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  reserved[208]; // СТРОГО 208 байт для смещения 0x100!
    AHCI_Port ports[32];    // СТРОГО массив из 32 портов!
} __attribute__((packed)) AHCI_MemoryMap;

typedef struct {
    uint8_t  cfl:5;     
    uint8_t  a:1;       
    uint8_t  w:1;       
    uint8_t  p:1;       
    uint8_t  r:1;       
    uint8_t  b:1;       
    uint8_t  c:1;       
    uint8_t  rsv0:1;    
    uint8_t  pmp:4;      
    uint16_t prdtl;     
    volatile uint32_t prdbc;     
    uint32_t ctba;      
    uint32_t ctbau;     
    uint32_t reserved[4]; // СТРОГО массив из 4 элементов, чтобы заголовок весил 32 байта!
} __attribute__((packed)) AHCI_CmdHeader;


typedef struct {
    uint32_t dba;       
    uint32_t dbau;      
    uint32_t reserved0;
    uint32_t dbc:22;    
    uint32_t reserved1:9;
    uint32_t ioc:1;     
} __attribute__((packed)) AHCI_PrdtEntry;

typedef struct {
    uint8_t        cfis[64];   // Смещение 0x00: Место под FIS (64 байта)
    uint8_t        acmd[16];   // Смещение 0x40: ATAPI (16 байт)
    uint8_t        rsv[48];    // Смещение 0x50: Зарезервировано (48 байт)
    AHCI_PrdtEntry prdt_entry; // Смещение 0x80: Таблица PRDT начнется точно тут!
} __attribute__((packed)) AHCI_CommandTable;

__attribute__((aligned(1024))) AHCI_CmdHeader my_cmd_list[32];      // 32 командных слота (1КБ)
__attribute__((aligned(256)))  uint8_t         my_received_fis[256]; // 256 байт под прилетающие FIS
__attribute__((aligned(1024))) AHCI_CommandTable my_command_table[32]; // 32 таблицы команд под каждый слот!

volatile AHCI_MemoryMap* ahci = 0;

static inline uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | (((uint32_t)slot) << 11) |
                       (((uint32_t)func) << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    __asm__ volatile("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xCF8));
    uint32_t tmp;
    __asm__ volatile("inl %1, %0" : "=a"(tmp) : "Nd"((uint16_t)0xCFC));
    return tmp;
}

void init_ahci(void) {
    if (ahci_abar_address == 0) {
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                uint32_t id = pci_read_dword(bus, slot, 0, 0);
                if (id == 0xFFFFFFFF) continue;
                uint32_t class_reg = pci_read_dword(bus, slot, 0, 0x08);
                uint8_t base_class = (class_reg >> 24) & 0xFF;
                uint8_t sub_class  = (class_reg >> 16) & 0xFF;
                if (base_class == 0x01 && sub_class == 0x06) {
                    uint32_t bar5 = pci_read_dword(bus, slot, 0, 0x24);
                    if (bar5 != 0 && !(bar5 & 1)) {
                        ahci_abar_address = bar5 & 0xFFFFFFF0;
                        break;
                    }
                }
            }
            if (ahci_abar_address != 0) break;
        }
    }

    if (ahci_abar_address == 0) return;
    
    ahci = (volatile AHCI_MemoryMap*)((uint64_t)ahci_abar_address);
    volatile AHCI_Port* port = &ahci->ports;

    if ((port->ssts & 0x0F) != 3) return;

    port->cmd &= ~(1 << 0);
    uint32_t timeout = 1000000;
    while ((port->cmd & (1 << 15)) && --timeout);
    port->cmd &= ~(1 << 4);
    timeout = 1000000;
    while ((port->cmd & (1 << 14)) && --timeout);

    uint64_t clb_phys = (uint64_t)&my_cmd_list;
    port->clb = (uint32_t)clb_phys;
    port->clbu = (uint32_t)(clb_phys >> 32);

    uint64_t fb_phys = (uint64_t)&my_received_fis;
    port->fb = (uint32_t)fb_phys;
    port->fbu = (uint32_t)(fb_phys >> 32);

    for (int i = 0; i < 32; i++) {
        my_cmd_list[i].prdtl = 8; 
        uint64_t ctba_phys = (uint64_t)&my_command_table[i];
        my_cmd_list[i].ctba = (uint32_t)ctba_phys;
        my_cmd_list[i].ctbau = (uint32_t)(ctba_phys >> 32);
    }

    port->serr = 0xFFFFFFFF;
    port->cmd |= (1 << 4); 
    port->cmd |= (1 << 0); 
}

void ahci_read_sector(uint64_t lba, void* target_buffer) {
    if (!ahci) return;
    volatile AHCI_Port* port = &ahci->ports;
    int slot = 0; 
    if (port->ci & (1 << slot)) return; 

    volatile AHCI_CmdHeader* cmd_hdr = &my_cmd_list[slot];
    cmd_hdr->cfl = 5;
    cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;

    // Прямой доступ к полям структуры без опасных сдвигов адреса памяти
    volatile AHCI_PrdtEntry* prdt = &my_command_table[slot].prdt_entry;
    uintptr_t buf_phys = (uintptr_t)target_buffer;
    prdt->dba = (uint32_t)buf_phys;
    prdt->dbau = (uint32_t)(buf_phys >> 32);
    prdt->dbc = 511; 
    prdt->ioc = 0;

    volatile uint8_t* fis = my_command_table[slot].cfis;
    for(int i = 0; i < 64; i++) fis[i] = 0;

    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 0x80;
    fis[2] = ATA_CMD_READ_DMA_EXT;

    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 1 << 6;
    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);

    fis[12] = 1;
    fis[13] = 0;

    uint32_t timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && --timeout);
    if (timeout == 0) return;

    port->ci = (1 << slot);
    timeout = 5000000;
    while (--timeout) {
        if ((port->ci & (1 << slot)) == 0) break;
    }
}

bool ahci_write_sector(uint64_t lba, const void* source_buffer) {
    if (!ahci) return false;
    volatile AHCI_Port* port = &ahci->ports;
    int slot = 0; 
    if (port->ci & (1 << slot)) return false; 

    port->is = (uint32_t)-1;

    volatile AHCI_CmdHeader* cmd_hdr = &my_cmd_list[slot];
    cmd_hdr->cfl = 5;
    cmd_hdr->w = 1;
    cmd_hdr->prdtl = 1;

    // Прямой доступ к полям структуры без опасных сдвигов адреса памяти
    volatile AHCI_PrdtEntry* prdt = &my_command_table[slot].prdt_entry;
    uintptr_t buf_phys = (uintptr_t)source_buffer;
    prdt->dba = (uint32_t)buf_phys;
    prdt->dbau = (uint32_t)(buf_phys >> 32);
    prdt->dbc = 511; 
    prdt->ioc = 0;

    volatile uint8_t* fis = my_command_table[slot].cfis;
    for (int i = 0; i < 64; i++) fis[i] = 0;

    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = 0x80;
    fis[2] = ATA_CMD_WRITE_DMA_EXT;

    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 1 << 6;
    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);

    fis[12] = 1;
    fis[13] = 0;

    uint32_t timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && --timeout);
    if (timeout == 0) return false;

    port->ci = (1 << slot);
    timeout = 5000000;
    while (--timeout) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return false;
    }

    if (timeout == 0) return false;
    return true;
}

void print_hex(uint32_t val, unsigned int x, unsigned int y) {
    char hex_str[11];
    hex_str[0] = '0';
    hex_str[1] = 'x';
    const char* hex_chars = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        hex_str[2 + i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    hex_str[10] = '\0';
    printf(hex_str, x, y, 255, 255, 0);
}

void debug_ahci_status(void) {
    if (!ahci) {
        printf("AHCI Pointer is NULL!", 30, 150, 255, 0, 0);
        return;
    }
    volatile AHCI_Port* port = &ahci->ports;
    printf("SSTS:", 30, 170, 255, 255, 255);
    print_hex(port->ssts, 100, 170);
    printf("TFD :", 30, 190, 255, 255, 255);
    print_hex(port->tfd, 100, 190);
    printf("SERR:", 30, 210, 255, 255, 255);
    print_hex(port->serr, 100, 210);
}

void flush_cache_line(void* addr, uint32_t length) {
    uintptr_t start = (uintptr_t)addr;
    for (uintptr_t i = start; i < start + length; i += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(i) : "memory");
    }
}

__attribute__((aligned(4096))) uint8_t ahci_test_write_buf[512];
__attribute__((aligned(4096))) uint8_t ahci_test_read_buf[512];

void test_ahci_write_x(void) {
    for(int i = 0; i < 512; i++) ahci_test_write_buf[i] = 0;
    ahci_test_write_buf[0] = 0x58; 
    
    flush_cache_line(&ahci_test_write_buf, 512);
    ahci_write_sector(0, (void*)&ahci_test_write_buf);
}

uint32_t test_ahci_read_x(void) {
    for(int i = 0; i < 512; i++) ahci_test_read_buf[i] = 0;
    
    flush_cache_line(&ahci_test_read_buf, 512);
    ahci_read_sector(0, (void*)&ahci_test_read_buf);
    flush_cache_line(&ahci_test_read_buf, 512);
    
    return (uint32_t)ahci_test_read_buf[0];
}

