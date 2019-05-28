#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fs.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define test_fs_error(fmt, ...) \
	fprintf(stderr, "%s: "fmt"\n", __func__, ##__VA_ARGS__)

#define die(...)				\
do {							\
	test_fs_error(__VA_ARGS__);	\
	exit(1);					\
} while (0)

#define die_perror(msg)			\
do {							\
	perror(msg);				\
	exit(1);					\
} while (0)


struct thread_arg {
	int argc;
	char **argv;
};

void test_unmounted(char* filename)
{
    char buffer[10];
    
    assert(fs_umount() == -1);
    assert(fs_info() == -1);
    assert(fs_create(filename) == -1);
    assert(fs_delete(filename) == -1);
    assert(fs_open(filename) == -1);
    assert(fs_close(1) == -1);
    assert(fs_stat(1) == -1);
    assert(fs_lseek(1, 10) == -1);
    assert(fs_write(1, buffer, 10) == -1);
    assert(fs_read(1, buffer, 10) == -1);
    
}

void test_filename_valid(char* diskname){

    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    assert(fs_create("longlonglonglonglonglongfilename") == -1);
    assert(fs_create("") == -1);
    
    assert(fs_delete("longlonglonglonglonglongfilename") == -1);
    assert(fs_open("longlonglonglonglonglongfilename") == -1);
    
    if (fs_umount())
        die("cannot unmount diskname");
    
}

void test_valid_file_descriptor(char* diskname){
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    char buffer[10];
    memset(buffer, 0, 10);
    
    assert(fs_close(-1) == -1);
    assert(fs_close(32) == -1);
    assert(fs_close(0) == -1);
    
    assert(fs_stat(-1) == -1);
    assert(fs_stat(32) == -1);
    assert(fs_stat(0) == -1);
    
    assert(fs_lseek(-1, 0) == -1);
    assert(fs_lseek(32, 0) == -1);
    assert(fs_lseek(0, 0) == -1);
    
    assert(fs_write(-1, buffer, 10) == -1);
    assert(fs_write(32, buffer, 10) == -1);
    assert(fs_write(0, buffer, 10) == -1);
    
    assert(fs_read(-1, buffer, 10) == -1);
    assert(fs_read(32, buffer, 10) == -1);
    assert(fs_read(0, buffer, 10) == -1);
    
    if (fs_umount())
        die("cannot unmount diskname");
    
}



void test_file_exist(char* diskname, char *filename, char *bad_filename){
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    assert(fs_create(filename) == -1);
    
    assert(fs_delete(bad_filename) == -1);
    
    assert(fs_open(bad_filename) == -1);
    
    if (fs_umount())
        die("cannot unmount diskname");
}

void test_open_files(char* diskname, char* filename){
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    int fd = fs_open(filename);
    
    assert(fs_delete(filename) == -1);
    
    assert(fs_umount() == -1);
    
    fs_close(fd);
    
    if (fs_umount())
        die("cannot unmount diskname");
}

void test_open_full(char* diskname, char* filename){
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++){
        assert(fs_open(filename) == i);
    }
    assert(fs_open(filename) == -1);
    
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++){
        assert(fs_close(i) == 0);
    }
    
    if (fs_umount())
        die("cannot unmount diskname");
}

void test_lseek(char* diskname, char* filename){
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    int fd = fs_open(filename);
    assert(fs_lseek(fs_stat(fd) + 1, 0) == -1);
    
    fs_close(fd);
    
    if (fs_umount())
        die("cannot unmount diskname");
}

int main(int argc, char **argv)
{
    char *bad_filename = "filenotexist";
    char* diskname = argv[1];
    char* filename = argv[2];

    test_unmounted(filename);
    
    test_filename_valid(diskname);
    
    test_valid_file_descriptor(diskname);
    
    test_file_exist(diskname, filename, bad_filename);
    
    test_open_files(diskname, filename);
    
    test_open_full(diskname, filename);
    
    test_lseek(diskname, filename);
    
    return 0;
}
