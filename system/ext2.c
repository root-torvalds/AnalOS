#include "ext2.h"
#include "kernel.h"

uint8_t superblock[1024];
uint8_t group_desc[1024];
uint8_t root_dir_block[1024];
uint8_t inode_table_block[1024];
uint8_t inode_bitmap[1024] = {0xFF, 0x03};
uint8_t block_bitmap[1024] = {0xFE, 0xFF, 0xFF, 0x03};

typedef struct __attribute__((packed)) {
    uint32_t  inodecount;       // 0
    uint32_t  blockcount;       // 4
    uint32_t  r_blocks_count;     // 8
    uint32_t  free_blockcount;  // 12
    uint32_t  free_inodecount;  // 16
    uint32_t  first_data_block;   // 20
    uint32_t  log_block_size;     // 24
    uint32_t  log_frag_size;      // 28
    uint32_t  blockper_group;   // 32
    uint32_t  frags_per_group;    // 36
    uint32_t  inodeper_group;   // 40
    uint32_t  mtime;              // 44
    uint32_t  wtime;              // 48
    uint16_t  mnt_count;          // 52
    uint16_t  max_mnt_count;      // 54
    uint16_t  magic;              // 56  (0xEF53)
    uint16_t  state;              // 58
    uint16_t  errors;             // 60
    uint16_t  minor_rev_level;    // 62
    uint32_t  lastcheck;          // 64
    uint32_t  checkinterval;      // 68
    uint32_t  creator_os;         // 72
    uint32_t  rev_level;          // 76  (1)
    uint16_t  def_resuid;         // 80
    uint16_t  def_resgid;         // 82
    
    // Поля ревизии 1 (Dynamic) - Обязательные для rev_level = 1!
    uint32_t  first_ino;          // 84  (11)
    uint16_t  inode_size;         // 88  (128)
    uint16_t  block_group_nr;     // 90
    uint32_t  feature_compat;     // 92  (0)
    uint32_t  feature_incompat;   // 96  (0)
    uint32_t  feature_ro_compat;  // 100 (0)
    uint8_t   uuid[16];           // 104
    char      volume_name[16];    // 120
    char      last_mounted[64];   // 136
    uint32_t  algorithm_usage_bitmap; // 200
    
    // Добиваем блок до 1024 байт (1024 - 204 = 820 байт)
    uint8_t   reserved[820];      
} SuperBlock;

typedef struct __attribute__((packed)) {
    uint32_t bg_block_bitmap;      // Блок битовой карты занятости блоков (32 бита)
    uint32_t bg_inode_bitmap;      // Блок битовой карты занятости inode (32 бита)
    uint32_t bg_inode_table;       // Стартовый блок таблицы inode этой группы
    uint16_t bg_free_blocks_count; // Количество свободных блоков в группе
    uint16_t bg_free_inodes_count; // Количество свободных inode в группе
    uint16_t bg_used_dirs_count;   // Количество каталогов, созданных в группе
    uint16_t bg_pad;               // Выравнивание (padding) до границы слова
    uint32_t bg_reserved[3];       // Зарезервировано для будущего использования (нули)
} GroupDescriptor;

typedef struct __attribute__((packed)) {
    uint16_t mode;        // Тип файла и права доступа (папка, обычный файл и т.д.)
    uint16_t uid;         // ID пользователя-владельца (0 для root)
    uint32_t size;        // Размер файла или папки в байтах
    uint32_t atime;       // Время последнего доступа (0)
    uint32_t ctime;       // Время создания (0)
    uint32_t mtime;       // Время модификации (0)
    uint32_t dtime;       // Время удаления (0)
    uint16_t gid;         // ID группы владельца (0 для root)
    uint16_t links_count; // Количество ссылок на эту инноду
    uint32_t blocks;      // Количество 512-байтных секторов, выделенных файлу!
    uint32_t flags;       // Флаги файла (0)
    uint32_t osd1;        // Специфичные данные для ОС (0)
    uint32_t block[15];   // Главное: Массив указателей на блоки данных файла!
    uint32_t generation;  // Версия файла (0)
    uint32_t file_acl;    // ACL файла (0)
    uint32_t dir_acl;     // ACL директории (0)
    uint32_t faddr;       // Адрес фрагмента (0)
    uint8_t  osd2[12];    // Специфичные данные для ОС (0)
} Inode;

