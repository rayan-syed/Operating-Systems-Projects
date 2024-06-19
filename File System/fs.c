#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include "fs.h"
#include "disk.h"

#define MAX_FD 32
#define MAX_FILES 64

struct super_block {
	uint16_t used_block_bitmap_count;
	uint16_t used_block_bitmap_offset;
	uint16_t inode_metadata_blocks;
	uint16_t inode_metadata_offset;
	uint16_t dir_offset;
};

struct inode {
	uint16_t direct_offset;
	uint16_t single_indirect_offset;
	uint64_t file_size;
	int init;
};

struct entry {
	char name [16];		//max length 16
	bool used;
	int inode_num;	
};

struct fd {
	bool used;
	int inode_num;	//first block
	int offset;
};

uint8_t bitmap[DISK_BLOCKS/CHAR_BIT];
struct inode inodes[MAX_FILES];	//inode table for 64 files
struct super_block superblock;	
struct fd fds[MAX_FD];	//max 32 fds
struct entry dir[MAX_FILES]; 	//directory data for 64 files

bool mounted = false;		//boolean to see if its been mounted


int nextopen() {	
	int count = 0;
	for(int i = 0; i < DISK_BLOCKS/CHAR_BIT; i++)
	{
		uint8_t temp = bitmap[i];
		//255 means chunk full
		if(temp == 255) 
		{
			count+=CHAR_BIT;
			continue;
		}
		else temp = bitmap[i];
		//keep shifting left until most signifcant bit is 1
		while((temp & 1) != 0) 
		{
			temp >>= 1;
			count++;
		}
		break;
	}
	return count;	//should be the next open bit in bitmap
}
void set(int bit) {
	int bytei = bit/CHAR_BIT;
	int biti = bit % CHAR_BIT;
	bitmap[bytei] |= (uint8_t)(1 << biti);
}
void clear(int bit) {
	int bytei = bit/CHAR_BIT;
	int biti = bit%CHAR_BIT;
	uint8_t temp = ~(1<<biti);
	bitmap[bytei] &= temp;
}

int make_fs(const char *disk_name){
	//make disk
	if(mounted) return -1;
	if(make_disk(disk_name) ==-1) return -1;
	if(open_disk(disk_name)==-1) return -1;

	//superblock
	superblock.used_block_bitmap_count = 4;
	superblock.used_block_bitmap_offset = 1;
	superblock.inode_metadata_blocks = 1;
	superblock.inode_metadata_offset = 2;
	superblock.dir_offset = 3;

	//initalize bitmap
	memset(bitmap, 0, sizeof(bitmap));
	set(0);
	set(1);
	set(2);
	set(3);

	//traverse bits
	//for(int i = 0; i < superblock.inode_metadata_offset; i++)
	//	//or the used status with the bit being checked, the result is saved with |=
	//	bitmap[i/CHAR_BIT] |= (1<< (i%CHAR_BIT));	//i%CHAR_BIT is bitmask for bit index

	//initialize inode table
	for(int i = 0; i < MAX_FILES; i++) {
		inodes[i].file_size = 0;
		inodes[i].init = 0;
	}

	//init dir array
	for(int i = 0; i < MAX_FILES; i++) {
		memset(dir[i].name, 0, sizeof(dir->name));
		dir[i].used = false;
		dir[i].inode_num = -1;
	}

	//init fd
	for(int i = 0; i < MAX_FD; i++) {
		fds[i].used = 0;
		fds[i].offset = 0;
		fds[i].inode_num = -1;
	}

	//write superblock, bitmap, fd
	char temp [BLOCK_SIZE];
	memset(temp, 0, sizeof(temp));
	memcpy(temp, &superblock, sizeof(superblock));
	if(block_write(0, temp) == -1) return -1;
	memset(temp, 0, sizeof(temp));
	memcpy(temp, bitmap, sizeof(bitmap));
	if(block_write(superblock.used_block_bitmap_offset, temp) == -1) return -1;
	memset(temp, 0, sizeof(temp));
	memcpy(temp, inodes, sizeof(inodes));
	if(block_write(superblock.inode_metadata_offset, temp)==-1) return -1;
	memset(temp, 0, sizeof(temp));
	memcpy(temp, dir, sizeof(dir));
	if(block_write(superblock.dir_offset, temp) == -1) return -1;

	//close disk
	if(close_disk() == -1) return -1;

	return 0;
}
int mount_fs(const char *disk_name){
	//error check
	if(mounted) return -1;
	if(open_disk(disk_name)==-1) return -1;

	//read superblock&block bitmap&inode&dir
	char temp [BLOCK_SIZE];
	if(block_read(0, temp)==-1) return -1;
	memcpy(&superblock, temp, sizeof(superblock));
	if(block_read(superblock.used_block_bitmap_offset, temp) == -1) return -1;
	memcpy(bitmap, temp, sizeof(bitmap));
	if(block_read(superblock.inode_metadata_offset, temp) == -1) return -1;
	memcpy(inodes, temp, sizeof(inodes));
	if(block_read(superblock.dir_offset, temp) == -1) return -1;
	memcpy(dir, temp, sizeof(dir));

	//now mounted
	mounted=true;

	return 0;
}

