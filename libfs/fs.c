#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define fat_length(fat_amount) ((fat_amount)*BLOCK_SIZE/2)

/* TODO: Phase 1 */
//super block structure definition
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

//root entry structure definition
typedef struct root_entry{
    uint8_t filename[16];
    uint32_t filesize;
    uint16_t data_start;
    uint8_t padding[10];
}__attribute__((__packed__)) root_entry;

//file descriptor definition
typedef struct file_descriptor{
    int root_idx;
    size_t offset;
    //record the block index where the offset locates
    uint16_t block_idx;
}file_descriptor;

typedef root_entry* root_dir;

//global super block structure
superblock super_block;

//global fat table
FAT fat_array = NULL;

//global root entry array
root_dir root;

//global file descriptor array
file_descriptor open_files[FS_OPEN_MAX_COUNT];

//global flag for recording changing tables
bool change_table = false;

//global flag for mounting
bool mounted = false;

//record next free root entry and next free position in fat table
int root_next_free = 0;
int fat_next_free = 1;

//reading multiple blocks to buff
int block_to_buffer(size_t block, void* buff, size_t length){
    for (size_t i = block; i < block + length; i++){
        if(block_read(i, buff + (i - block) * BLOCK_SIZE) == -1)
            return -1;
    }
    return 0;
}

//writing buff multiple blocks
int buffer_to_block(size_t block, void* buff, size_t length){
    for (size_t i = block; i < block + length; i++){
        if(block_write(i, buff + (i - block) * BLOCK_SIZE) == -1)
            return -1;
    }
    return 0;
}

//count the number of free entries in fat table
int fat_free(){
    int count = 0;
    for (int i = 1; i < super_block.data_amount; i++){
        if (fat_array[i] == 0)
            count++;
    }
    return count;
}

//count the number of free entries in root array
int root_free(){
    int count = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (root[i].filename[0] == '\0')
            count++;
    }
    return count;
}

//find the next free entry in the fat table
int find_fat_next_free(){
    for (int i = 1; i < super_block.data_amount; i++){
        if (fat_array[i] == 0)
            return i;
    }
    return -1;
}

//check whether the filename is valid
int check_filename(const char* filename){
    bool valid_file = false;
    for (int i = 0; i < FS_FILENAME_LEN; i++){
        if (filename[i] == '\0'){
            valid_file = true;
            break;
        }
    }
    if (!valid_file)
        return -1;
    return 0;
}

//find the first free entry in the root array
//also check whether the filename exists in the root array
int check_root(const char *filename){
    bool first_empty = true;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (root[i].filename[0] == '\0'){
            if (first_empty){
                root_next_free = i;
                first_empty = false;
            }
        }else{
            if(strcmp((char*) root[i].filename, filename) == 0)
                return -1;
        }
    }
    if (first_empty)
        return -1;
    return 0;
}

//find the index of the given file in the root entry array
int find_file(const char *filename){
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (strcmp((char*) root[i].filename, filename) == 0){
            return i;
        }
    }
    return -1;
}

//calculate the number of blocks we need to read or write
int get_num_blocks(size_t count, size_t offset){
    int diff = BLOCK_SIZE - offset % BLOCK_SIZE;
    // if count fits within the current block
    if (count < diff)
        return 1;
    //count evenly fits within multiple blocks
    else if ((count - diff) % BLOCK_SIZE == 0)
        //add 1 to count for the current block
        return (count - diff) / BLOCK_SIZE + 1;
    else
        //add 2 to count for current block and the extra bytes left when we divide
        return (count - diff) / BLOCK_SIZE + 2;
}


size_t check_and_copy(char* block, char* buff, size_t count, size_t block_offset, size_t buff_offset, int fd){
    for(int j = 0; j < count; j ++){
        if (block[block_offset + j] != EOF)
            buff[buff_offset + j] = block[block_offset + j];
        else{
            buff[buff_offset + j] = EOF;
            open_files[fd].offset += j;
            return j + 1;
        }
    }
    open_files[fd].offset += count;
    return count;
}

