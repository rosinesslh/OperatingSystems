#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"

#define TEST_ASSERT(assert)			\
do {						\
	printf("ASSERT: " #assert " ... ");	\
	if (assert) {				\
		printf("PASS\n");		\
	} else	{				\
		printf("FAIL\n");		\
		exit(1);			\
	}					\
} while(0)					\

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("Usage: %s <newly formatted disk> <file to be created>\n", argv[0]);
		return -1;
	}
	//umount with no disk
	TEST_ASSERT(fs_umount() == -1);
	//mount disk
	TEST_ASSERT(fs_mount(argv[1]) == 0);
	//open nonexistent file
	TEST_ASSERT(fs_open("file1") == -1);
	//delete nonexistent file
	TEST_ASSERT(fs_delete("file2") == -1);
	//create file
	TEST_ASSERT(fs_create(argv[2]) == 0);
	//open file
	int fd = fs_open(argv[2]);
	TEST_ASSERT(fd != -1);
	//umount with open file
	TEST_ASSERT(fs_umount() == -1);
	//lseek with empty file
	TEST_ASSERT(fs_lseek(fd, 20) == -1);
	//write 24 bytes
	TEST_ASSERT(fs_write(fd, (void*)"This is a line of text.", 24) == 24);
	char buf[6];
	//set offset
	TEST_ASSERT(fs_lseek(fd, 18) == 0);
	//read 6 bytes
	TEST_ASSERT(fs_read(fd, buf, 6) == 6);
	//test for successful read
	TEST_ASSERT(!strcmp(buf, "text."));
	//close file
	TEST_ASSERT(fs_close(fd) == 0);
	//delete file
	TEST_ASSERT(fs_delete(argv[2]) == 0);
	//umount disk
	TEST_ASSERT(fs_umount() == 0);
	
	return 0;
}
