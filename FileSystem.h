#ifndef FILESYSTEM_FILESYSTEM_H
#define FILESYSTEM_FILESYSTEM_H

#include <glob.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_FILE_COUNT 100
#define MAX_NAME_LENGTH 30
#define BLOCK_SIZE 4096


typedef uint32_t SIZE;

struct Block
{
    char data[BLOCK_SIZE]; //4096 bajtów
};


struct File
{
    bool  is_in_use;
    char name[MAX_NAME_LENGTH];
    SIZE size; // total file size
    SIZE used_blocks;
    SIZE first_block;
};


struct SuperBlock
{
    SIZE system_size; // all_blocks * 4096 + metadata
    SIZE user_space_in_use; // used_blocks * 4096
    SIZE user_space; // all_blocks * 4096
    SIZE blocks_count; // all_blocks
    SIZE the_biggest_hole;
    SIZE file_count;
    SIZE holes_count;
};

struct Hole
{
    SIZE first_block;
    SIZE blocks_count;
};



#endif //FILESYSTEM_FILESYSTEM_H