size_t write_bytes(char* block, char* buff, size_t count, size_t block_offset, size_t buff_offset, int fd){
    bool reach_end = false;
    int fat_free_idx;
    for(int j = 0; j < count; j ++){
        if(block[block_offset + j] == EOF){
            reach_end = true;
        }
        block[block_offset + j] = buff[buff_offset + j];
    }
    if (reach_end && count < BLOCK_SIZE - block_offset){
        block[count] = EOF;
    }else if(reach_end && count == BLOCK_SIZE - block_offset){
        fat_free_idx = find_fat_next_free();
        if (fat_free_idx != -1){
            fat_array[open_files[fd].block_idx] = fat_free_idx;
            fat_array[fat_free_idx] = FAT_EOC;
            memset(block, 0, BLOCK_SIZE);
            block[0] = EOF;
            open_files[fd].block_idx = fat_array[open_files[fd].block_idx];
            block_write(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) block);
        }
    }
    open_files[fd].offset += count;
    return count;
}

//update filesize
size_t update_filesize(size_t old, size_t new){
    return new > old? new:old;
}


int fs_mount(const char *diskname)
{
    char* signature = "ECS150FS";
    
    if(block_disk_open(diskname) == -1) //disk couldn't be opened
        return -1;
    
    //read super block
    block_read(0, (void*) &super_block);
    
    //check the content of the super block
    if (memcmp((char*) &super_block.signature, signature, 8) != 0)
        return -1;
    
    if (super_block.total_amount != block_disk_count()) //superblock data doesn't match disk size (ie disk is probably corrupted or not a valid disk)
        return -1;
    
    //copy fat blocks into fat array
    fat_array = malloc(super_block.data_amount * sizeof(uint16_t));
    
    if (fat_array == NULL) //malloc failed
        return -1;
    
    uint16_t* buffer = malloc(fat_length(super_block.FAT_amount) * sizeof(uint16_t));
    if (block_to_buffer(1, buffer, super_block.FAT_amount) == -1){ //disk read failed (should never happen)
        free(buffer);
        return -1;
    }
    
    memcpy(fat_array, buffer, super_block.data_amount);
    
    //copy root entry blocks into root array
    root = malloc(FS_FILE_MAX_COUNT * sizeof(root_entry));
    if (block_read(super_block.root_idx, (void*) root) == -1){ //disk read failed (should never happen)
        free(buffer);
        return -1;
    }
    
    //initialize the file descriptor array
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
        open_files[i].root_idx = -1;
    
    free(buffer);
    
    //when finish mounting, set the mounted flag
    mounted = true;
    return 0;
}

int fs_umount(void)
{
    if (mounted == false) //disk hasn't been mounted
        return -1;
    
    //all files should be closed when unmounted
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
        if (open_files[i].root_idx != -1)
            return -1;
    
    //write fat array and root table back to disk if they have been changed
    if (change_table){
        uint16_t* buffer = malloc(fat_length(super_block.FAT_amount) * sizeof(uint16_t));
        memset(buffer, 0, fat_length(super_block.FAT_amount) * sizeof(uint16_t));
        memcpy(buffer, fat_array, super_block.data_amount);
    
        if (buffer_to_block(1, buffer, super_block.FAT_amount) == -1){ //disk write failed (should not happen)
            free(buffer);
            return -1;
        }
        
        free(buffer);
        
        if (block_write(super_block.root_idx, root) == -1){ //disk write failed (should not happen)
            return -1;
        }
    }
    
    //when finish unmounting, set the mounted flag
    mounted = false;
    //fails if disk isn't open
    return block_disk_close();
}

int fs_info(void)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
        
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", super_block.total_amount);
    printf("fat_blk_count=%d\n", super_block.FAT_amount);
    printf("rdir_blk=%d\n",super_block.root_idx);
    printf("data_blk=%d\n",super_block.data_idx);
    printf("data_blk_count=%d\n",super_block.data_amount);
    printf("fat_free_ratio=%d/%d\n",fat_free(), super_block.data_amount);
    printf("rdir_free_ratio=%d/%d\n",root_free(), FS_FILE_MAX_COUNT);
    
    return 0;
}

