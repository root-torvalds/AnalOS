#include <stdint.h>

uint64_t ahci_abar_address;

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
    uint32_t reserved1[4]; // Дополнение до 64 байт на один порт
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

    // Вся базовая структура занимает 0x2C (44 байта). 
    // Чтобы порты начались ровно с 0x100 (256 байт), пропускаем 212 байт.
    uint8_t  reserved[212]; 
    AHCI_Port ports[32]; // Массив из 32 портов
} __attribute__((packed)) AHCI_MemoryMap;

typedef struct {
    uint8_t  cfl:5;     
    uint8_t  a:1;       
    uint8_t  w:1;       
    uint8_t  p:1;       
    uint16_t prdtl;     
    uint32_t prdbc;     
    uint32_t ctba;      
    uint32_t ctbau;     
    uint32_t reserved[4];
} __attribute__((packed)) AHCI_CmdHeader;

typedef struct {
    uint32_t dba;       
    uint32_t dbau;      
    uint32_t reserved0;
    uint32_t dbc:22;    
    uint32_t reserved1:9;
    uint32_t ioc:1;     
} __attribute__((packed)) AHCI_PrdtEntry;

// Выровненные глобальные буферы для нулевого порта
__attribute__((aligned(1024))) AHCI_CmdHeader my_cmd_list[32];
__attribute__((aligned(256)))  uint8_t my_received_fis[256];
__attribute__((aligned(1024))) uint8_t my_command_table[1024];

volatile AHCI_MemoryMap* ahci = 0;

void init_ahci(void) {
    if (ahci_abar_address == 0) return;
    
    ahci = (volatile AHCI_MemoryMap*)((uint64_t)ahci_abar_address);
    volatile AHCI_Port* port = &ahci->ports[0]; // Работаем с портом 0

    // Проверяем статус (должно быть 3 - подключен и активен)
    if ((port->ssts & 0x0F) != 3) return;

    // Безопасно останавливаем порт (сбрасываем ST)
    port->cmd &= ~(1 << 0);
    
    // Таймаут ожидания остановки (биты ST и CR должны уйти в 0)
    uint32_t timeout = 1000000;
    while ((port->cmd & (1 << 15)) && --timeout);
    
    // Отключаем прием FIS (FRE)
    port->cmd &= ~(1 << 4);
    timeout = 1000000;
    while ((port->cmd & (1 << 14)) && --timeout);

    // Прописываем физические адреса наших буферов контроллеру
    uint64_t clb_phys = (uint64_t)&my_cmd_list[0];
    port->clb = (uint32_t)clb_phys;
    port->clbu = (uint32_t)(clb_phys >> 32);

    uint64_t fb_phys = (uint64_t)&my_received_fis[0];
    port->fb = (uint32_t)fb_phys;
    port->fbu = (uint32_t)(fb_phys >> 32);

    // Привязываем нулевой слот списка команд к нашей таблице команд
    uint64_t ctba_phys = (uint64_t)&my_command_table[0];
    my_cmd_list[0].ctba = (uint32_t)ctba_phys;
    my_cmd_list[0].ctbau = (uint32_t)(ctba_phys >> 32);

    // Очищаем регистр ошибок связи, без этого контроллер проигнорирует запуск
    port->serr = 0xFFFFFFFF;

    // Включаем обратно FRE и ST
    port->cmd |= (1 << 4); // FRE = 1
    port->cmd |= (1 << 0); // ST = 1
}

void ahci_read_sector(uint64_t lba, void* target_buffer) {
    if (!ahci) return;
    
    volatile AHCI_Port* port = &ahci->ports[0];
    
    // Используем жестко слот 0 для простоты
    int slot = 0; 
    if (port->ci & (1 << slot)) return; 

    // Заполняем Command Header в списке команд
    volatile AHCI_CmdHeader* cmd_hdr = &my_cmd_list[slot];
    cmd_hdr->cfl = 5;  // Размер FIS в dword-ах (5 * 4 = 20 байт)
    cmd_hdr->w = 0;    // Чтение
    cmd_hdr->prdtl = 1;// Один кусок данных в таблице PRDT

    // Настраиваем PRDT Entry (описание буфера назначения) внутри нашей таблицы команд
    volatile AHCI_PrdtEntry* prdt = (volatile AHCI_PrdtEntry*)(&my_command_table[0] + 0x80);
    prdt->dba = (uint32_t)((uint64_t)target_buffer);
    prdt->dbau = (uint32_t)((uint64_t)target_buffer >> 32);
    prdt->dbc = 511;   // 512 байт минус 1
    prdt->ioc = 0;     // Без прерываний (работаем по опросу бита CI)

    // Формируем FIS-пакет типа Host-to-Device (запись в порты ATA)
    volatile uint8_t* fis = (volatile uint8_t*)&my_command_table[0];
    for(int i = 0; i < 20; i++) fis[i] = 0;

    fis[0] = 0x27; // Register FIS - Host to Device
    fis[1] = 0x80; // Сделать операцией (не просто обновление регистров)
    fis[2] = 0x25; // READ DMA EXT

    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = 1 << 6; // Режим LBA
    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);

    fis[12] = 1; // Количество секторов (Младший байт)
    fis[13] = 0; // Старший байт количества секторов

    // Ожидаем, пока очистятся биты BSY (0x80) и DRQ (0x08)
    uint32_t timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && --timeout);
    if (timeout == 0) return; // Устройство занято, выходим чтобы не зависнуть

    // Даем команду контроллеру обработать слот 0
    port->ci = (1 << slot);

    // Ожидаем завершения команды (контроллер сам сбросит бит в 0)
    timeout = 5000000;
    while (--timeout) {
        if ((port->ci & (1 << slot)) == 0) {
            break; // Команда успешно выполнена!
        }
    }
}