typedef struct __attribute__((packed)) {
    uint32_t inode;     // Номер инноды (2 для "." и "..")
    uint16_t rec_len;   // Длина записи (12 байт для первой, 1012 для второй)
    uint8_t  name_len;  // Длина имени (1 для ".", 2 для "..")
    uint8_t  file_type; // Тип файла (2 = папка/директория)
    char     name[4];   // Имя файла (выровнено по границе 4 байт)
} DirectoryEntry;

void init_ext2() {
    for (int i = 0; i < 1024; i++) {
        superblock[i]        = 0;
        group_desc[i]        = 0;
        inode_table_block[i] = 0;
    }
    
    for (uint64_t sector = 12; sector <= 51; sector++) {
        ahci_write_sector(sector, inode_table_block); 
    }
    
    SuperBlock *superblock_ext2 = (SuperBlock *) superblock;
    
    superblock_ext2->inodecount       = 84;
    superblock_ext2->blockcount       = 1024;
    superblock_ext2->free_blockcount  = 996;
    superblock_ext2->free_inodecount  = 74;
    superblock_ext2->first_data_block = 1;
    superblock_ext2->log_block_size   = 0;
    superblock_ext2->blockper_group   = 8192;
    superblock_ext2->inodeper_group   = 84;
    superblock_ext2->magic            = 0xEF53;
    superblock_ext2->state            = 1;
    superblock_ext2->rev_level        = 1;
    superblock_ext2->first_ino        = 11; 
    superblock_ext2->inode_size       = 256;
    superblock_ext2->mtime = 1717200000;       // Любое валидное Unix-время (например, 2024 год)
    superblock_ext2->wtime = 1717200000;       // Время последней записи
    superblock_ext2->lastcheck = 1717200000;   // Время последней проверки fsck
    superblock_ext2->checkinterval = 0;        // Отключаем проверку по времени
    superblock_ext2->max_mnt_count = 0xFFFF;   // Отключаем проверку по числу монтирований
    superblock_ext2->errors = 1;               // Спецификация Ext2: 1 = Continue
    superblock_ext2->r_blocks_count = 51;
    superblock_ext2->frags_per_group = 8192;
    
    ahci_write_sector(2, superblock);
    ahci_write_sector(3, superblock + 512);
    
    GroupDescriptor *group_desc_ext2 = (GroupDescriptor *) group_desc;
    
    group_desc_ext2->bg_block_bitmap      = 3;
    group_desc_ext2->bg_inode_bitmap      = 4;
    group_desc_ext2->bg_inode_table       = 5;
    group_desc_ext2->bg_free_blocks_count = 996;
    group_desc_ext2->bg_free_inodes_count = 74;
    group_desc_ext2->bg_used_dirs_count   = 1;
    
    ahci_write_sector(4, group_desc);
    ahci_write_sector(5, group_desc + 512);
    
    ahci_write_sector(6, block_bitmap);
    ahci_write_sector(7, block_bitmap + 512);

    ahci_write_sector(8, inode_bitmap);
    ahci_write_sector(9, inode_bitmap + 512);
    
    Inode *root_inode = (Inode *)(inode_table_block + 256);
    
    root_inode->mode = 0x41ED;
    root_inode->size = 1024;
    root_inode->links_count = 2;
    root_inode->blocks = 2;
    root_inode->block[0] = 26;
    
    ahci_write_sector(10, inode_table_block);
    ahci_write_sector(11, inode_table_block + 512);
    
    DirectoryEntry *entry1 = (DirectoryEntry *) root_dir_block;

    entry1->name[0] = '.';
    entry1->inode = 2;
    entry1->rec_len = 12;
    entry1->name_len = 1;
    entry1->file_type = 0;
    
    DirectoryEntry *entry2 = (DirectoryEntry *) (root_dir_block + 12);
    
    entry2->name[0] = '.';
    entry2->name[1] = '.';
    entry2->inode = 2;
    entry2->rec_len = 1012;
    entry2->name_len = 2;
    entry2->file_type = 0;
    
    ahci_write_sector(52, root_dir_block);
    ahci_write_sector(53, root_dir_block + 512);
}