int fs_create(const char *filename)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    if (check_filename(filename) == -1) //filename is short enough
        return -1;
    
    if (check_root(filename) == -1) //finds next free, fails if file already exists or there is no space
        return -1;
    
    strcpy((char*) root[root_next_free].filename, filename);
    root[root_next_free].filesize = 0;
    root[root_next_free].data_start = FAT_EOC;
    
    change_table = true;
    return 0;
}

int fs_delete(const char *filename)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    if (check_filename(filename) == -1) //filename is short enough
        return -1;
    
    int pos = find_file(filename); 
    
    if (pos == -1) //file exists in root
        return -1;
    
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
        if (open_files[i].root_idx == pos) //fails if file is open
            return -1;
    
    root[pos].filename[0] = '\0';
    change_table = true;
    return 0;
}

int fs_ls(void)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    printf("FS Ls:\n");
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (root[i].filename[0] != '\0')
            printf("file: %s, size: %d, data_blk: %d\n", root[i].filename, root[i].filesize, root[i].data_start);
    }
    return 0;
}

int fs_open(const char *filename)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    if (check_filename(filename) == -1) //filename is short enough
        return -1;
    
    int pos = find_file(filename); 
    
    if (pos == -1) //file exists in root
        return -1;
    
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i ++){
        if ( open_files[i].root_idx == -1){
            open_files[i].root_idx = pos;
            open_files[i].offset = 0;
            open_files[i].block_idx = root[pos].data_start;
            return i;
        }
    }
    
    return -1; //open file table is full
}

int fs_close(int fd)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) //file descriptor is out of bounds 
        return -1;
    
    if (open_files[fd].root_idx == -1) //file descriptor points to unused entry
        return -1;
    
    open_files[fd].root_idx = -1;
    
    return 0;
}

int fs_stat(int fd)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) //file descriptor is out of bounds 
        return -1;
    
    if (open_files[fd].root_idx == -1) //file descriptor points to unused entry
        return -1;
    
    return root[open_files[fd].root_idx].filesize;
}

