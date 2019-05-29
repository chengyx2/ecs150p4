
# ECS150 Project 4 Report

## Introduction

Our file system implementation based on FAT can support 128 files in a single
root directory. We use same virtual disk blocks, whose size is 4096 bytes, to 
record different parts of our file system. The superblock consists of internal
information of the file system itself. FAT table helps us to connect data 
blocks in a same file. Root entries record the basic information for each file.
Finally, there are data blocks storing the content of each file.

The superblock and root entry structs follow the format specified in the assigment. 
We have a global super block, fat array pointer, and root entry array pointer, 
which are dynamically allocated. 

## Mounting and Unmounting

When we mount a file system, we copy all the basic existing file system 
information, including superblock, FAT table, root entry, into our predefined 
global variables. We do several sanity checks when copy the superblock (e.g. that 
the name and the total block amount are what we expect). Copying the FAT table 
would require an additional buffer because we can only read the whole block from
our file system and the length of the FAT table may not be perfectly divided by 
the the block size. Therefore, we set a buffer, whose length is larger than that 
of the FAT table and is perfectly divided by the block size. We read FAT blocks 
into the buffer and copy those valid parts into our FAT table array, then free the 
buffer. For the root entry, we read the root entry block into our global root entry 
array. We also initialize our file descriptor array and set the mounted flag to be 
true, which indicates we have mounted successfully.

Unmounting our file system is much easier. We check whether the file system is 
mounted and all files are closed. If something is modified in the FAT table or
in the root entry array, which is inidicated by a flag we set when those 
structures are modified, we rewrite them into our disk. Finally, we set the 
mounted flag and the change table flag to be false.

fs_info prints some information about the mounted file system using the 
information we copied from the disk superblock. We also run a loop to calculate the 
number of existing files and data blocks to get the free ratio for the fat and root 
entry.

## File creation/deletion

After checking whether the disk is mounted, the given file name is valid, 
the file already exists and there are free entries, we create a file by 
searching for the next free root entry in the root entry array. We initialize 
this entry's name to be the given file name, the size to be 0 and the start
block index to be FAT_EOC. We will allocate new data blocks to this file when
the user calls write on the file.

To delete a file, we also do some sanity checks first(e.g. if the file system
is mounted, if the file name is valid, if the file exists and if the file is not 
open). Then we clear the FAT table for this file by following the indices and 
setting them all to zero. We set the first character of the file name to be null 
so that this root entry can be regarded as empty when we check for empty root 
entries later.

## File descriptor operations

For implementing file decriptor operations, we need to define a file descriptor
struct. This struct consists of the root entry index of the file, the current 
offset, the data block index that the offset locates in and a flag to record 
whether the file perfectly ends at the end of a block. The last two entries are 
used as an optimization for getting the data block and allocating a new data block 
if need be. We record the data block index so that we don't need to calculate it 
or search through the fat table when we read or write. The flag can 
help us assign an additional empty data block to the end of the file the next time 
we write to the file.

When opening a file, we allocate a new entry in the file descriptor array, 
which points to the file's root entry. After we do the same sanity checks as 
file deletion, we search the file descriptor array from the beginning for the
first free enrty. We initialize this file descriptor entry and return the index
as our file descriptor.

To close a file using a given file descriptor, we need to check whether this 
file descriptor is valid. If so, we reset its root index so that it is free.

Finally, fs_lseek changes the offset of a valid file descriptor, and sets the 
end of block flag if needed. fs_stat returns the file size in the file root entry
after performing some sanity checks.

## File reading/writing

For both reading and writing, we divide the data being read into three parts: the 
first block, the last block and the middle blocks between them. We then
calculate how many blocks we are going to use based on the count and the 
current offset.

### Fs_read
For the first and last block, we read into an internal buffer, than copy the valid
data into the user's buffer. We implement a check_and_copy function for our read 
operation, which compares the count and the number of remaining characters in the 
file to make sure that we stop when reaching the end of the file, updating offset 
and returning the amount read. If our read is less than one block, all we need to 
do is to call check_and_copy then return the amount read. Otherwise, we read the 
rest of the first block and proceed to the next section. For those middle blocks, we 
go through the fat table to find the next block and read the whole block directly to 
the user's buffer. The last block is similar to the first, it gets read into our 
buffer then the remaining characters are copied to the user's buffer. We also update 
the offset and invalid_block flag in the file descriptor when reading.

### Fs_write
Writing is much more complex since we need to allocate more data blocks
when we need to extend our file. We still have a first, middle, and last block 
sections. For the first block, we check if we need to allocate a new block, 
otherwise we read the current block. Then our helper function write_bytes can copy
the user's data starting from offset into the block, then we can write it back to 
the disk. The middle blocks we can directly copy to the disk, after checking that 
a block is already allocated and allocating a new one if need be. If we can't find 
a new block we just return the amount already wrote. For the last block, if we need
to allocate a new block we do that, then zero our block buffer, then write the data
to our buffer and then to the disk. If it's not a new block we read the block from 
the disk to our buffer and then write into our buffer, then write back to the disk. 
We keep updating the filesize, the current block index and the offset during our 
writing, and set the end of block flag if needed.

## Testing 

Our file system API passed all the testers that were given by the professor. We 
designed two additional test files and passed them as well. test_fs_err.c helps us
to check all the sanity checks in each api to ensure that we catch all bad user 
behavior and have a valid environment to run the commands in. test_read_write.c 
tests several edge cases for read and write. It requires some additional arguments
(the disk name and file names of files on the disk). For both read and write 
operations, we test reading a single block and multiple blocks. We also test the 
case when offset + count > filesize and vise versa, as well as the edge case where
we write perfectly into a block. Results show that our implementation can pass all 
above test cases.

 
