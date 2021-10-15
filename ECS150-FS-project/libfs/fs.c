#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define MAXI_SIZE 4096

//memory layout of the superblock
struct __attribute__ ((__packed__)) superblock {
	uint8_t signature[8];
	uint16_t total_block_amount;
	uint16_t root_block_index;
	uint16_t data_block_start_index;
	uint16_t data_block_amount;
	uint8_t fat_block_count;
	uint8_t padding[4079];
};
//memory layout of an individual file entry
struct __attribute__ ((__packed__)) fileentry {
	uint8_t filename[FS_FILENAME_LEN];
	uint32_t file_size;
	uint16_t first_data_block_index;
	uint8_t padding[10];
};
//open file
struct openfile {
	struct fileentry *file;
	uint32_t offset;
};

static struct superblock *super_block;
static uint16_t *FAT; //array to be size data block amount
static struct fileentry *rootdirectory; //array to have size 128
static uint16_t num_free_data_blocks;
static int num_empty_entries;
static struct openfile *openfile_table;

int fs_mount(const char *diskname)
{
	super_block = (struct superblock*) malloc(sizeof(struct superblock));
	rootdirectory = (struct fileentry*) malloc(FS_FILE_MAX_COUNT * sizeof(struct fileentry));
	openfile_table = (struct openfile*) malloc(FS_OPEN_MAX_COUNT * sizeof(struct openfile));

	int dopenret = block_disk_open(diskname);

	int readret1 = block_read(0, super_block);
	//check for failed operations
	if (!super_block || !rootdirectory || !openfile_table || dopenret == -1 || readret1 == -1 || block_disk_count() != super_block->total_block_amount) {
		fs_umount();
		return -1;
	}
	//check for signature
	uint8_t fmt[8] = {'E', 'C', 'S', '1', '5', '0', 'F', 'S'};
	for (int i = 0; i < 8; i++) {
		if (fmt[i] != super_block->signature[i]) {
			fs_umount();
			return -1;
		}
	}

	FAT = (uint16_t*) malloc(super_block->fat_block_count * sizeof(struct superblock));

	int readret2 = block_read(super_block->root_block_index, rootdirectory);
	//check for failed operations
	if (!FAT || readret2 == -1) {
		fs_umount();
		return -1;
	}
	//fill FAT
	for (unsigned int i = 1; i <= super_block->fat_block_count; i++) {
		int readret = block_read(i, FAT + (i - 1)*(MAXI_SIZE / 2));
		//check for failed read
		if (readret == -1) {
			fs_umount();
			return -1;
		}
	}
	//count number of free data blocks
	num_free_data_blocks = super_block->data_block_amount;
	for (uint16_t i = 0; i < super_block->data_block_amount; i++) {
		if (FAT[i] != 0) {
			num_free_data_blocks--;
		}
	}
	//count number of empty file entries
	num_empty_entries = FS_FILE_MAX_COUNT;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (rootdirectory[i].filename[0] != '\0') {
			num_empty_entries--;
		}
	}
	//empty fd table
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		openfile_table[i].file = NULL;
		openfile_table[i].offset = 0;
	}

	return 0;
}

int fs_umount(void)
{
	if (!super_block) return -1;
	//check for open file
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (openfile_table[i].file)  return -1;
	}
	int ret = block_disk_close();
	if (ret == -1) return -1;
	//reset data
	free(super_block);
	free(FAT);
	free(rootdirectory);
	free(openfile_table);
	num_free_data_blocks = 0;
	num_empty_entries = 0;
	super_block = NULL;
	FAT = NULL;
	rootdirectory = NULL;

	return 0;
}

int fs_info(void)
{
	if (!super_block) return -1;
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", super_block->total_block_amount);
	printf("fat_blk_count=%d\n", super_block->fat_block_count);
	printf("rdir_blk=%d\n", super_block->root_block_index);
	printf("data_blk=%d\n", super_block->data_block_start_index);
	printf("data_blk_count=%d\n", super_block->data_block_amount);
	printf("fat_free_ratio=%d/%d\n", num_free_data_blocks, super_block->data_block_amount);
	printf("rdir_free_ratio=%d/%d\n", num_empty_entries, FS_FILE_MAX_COUNT);
	return 0;
}

