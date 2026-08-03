#include <stdint.h>
#include <stdbool.h>
#pragma once

void init_ext2();
void ext2_add_dir_entry(uint32_t parent_block, uint32_t file_inode, const char* name, uint8_t file_type);
bool ext2_create_file(const char* path, const char* name, uint16_t mode, const void* data, uint32_t data_size);
bool ext2_create_dir(const char* path, const char* name);