int umount_fs(const char *disk_name){
	if(!mounted) return -1;

	//write everything	
	char temp [BLOCK_SIZE];
	memset(temp, 0, sizeof(temp));
	memcpy(temp, &superblock, sizeof(superblock));
	if(block_write(0, temp) == -1) return -1;	
	memset(temp, 0, sizeof(temp));
	memcpy(temp, bitmap, sizeof(bitmap));
	if(block_write(superblock.used_block_bitmap_offset, temp) == -1) return -1;	
	memset(temp, 0, sizeof(temp));
	memcpy(temp, inodes, sizeof(inodes));
	if(block_write(superblock.inode_metadata_offset, temp)==-1) return -1;
	memset(temp, 0, sizeof(temp));
	memcpy(temp, dir, sizeof(dir));
	if(block_write(superblock.dir_offset, temp) == -1) return -1;

	for(int i = 0; i < MAX_FD; i++)
	{
		dir[i].used = 0;
		fds[i].used = 0;
	}

	//no longer mounted
	mounted = false;

	if(close_disk()==-1) return -1;

	return 0;
}

int fs_open(const char *name){
	if(mounted == 0) return -1;

	//find in dir
	int j;
	for(j = 0; j < MAX_FILES; j++)
		if(strcmp(dir[j].name, name) ==0) break;
	if(j == MAX_FILES) return -1;	//return -1 if not there

	if(strlen(name) > 16) return -1;

	//find free fd
	int i;
	for(i = 0; i < MAX_FD; i++)
		if(fds[i].used == 0) break;
	if(i == MAX_FD) return -1;	//cant have more than 32 fd

	//put in array
	fds[i].used = 1;
	fds[i].inode_num = j;
	fds[i].offset = 0;

	return i;
}

int fs_close(int fildes){
	if(mounted == 0) return -1;
	if(fildes > MAX_FD || fildes < 0) return -1;
	if(fds[fildes].used==0 || fds[fildes].inode_num == -1) return -1;

	fds[fildes].used = 0;
	fds[fildes].inode_num = -1;
	fds[fildes].offset=0;

	return 0;
}

int fs_create(const char *name){
	if(mounted == 0) return -1;
	//make sure name doesnt exist+not too big
	for(int i = 0; i<MAX_FILES; i++)
		if(strcmp(dir[i].name, name) == 0)
			return -1;
	if(strlen(name)>16) return -1;

	//find free spot in directory
	int free = -1; 
	for(int i = 0; i<MAX_FILES; i++)
		if(!dir[i].used)
		{
			free = i;
			break;
		}
	if(free==-1) return -1;
	
	//add to dir
	strcpy(dir[free].name,name);
	dir[free].used = true;
	dir[free].inode_num = free;

	//inode metadata
	inodes[free].direct_offset = 0;
	inodes[free].single_indirect_offset = 0;
	inodes[free].file_size = 0;
	inodes[free].init = 0;

	return 0;
}

int fs_delete(const char *name){
	if(mounted==0) return -1;
	if(strlen(name) > 16) return -1;

	//make sure name exists
	int i;
	for(i = 0; i< MAX_FILES; i++)
		if(strcmp(dir[i].name, name) == 0)
			break;
	if(i == MAX_FILES) return -1;

	//find fd with inode num
	for(int j = 0; j < MAX_FD; j++)
		if(i == fds[j].inode_num)
			if(fds[j].used) return -1;	//cant delete open file

	//wipe bitmap blocks
	int block = inodes[i].direct_offset;
	for(int j = 0; j<inodes[i].file_size;j++)
	{
		clear(block+j);
	}

	//clear dir & inode info
	memset(dir[i].name,0,sizeof(dir->name));
	dir[i].used = false;
	dir[i].inode_num = -1;
	inodes[i].direct_offset = 0;
	inodes[i].single_indirect_offset = 0;
	inodes[i].file_size = 0;
	inodes[i].init = 0;
	return 0;
}