int fs_create(const char *filename)
{
	if (!super_block) return -1;
	//check for space
	if (num_empty_entries < 1) return -1;
	int len = strlen(filename) + 1;
	//check for appropriate length
	if (!(len >= 2 && len <= FS_FILENAME_LEN)) {
		return -1;
	}
	//check if filename exists
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp((char*)rootdirectory[i].filename, filename)) {
			return -1;
		}
	}

	//identify empty file entry
	int index;
	for (index = 0; rootdirectory[index].filename[0] != '\0'; index++);
	//fill entry
	strcpy((char*)rootdirectory[index].filename, filename);
	rootdirectory[index].file_size = 0;
	rootdirectory[index].first_data_block_index = FAT_EOC;
	//write root block to disk
	block_write(super_block->root_block_index, rootdirectory);
	num_empty_entries--;

	return 0;
}

int fs_delete(const char *filename)
{
	if (!super_block) return -1;
	//check if filename exists
	int index = -1;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp((char*)rootdirectory[i].filename, filename)) {
			index = i;
			break;
		}
	}
	if (index == -1) return -1;
	//need to check if file is open and return -1 if so
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (openfile_table[i].file && !strcmp((char*)openfile_table[i].file->filename, filename))  return -1;
	}
	//update data
	uint16_t bindex = rootdirectory[index].first_data_block_index;
	while (bindex != FAT_EOC) {
		uint16_t next = FAT[bindex];
		FAT[bindex] = 0;
		bindex = next;
		num_free_data_blocks++;
	}
	rootdirectory[index].filename[0] = '\0';
	num_empty_entries++;

	//write to disk
	for (uint8_t i = 1; i <= super_block->fat_block_count; i++) {
		block_write(i, FAT + (i - 1)*(MAXI_SIZE / 2));
	}
	block_write(super_block->root_block_index, rootdirectory);

	return 0;
}

int fs_ls(void)
{
	if (!super_block) return -1;

	printf("FS Ls:\n");
	//print information for all files
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (rootdirectory[i].filename[0] != '\0') {
			printf("file: %s, size: %d, data_blk: %d\n", rootdirectory[i].filename, rootdirectory[i].file_size, 
			rootdirectory[i].first_data_block_index);
		}
	}

	return 0;
}


int fs_open(const char *filename)
{
	if (!super_block) return -1;

	//find entry
	int index = -1;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (!strcmp((char*)rootdirectory[i].filename, filename)) {
			index = i;
			break;
		}
	}
	if (index == -1) return -1;
	//find empty spot in fd table
	int tbindex = -1;
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		if (!openfile_table[i].file)  {
			tbindex = i;
			break;
		}
	}
	if (tbindex == -1) return -1;
	//update fd table
	openfile_table[tbindex].file = &rootdirectory[index];
	openfile_table[tbindex].offset = 0;

	return tbindex;
}

int fs_close(int fd)
{
	if (!super_block || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !openfile_table[fd].file) return -1;

	openfile_table[fd].file = NULL;
	openfile_table[fd].offset = 0;

	return 0;
}

int fs_stat(int fd)
{
	if (!super_block || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !openfile_table[fd].file) return -1;

	return openfile_table[fd].file->file_size;
}

int fs_lseek(int fd, size_t offset)
{
	if (!super_block || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !openfile_table[fd].file || offset > openfile_table[fd].file->file_size) return -1;
	openfile_table[fd].offset = offset;
	return 0;
}
/*
need a function that returns the index of the data block 
corresponding to the file’s offset.
*/
static uint16_t index_check(int fd)
{
	uint16_t index = 0;
	char* name = (char *)openfile_table[fd].file->filename;

	for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(!strcmp((char *)rootdirectory[i].filename, name))
		{
			index = rootdirectory[i].first_data_block_index;
			for (int j = 0; j < (int) (openfile_table[fd].offset / MAXI_SIZE); j++) {
				index = FAT[index];
			}
			break;
		}
	}

	return index;

}

