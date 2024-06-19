/* These tests are derived from autograding tools originally created by Manuel
 * Egele for EC 440 at Boston University. Permission has been granted to use
 * these tests in the EC 440 course taught by Orran Krieger. Contact both
 * professors before reusing this code elsewhere.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "disk.h"

/******************************************************************************/
static int active = 0; /* is the virtual disk open (active) */
static int handle; /* file handle to virtual disk       */
/******************************************************************************/

int make_disk(const char *name)
{
	int f, cnt;
	char buf[BLOCK_SIZE];

	if (!name) {
		fprintf(stderr, "make_disk: invalid file name\n");
		return -1;
	}

	if ((f = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		perror("make_disk: cannot open file");
		return -1;
	}

	memset(buf, 0, BLOCK_SIZE);
	for (cnt = 0; cnt < DISK_BLOCKS; ++cnt) {
		if (write(f, buf, BLOCK_SIZE) < 0) {
			perror("make_disk: failed to write");
			return -1;
		}
	}

	close(f);

	return 0;
}

int open_disk(const char *name)
{
	int f;

	if (!name) {
		fprintf(stderr, "open_disk: invalid file name\n");
		return -1;
	}

	if (active) {
		fprintf(stderr, "open_disk: disk is already open\n");
		return -1;
	}

	if ((f = open(name, O_RDWR, 0644)) < 0) {
		perror("open_disk: cannot open file");
		return -1;
	}

	handle = f;
	active = 1;

	return 0;
}

int close_disk()
{
	if (!active) {
		fprintf(stderr, "close_disk: no open disk\n");
		return -1;
	}

	close(handle);

	active = handle = 0;

	return 0;
}

int block_write(int block, const void *buf)
{
	if (!active) {
		fprintf(stderr, "block_write: disk not active\n");
		return -1;
	}

	if ((block < 0) || (block >= DISK_BLOCKS)) {
		fprintf(stderr, "block_write: block index out of bounds\n");
		return -1;
	}

	if (lseek(handle, block * BLOCK_SIZE, SEEK_SET) < 0) {
		perror("block_write: failed to lseek");
		return -1;
	}

	if (write(handle, buf, BLOCK_SIZE) < 0) {
		perror("block_write: failed to write");
		return -1;
	}

	return 0;
}

int block_read(int block, void *buf)
{
	if (!active) {
		fprintf(stderr, "block_read: disk not active\n");
		return -1;
	}

	if ((block < 0) || (block >= DISK_BLOCKS)) {
		fprintf(stderr, "block_read: block index out of bounds\n");
		return -1;
	}

	if (lseek(handle, block * BLOCK_SIZE, SEEK_SET) < 0) {
		perror("block_read: failed to lseek");
		return -1;
	}

	if (read(handle, buf, BLOCK_SIZE) < 0) {
		perror("block_read: failed to read");
		return -1;
	}

	return 0;
}