int fs_lseek(int fd, size_t offset)
{
    if (!mounted) //disk hasn't been mounted
        return -1;
    
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) //file descriptor is out of bounds 
        return -1;
    
    if (open_files[fd].root_idx == -1) //file descriptor points to unused entry
        return -1;
    
    if (offset > root[open_files[fd].root_idx].filesize) //offset is out of bounds
        return -1;
    
    open_files[fd].offset = offset;
    
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
    int i, fat_free_idx;
    int amount_wrote = 0;
    size_t start_offset = open_files[fd].offset;
	
    if (!mounted) //disk hasn't been mounted
        return -1;
	
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) //file descriptor is out of bounds 
        return -1;
    
    if (open_files[fd].root_idx == -1) //file descriptor points to unused entry
        return -1;
    
    char *buffer = malloc(BLOCK_SIZE*sizeof(char));
    int num_blocks = get_num_blocks(count, open_files[fd].offset);
    int diff = BLOCK_SIZE - (open_files[fd].offset % BLOCK_SIZE);
    
    if (root[open_files[fd].root_idx].data_start == FAT_EOC){
        fat_free_idx = find_fat_next_free();
        if (fat_free_idx == -1)
            return amount_wrote;
        else{
            open_files[fd].block_idx = fat_free_idx;
            root[open_files[fd].root_idx].data_start = fat_free_idx;
            fat_array[fat_free_idx] = FAT_EOC;
        }
    }
    else{
        block_read(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
    }
    
    if (count > diff){
        write_bytes(buffer, buf, diff, open_files[fd].offset % BLOCK_SIZE, 0, fd);
        amount_wrote += diff;
        block_write(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
    }
    else{
        write_bytes(buffer, buf, count, open_files[fd].offset % BLOCK_SIZE, 0, fd);
        amount_wrote += count;
        block_write(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
        root[open_files[fd].root_idx].filesize = update_filesize(root[open_files[fd].root_idx].filesize, start_offset + amount_wrote);
        
        return amount_wrote;
    }
    
    for (i = 1; i < num_blocks - 1; i++){
        
        //check if it is the last block
        if (fat_array[open_files[fd].block_idx] == FAT_EOC){
            
            fat_free_idx = find_fat_next_free();
            if (fat_free_idx == -1){
                root[open_files[fd].root_idx].filesize = update_filesize(root[open_files[fd].root_idx].filesize, start_offset + amount_wrote);
                return amount_wrote;
            }
            else{
                fat_array[open_files[fd].block_idx] = fat_free_idx;
                fat_array[fat_free_idx] = FAT_EOC;
            }
        }
        //get next block idx
        open_files[fd].block_idx = fat_array[open_files[fd].block_idx];
            
        //write the block directly into buff
        block_write(open_files[fd].block_idx + 2 + super_block.FAT_amount, buf + amount_wrote);
        open_files[fd].offset += BLOCK_SIZE;
        amount_wrote += BLOCK_SIZE;
    }
    if (num_blocks > 1){
        //check if it is the last block
        if (fat_array[open_files[fd].block_idx] == FAT_EOC){
            
            fat_free_idx = find_fat_next_free();
            if (fat_free_idx == -1){
                root[open_files[fd].root_idx].filesize = update_filesize(root[open_files[fd].root_idx].filesize, start_offset + amount_wrote);
                return amount_wrote;
            }
            else{
                fat_array[open_files[fd].block_idx] = fat_free_idx;
                fat_array[fat_free_idx] = FAT_EOC;
                memset(buffer, 0, BLOCK_SIZE);
            }
        }else{
            open_files[fd].block_idx = fat_array[open_files[fd].block_idx];
            block_read(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
        }
        write_bytes(buffer, buf, count - amount_wrote, open_files[fd].offset % BLOCK_SIZE, amount_wrote, fd);
        block_write(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
        
        amount_wrote = count;
    }
    root[open_files[fd].root_idx].filesize = update_filesize(root[open_files[fd].root_idx].filesize, start_offset + amount_wrote);
    return amount_wrote;
}

int fs_read(int fd, void *buf, size_t count)
{
    int i;
    int amount_read = 0;
    size_t res;
    
    if (!mounted) //disk hasn't been mounted
        return -1;
	
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) //file descriptor is out of bounds
        return -1;
    
    if (open_files[fd].root_idx == -1) //fd points to unused entry
        return -1;
    
    char *buffer = malloc(BLOCK_SIZE*sizeof(char));
    
    int num_blocks = get_num_blocks(count, open_files[fd].offset);
    int diff = BLOCK_SIZE - (open_files[fd].offset % BLOCK_SIZE);
    
    block_read(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
    if (count > diff){
        res = check_and_copy(buffer, buf, diff, open_files[fd].offset, 0, fd);
        if (res != diff)
            return res;
        else
            amount_read += res;
    }
    else{
        res = check_and_copy(buffer, buf, count, open_files[fd].offset, 0, fd);
        return res;
    }
        
    
    for (i = 1; i < num_blocks - 1; i++){
        //get next block idx
        open_files[fd].block_idx = fat_array[open_files[fd].block_idx];
        //check if it is the last block
        if (fat_array[open_files[fd].block_idx] == FAT_EOC){
            block_read(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
            res = check_and_copy(buffer, buf , count - amount_read, 0, amount_read, fd);
            amount_read += res;
            return amount_read;
        }
        //read the block directly into buff
        block_read(open_files[fd].block_idx + 2 + super_block.FAT_amount, buf + amount_read);
        open_files[fd].offset += BLOCK_SIZE;
        amount_read += BLOCK_SIZE;
    }
    if (num_blocks > 1){
        
        open_files[fd].block_idx = fat_array[open_files[fd].block_idx];
        block_read(open_files[fd].block_idx + 2 + super_block.FAT_amount, (void*) buffer);
        res = check_and_copy(buffer, buf , count - amount_read, 0, amount_read, fd);
        amount_read += res;
    }
    
    return amount_read;
}