/*
a function that allocates a new data block and link 
it at the end of the file’s data block chain.
*/
static int new_block(int fd)
{
	if (!super_block || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !openfile_table[fd].file) return -1;
	int curr = 0;
	int index = 0;
	if(num_free_data_blocks > 0)
	{
		for (index = 0; FAT[index] != 0; index++);
		for (curr = openfile_table[fd].file->first_data_block_index; (curr != FAT_EOC) && (FAT[curr] != FAT_EOC); curr = FAT[curr]);
		if (curr == FAT_EOC) {
			openfile_table[fd].file->first_data_block_index = index;
		} else {
			FAT[curr] = index;
		}
		FAT[index] = FAT_EOC;
		num_free_data_blocks--;
	}
	else 
		return -1;
	return index;
}


int fs_write(int fd, void *buf, size_t count)
{
	
	if (!super_block || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !openfile_table[fd].file) return -1;
	if (count < 1) return 0;
	if (openfile_table[fd].file->first_data_block_index == FAT_EOC) {
		new_block(fd);
	}
	int start = openfile_table[fd].offset / MAXI_SIZE;
	int end = (openfile_table[fd].offset + count) / MAXI_SIZE;
	char bounce[(end - start + 1) * MAXI_SIZE];
	fs_read(fd, bounce, count);
	uint32_t numwrite = 0;
	uint32_t offset = openfile_table[fd].offset;
	uint16_t index = index_check(fd);
	//block_read(index, bounce);
	int file_des = fd;

	if(count < MAXI_SIZE - offset)
	{
		memcpy((bounce+offset), buf, count);
		block_write(index+super_block->data_block_start_index, (bounce-offset));
		numwrite = count;
	}
	
	if(count > MAXI_SIZE-offset){
		int remain_count = count - (MAXI_SIZE - offset);
		int remain_block = remain_count / MAXI_SIZE;
		int left_bytes = remain_count % MAXI_SIZE;

		memcpy((bounce+offset), buf, MAXI_SIZE - offset);
		block_write(index+super_block->data_block_start_index, (bounce-offset));
		numwrite = MAXI_SIZE - offset;
		buf += MAXI_SIZE - offset;

		for (int i = 0; i < remain_block; i++)
		{
			if(num_free_data_blocks > 0){
				index = index_check(file_des);
				int new_index = FAT[new_block(index)];
				block_write(new_index+super_block->data_block_start_index, buf + MAXI_SIZE);
				file_des = new_index;
				buf += MAXI_SIZE;
				numwrite += MAXI_SIZE;

			}
		}
		//for remainders
		if(num_free_data_blocks > 0){
			index = index_check(file_des);
			int new_index = FAT[new_block(index)];
			block_write(new_index+super_block->data_block_start_index,buf);
			numwrite += left_bytes;
		}

	}

	if (offset + count > openfile_table[fd].file->file_size) {
		openfile_table[fd].file->file_size = offset + count;
	}

	block_write(super_block->root_block_index, rootdirectory);
	for (uint8_t i = 1; i <= super_block->fat_block_count; i++) {
		block_write(i, FAT + (i - 1)*(MAXI_SIZE / 2));
	}
	openfile_table[fd].offset += numwrite;

	return numwrite;
}

int fs_read(int fd, void *buf, size_t count)
{

	if (!super_block || fd < 0 || fd >= FS_OPEN_MAX_COUNT || !openfile_table[fd].file) return -1;
	int start = openfile_table[fd].offset / MAXI_SIZE;
	int end = (openfile_table[fd].offset + count) / MAXI_SIZE;
	uint16_t blk_cnt = end - start + 1;
	uint8_t bounce[(blk_cnt) * MAXI_SIZE];
	for (uint32_t c = 0; c < (blk_cnt) * MAXI_SIZE; c++) {
		bounce[c] = '\0';
	}
	uint32_t numread = 0;
	uint32_t offset = openfile_table[fd].offset;
	uint16_t index = index_check(fd);

	for (uint16_t i = 0; i < blk_cnt && index != FAT_EOC; i++) {
		block_read(index+super_block->data_block_start_index, bounce + (i*MAXI_SIZE));
		index = FAT[index];
	}
	uint8_t *ibuf = (uint8_t*) buf;
	for (uint32_t i = 0; i < count && bounce[i + offset] != '\0'; i++) {
		ibuf[i] = bounce[i + offset];
		numread++;
	}
	openfile_table[fd].offset += numread;

	return (int)numread;
}
