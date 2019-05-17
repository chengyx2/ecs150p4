#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define fat_length(fat_amount) (fat_amount*BLOCK_SIZE/2)

/* TODO: Phase 1 */
typedef struct superblock{
    uint64_t signature;
    uint16_t total_amount;
    uint16_t root_idx;
    uint16_t data_idx;
    uint16_t data_amount;
    uint8_t FAT_amount;
    uint8_t padding[4079];
}__attribute__((__packed__)) superblock;



typedef uint16_t* FAT;

typedef struct root_entry{
    uint8_t filename[16];
    uint32_t filesize;
    uint16_t data_start;
    uint8_t padding[10];
}__attribute__((__packed__)) root_entry;

typedef root_entry* root_dir[FS_FILE_MAX_COUNT];

superblock super_block;

FAT fat_array = NULL;

root_dir root;

bool change_table = false;

bool mounted = false;

int block_to_buffer(size_t block, void* buff, size_t length){
    for (size_t i = block; i < block + length; i++){
        if(block_read(i, buff + i * BLOCK_SIZE) == -1)
            return -1;
    }
    return 0;
}

int buffer_to_block(size_t block, void* buff, size_t length){
    for (size_t i = block; i < block + length; i++){
        if(block_write(i, buff + i * BLOCK_SIZE) == -1)
            return -1;
    }
    return 0;
}

int fat_free(){
    int count = 0;
    for (int i = 1; i < fat_length(super_block.FAT_amount); i++){
        if (fat_array[i] == 0)
            count++;
    }
    return count;
}

int root_free(){
    int count = 0;
    for (int i = 1; i < FS_FILE_MAX_COUNT; i++){
        if (root[i]->filename[0] == '\0')
            count++;
    }
    return count;
}

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
    char* signature = "ECS150FS";
    if(block_disk_open(diskname) == -1)
        return -1;

    block_read(0, (void*) &super_block);
    
    if (memcmp((char*) &super_block.signature, signature, 8) != 0)
        return -1;
    
    if (super_block.total_amount != block_disk_count())
        return -1;
    
    fat_array = malloc(fat_length(super_block.FAT_amount) * sizeof(uint16_t));
    if (fat_array == NULL)
        return -1;
    if (block_to_buffer(1, fat_array, super_block.FAT_amount) == -1)
        return -1;
    
    if (block_read(super_block.root_idx, (void*) root) == -1)
        return -1;
    
    mounted = true;
    return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
    // to do check file des
    
    if (change_table){
        if (buffer_to_block(1, fat_array, super_block.FAT_amount) == -1)
            return -1;
        if (block_write(super_block.root_idx, root) == -1)
            return -1;
    }
    
    mounted = false;
    return block_disk_close();
}

int fs_info(void)
{
	/* TODO: Phase 1 */
    if (!mounted)
        return -1;
        
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", super_block.total_amount);
    printf("fat_blk_count=%d\n", super_block.FAT_amount);
    printf("rdir_blk=%d\n",super_block.root_idx);
    printf("data_blk=%d\n",super_block.data_idx);
    printf("data_blk_count=%d\n",super_block.data_amount);
    printf("fat_free_ratio=%d/%d\n",fat_free(), fat_length(super_block.FAT_amount));
    printf("rdir_free_ratio=%d/%d\n",root_free(), FS_FILE_MAX_COUNT);
    
    return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
    return -1;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
    return -1;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
    return -1;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
    return -1;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
    return -1;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
    return -1;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
    return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    return -1;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    return -1;
}