int fs_read(int fildes, void *buf, size_t nbyte) {
	if(mounted == 0) return -1;
	if(fildes<0 || fildes >= MAX_FD) return -1;
	if(fds[fildes].used==0) return -1;
	if(buf==NULL) return -1;
	if(nbyte<0) return -1;
	if(fds[fildes].offset<0 || fds[fildes].offset > inodes[fds[fildes].inode_num].file_size) return -1;
	if(fds[fildes].offset == inodes[fds[fildes].inode_num].file_size) return 0;

	int inode = fds[fildes].inode_num;
	
	//need to read from inode offset -> filesize
	int blockoffset = inodes[inode].file_size  % BLOCK_SIZE;

	//read
	int read = 0;
	char *buffer = (char *)buf;
	int block = inodes[inode].direct_offset;
	int inc = 0;
	int starting_offset = fds[fildes].offset%BLOCK_SIZE;
	bool first = true;
	while(read < nbyte && fds[fildes].offset < inodes[inode].file_size) {
		if(1)
			block = block+ inc;
		else;

		//read block		
		char temp[BLOCK_SIZE];
		if(block_read(block, temp) == -1) return -1;

		//copy whole block to buffer
		int copy = BLOCK_SIZE;
		//unless the block we r on is the last block
		if(block/BLOCK_SIZE == inodes[inode].file_size/BLOCK_SIZE) copy = blockoffset;
		if(copy > nbyte - read) copy = nbyte - read;
		if(first) 
		{
			memcpy(buffer+read, temp+starting_offset,copy);
			first = false;
		}
		else memcpy(buffer+read, temp, copy);

		//updates
		read+=copy;
		inc++;

		if(copy < BLOCK_SIZE || read >= nbyte || read >= inodes[inode].file_size) break;
	}

	return read;
}

int fs_write(int fildes, void *buf, size_t nbyte) {
	if(!mounted) return -1;
	if(fildes < 0 || fildes >= MAX_FD) return -1;
	if(fds[fildes].used == 0) return -1;	

	int inode = fds[fildes].inode_num;
	int blockoffset = fds[fildes].offset % BLOCK_SIZE;	//offset already within current block	
	//new blocks needed
	int newblocks = (nbyte+blockoffset)/BLOCK_SIZE+1;

	//assign needed blocks
	if(inodes[inode].init==0)
	{
		inodes[inode].direct_offset = nextopen();
		for(int i = 0; i < newblocks; i++)
		{
			set(inodes[inode].direct_offset+i);
		}
		inodes[inode].init=1;
	}
	int written= 0;	//keep track of how many bytes written
	int blocks = inodes[inode].direct_offset + fds[fildes].offset / BLOCK_SIZE;	//get current block

	for(int i = 0; i < newblocks; i++)
	{
		//end of disk
		if(blocks >= DISK_BLOCKS) return -1;	
		//direct 
		if(blocks >= inodes[inode].direct_offset/* && blocks < inodes[inode].direct_offset + 256*/) {
			uint8_t temp[BLOCK_SIZE];	//block to write
			if(block_read(blocks, temp) == -1) return -1;
			int writing = BLOCK_SIZE - blockoffset;	//start by only writing to the first block's leftovers (or start of next)
			if(writing > nbyte) writing = nbyte;	//if bytes that can be written is greater than bytes to write, set to lower
			memcpy(temp + blockoffset, buf, writing);	//cpy from buff into the block
			if(block_write(blocks, temp) == -1) return -1;	//write the block back
			written += writing;		//keep track of how many bytes written total
			nbyte-=writing;			//nbyte only has whatever is left to write
			inodes[inode].file_size += writing; //update file size
			blockoffset = 0;		//offset 0 for all blocks after first since block empty
		}
		//indirect
		else;
		
		//update bitmap + next block
		blocks++;
	}

	//write bitmap back
	if(block_write(superblock.used_block_bitmap_offset, bitmap) ==-1) return -1;

	//return bytes written
	return written;
}

int fs_get_filesize(int fildes){
	if(mounted == 0) return -1;
	if(fildes < 0 || fildes >= MAX_FD) return -1;
	if(fds[fildes].used == 0) return -1;

	return inodes[fds[fildes].inode_num].file_size;
}

int fs_listfiles(char ***files){
	if(files == NULL) return -1;

	//malloc
	*files = malloc(MAX_FILES * sizeof(char*));

	//init files
	for(int i = 0; i < MAX_FILES; i++)
		(*files)[i] = NULL;

	//copy file names
	for(int i = 0; i < MAX_FILES; i++)
	{
		if(dir[i].used == 1)
		{	
			//malloc name
			(*files)[i] = malloc(16);
			strcpy((*files)[i], dir[i].name);
		}
	}

	//terminate array
	(*files)[65] = NULL;

	return 0;
}

int fs_lseek(int fildes, off_t offset){
	if(mounted==0) return -1;
	if(fildes < 0 || fildes > MAX_FD) return -1;
	if(fds[fildes].used == 0) return -1;
	if(offset <0 || offset > inodes[fds[fildes].inode_num].file_size) return -1;

	//update offset after error checking
	fds[fildes].offset = offset;
	return 0;
}


int fs_truncate(int fildes, off_t length){
	if(!mounted) return -1;
	if(fildes<0 || fildes >=MAX_FD) return -1;
	if(fds[fildes].used ==0) return -1;
	if(length <0) return -1;

	int inode = fds[fildes].inode_num;
	int size = inodes[inode].file_size;
	
	if(length>size) return -1;
	if(size==length) return 0;

	if(length<size)
	{
		int delete = size-length;
		for(int i = delete; i < size; i++)
		{
			clear(inodes[inode].direct_offset + i);
		}
		inodes[inode].file_size = length;
	}

	return 0;
}
