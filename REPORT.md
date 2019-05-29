
# ECS150 Project 4 Report

## Introduction

Our file system implementation based on FAT can support 128 files in a single
root directory. We use same virtual disk blocks, whose size is 4096 bytes, to 
record different parts of our file system. The superblock consists of internal
information of the file system itself. FAT table helps us to connect data 
blocks in a same file. Root entries record the basic information for each file.
Finally, there are data blocks storing the content of each file.

We define the sturct of the superblock and root entry as required and design 
all the file systems APIs using those structs.

## Mounting and Unmounting

When we mount a file system, we copy all the basic existing file system 
information, including superblock, FAT table, root entry, into our predefined 
global variables. We do several sanity checks when copy the superblock(e.g. the
name and the total block amount). Copying the FAT table would require an 
additional buffer because we can only read the whole block from our file system
and the length of the FAT table may not be perfectly divided by the the block
size. Therefore, we set a buffer, whose length is larger than that of the FAT
table and is perfectly divided by the block size. We read FAT blocks into the 
buffer and copy those valid parts into our FAT table array. For the root entry,
we read the root entry block into our global root entry array. We also 
initialize our file descriptor array and set the mounted flag to be true, which
indicates we mount successfully.

Unmounting our file system is much easier. We check whether the file system is 
mounted and all files are closed. If something is modified in the FAT table or
in the root entry array, we rewrite them into our disk. Finally, we set the 
mounted flag and the change table flag to be false.

fs_info prints some information about the mounted file system using the 
information we copied from the disk. We also write a loop to calculate the 
number of existing files and data blocks for their free ratio.

## File creation/deletion

After checking whether the disk is mounted, the given file name is valid, 
the file already exists and there are free entries, we creat a file by 
searching for the next free root entry in the root entry array. We initianize 
this entry's name to be the given file name, the size to be 0 and the start
block index to be FAT_EOC. We will allocate new data blocks to this file when
we call write.

To delete a file, we also do some sanity checks first(e.g. if the file system
is mounted, if the file name is valid, if the file exists and if the file is 
open). Then we clear the FAT table for this file and set the first character 
of the file name to be null so that this root entry can be regarded as empty.
 
###File descriptor operations

For implementing file decriptor operations, we need to define a file descriptor
struct. This struct consists of the root entry index of the file, the current 
offset, the data block index that the offset locates in and a flag to record 
whether the file perfectly ends at the end of a block. We record the data 
block index so that we need to calculate it when we read or write. The flag can 
help us avoid assigning additional empty data blocks to the end of the file.

When openning a file, we allocate a new entry in the file descriptor array, 
which points to the file's root entry. After we do the same sanity checks as 
file deletion, we search the file descriptor array from the beginning for the
first free enrty. We initialize this file descriptor entry and return the index
as our file descriptor.

To close a file using a given file descriptor, we need to check whether this 
file descriptor is valid. If so, we reset its root index so that it is free.

Finally, fs_lseek helps us to modify the offset in the file descriptor and 
fs_stat returns the file size in the file root entry after some danity checks.

###File reading/writing

For both reading and writing, we divide all data blocks into three parts (the 
first block, the last block and those middle blocks between them). We can
calculate how many blocks we are going to use based on the count and the 
current offset.

We implement a check_and_copy function for our read operation, which compares
the count and the number of rest characters in the file to make sure that we
stop when reaching the end of the file. If our read is within one block, all
we need to do is to call check_and_copy. Otherwise, we read the rest of the 
first block and call check_and_copy for our last block. For those middle 
blocks, we keep using fat table to find the next block and read the whole block
to our buffer. We also update the offset and invalid_block flag in the file 
descriptor when reading.

Writing is much more complex since we are supposed to allocate more data blocks
when we need to extend our file. For example, we need to allocate the first 
block when we are writing to a new file. If we only need to modify one block, 
our write_bytes function can help us to overwrite the file from the offset. 
Otherwise, we first need to check whether we reach the end of the file. If so, 
we should allocate more blocks for writing. For those middle blocks, we 
directly copy the whole block into our disk. When we reach the last block, 
write_bytes can help us finish writing. We keep updating the filesize, the 
current block index and the offset during our writing.

## Testing 

Our file system API passed all the testers that are given. We designed two 
additional test file and passed them too. test_fs_err.c helps us to check all 
the sanity checks in each api. test_read_write.c tests several edge cases for 
read and write. It requires some additional arguments(the disk name and three 
file name). For both read and write operations, we test reading a single block,
multiple blocks. We also test the case when offset + count > filesize and vise
versa. Results show that our implementation can pass all above test cases.

 
