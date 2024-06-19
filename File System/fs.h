#ifndef INCLUDE_FS_H
#define INCLUDE_FS_H
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int make_fs(const char *disk_name);
int mount_fs(const char *disk_name);
int umount_fs(const char *disk_name);
int fs_open(const char *name);
int fs_close(int fildes);
int fs_create(const char *name);
int fs_delete(const char *name);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_listfiles(char ***files);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);
#endif /* INCLUDE_FS_H */
