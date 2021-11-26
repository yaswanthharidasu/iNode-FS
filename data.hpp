#ifndef __DATA_H_
#define __DATA_H_

#include<cmath>
using namespace std;

/*
    File System contains:
        Super Block || file_inode_mapping || inode bitmap || data bitmap || inode blocks (inode table) || data blocks

    Storing the Super block, bitmaps, inode table in data blocks only
*/
#define DISK_SIZE 536870912             // 512MB
#define BLOCK_SIZE 1024                 // 1KB
#define NO_OF_DISK_BLOCKS 524288        // DISK_SIZE/BLOCK_SIZE
#define NO_OF_INODES 32768              // 1:16KB Ratio 
#define NO_OF_BLOCK_POINTERS 20

// For 16KB (16384 bytes)       --> one inode
// For 512MB (536870912 bytes)  --> 536870912/16384 = 32768 inodes

struct file_inode_mapping {
    char file_name[20];
    int inode_num;
};

struct inode {
    int file_size;
    // 20 direct pointers to the data blocks
    int ptr[NO_OF_BLOCK_POINTERS];
};

// False means block/inode is free
// True means not free
bool inode_bitmap[NO_OF_INODES] = { false };
int no_of_inode_bitmap_blocks = (int)(ceil((float)sizeof(inode_bitmap) / BLOCK_SIZE));

bool disk_bitmap[NO_OF_DISK_BLOCKS] = { false };
int no_of_disk_bitmap_blocks = (int)(ceil((float)sizeof(disk_bitmap) / BLOCK_SIZE));


struct super_block {
    int no_of_super_block_blocks = (int)(ceil((float)sizeof(struct super_block) / BLOCK_SIZE));
    int no_of_file_inode_mapping_blocks = (int)(ceil(((float)(sizeof(struct file_inode_mapping) * NO_OF_INODES) / BLOCK_SIZE)));
    int inode_bitmap_start = no_of_super_block_blocks + no_of_file_inode_mapping_blocks;
    int disk_bitmap_start = inode_bitmap_start + no_of_inode_bitmap_blocks;

    int inode_blocks_start = disk_bitmap_start + no_of_disk_bitmap_blocks;
    int no_of_inode_blocks = (int)(ceil(((float)(sizeof(struct inode) * NO_OF_INODES) / BLOCK_SIZE)));

    int data_blocks_start = inode_blocks_start + no_of_inode_blocks;
    int no_of_available_blocks = NO_OF_DISK_BLOCKS - data_blocks_start;
};
#endif
