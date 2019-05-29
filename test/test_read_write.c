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

void test_write(char* diskname, char* filename, size_t offset){
    char* buf = "bbbb";
    int len = 4;
    int fs_fd;
    int written;
    
    
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    fs_fd = fs_open(filename);
    
    
    if (fs_fd < 0) {
        fs_umount();
        die("Cannot open file");
    }
    
    fs_lseek(fs_fd, offset);
    written = fs_write(fs_fd, buf, len);
    
    assert(written == 4);
    
    if (fs_close(fs_fd)) {
        fs_umount();
        die("Cannot close file");
    }
    
    
    if (fs_umount())
        die("Cannot unmount diskname");
}

void test_write_perfect(char* diskname, char*filename){
    int fs_fd = -1;
    int written;
    char *buffer;
    int len = 4096;
    
    buffer = malloc(len*sizeof(char));
    memset(buffer, 'a', len);
    
    
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    fs_fd = fs_open(filename);
    
    
    if (fs_fd < 0) {
        fs_umount();
        die("Cannot open file");
    }
    
    //fs_lseek(fs_fd, offset);
    written = fs_write(fs_fd, buffer, len);

    assert(written == len);
    
    if (fs_close(fs_fd)) {
        fs_umount();
        die("Cannot close file");
    }
    
    if (fs_umount())
        die("Cannot unmount diskname");
}


void test_read(char* diskname, char* filename, size_t offset){
    char* buf;
    int len = 4;
    int fs_fd;
    int read;
    
    buf = malloc(len* sizeof(char));
    if (fs_mount(diskname))
        die("Cannot mount diskname");
    
    fs_fd = fs_open(filename);
    
    
    if (fs_fd < 0) {
        fs_umount();
        die("Cannot open file");
    }
    
    fs_lseek(fs_fd, offset);
    read = fs_read(fs_fd, buf, len);
    
    assert(read == 4);
    assert(memcmp(buf, "cdef",len) == 0);
    
    if (fs_close(fs_fd)) {
        fs_umount();
        die("Cannot close file");
    }
    
    
    if (fs_umount())
        die("Cannot unmount diskname");
}


int main(int argc, char **argv)
{
    char* diskname = argv[1];
    char* filename = argv[2];
    char* filename2 = argv[3];
    char* filename3 = argv[4];
    
    test_write(diskname, filename, 2);
    
    test_write_perfect(diskname, filename2);
    
    test_read(diskname, filename3, 2);
    
    return 0;
}
