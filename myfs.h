#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>

#ifndef MYFS_H
#define MYFS_H

#define MY_BLK_SIZE 4096
#define INODE_INDEX_COUNT 8
#define NUM_SUPER_BLOCKS 1
#define MAX_FILE_NAME_SIZE 12
extern int disk_fd;

typedef u_int32_t block_no_t;
typedef u_int8_t byte_t;
typedef u_int32_t inode_no_t;

typedef struct
{
	u_int32_t num_blocks;
	u_int32_t num_inodes;
	inode_no_t root;
	inode_no_t ifreeptr;
	u_int32_t ifreecount;
	block_no_t bfreeptr;
	u_int32_t bfreecount;
} super_block_t;
extern super_block_t super_block;

typedef struct
{
	byte_t b[MY_BLK_SIZE];
} block_t;
typedef struct
{
	block_no_t block_no;
	int status;
} buffer_header_t;
typedef struct
{
	buffer_header_t *header;
	block_t *data;
} buffer_t;
typedef int64_t offset_t;

#define NUM_0DEG_INDEX 0
#define NUM_1DEG_INDEX 8
#define NUM_2DEG_INDEX 0
#define NUM_3DEG_INDEX 0

#define KEY_SIZE 20
extern char err[100];
typedef union
{
	u_int16_t permissions;
	struct
	{
		u_int16_t pd : 7, ur : 1, uw : 1, ux : 1, gr : 1, gw : 1, gx : 1, or : 1, ow : 1, ox : 1;
	} ugo;
} permission_t;
typedef struct
{
	offset_t size;
	u_int32_t size_on_disk;
	struct
	{
		block_no_t deg1[NUM_1DEG_INDEX];
	} index;
	u_int16_t links;
	u_int16_t type;
	permission_t permission;
	u_int16_t protection;
} disk_inode_t;
typedef struct
{
	inode_no_t inode_no;
	int status;
	u_int16_t reference_count;
	byte_t key[KEY_SIZE];
	disk_inode_t disk_inode;
} inode_t;
typedef struct
{
	inode_no_t inode_no;
	u_int16_t type;
	char name[10];
} dir_entry_t;

typedef struct
{
	inode_t *inode;
	offset_t offset;
	int mode;
} open_file_info_t;

#define DISK_INODE_SIZE sizeof(disk_inode_t)

/*  */extern int getblk(block_no_t, buffer_t *);
/*  */extern int brelse(buffer_t *);
/*  */extern int bread(block_no_t, buffer_t *);
/*  */extern int bwrite(buffer_t *);
/*  */extern int bclearcache();
/*  */extern int iget(inode_no_t, inode_t **);
/*  */extern int iput(inode_t *);
/*  */extern int bmap(inode_t *, offset_t, block_no_t *, offset_t *, size_t *);
/*  */extern int namei(const char *, inode_t **);
/*  */extern int ialloc(inode_t **);
/*  */extern int ifree(inode_no_t);
/*  */extern int balloc(buffer_t *);
/*  */extern int bfree(block_no_t);
extern int free_all_blocks(inode_t *);
/*  */extern int myopen(const char *, int, ...);
/*  */extern ssize_t myread(int, byte_t *, size_t);
extern ssize_t mywrite(int, byte_t *, size_t);
/*  */extern offset_t mylseek(int, offset_t , int);
/*  */extern int myclose(int);
/*  */extern int mycreat(const char *, permission_t);
/*  */extern int mymkdir(const char *, const char *);
/*  */extern int myrmdir(const char *);
/*  */extern int mylink(const char *, const char *);
extern int myunlink(const char *);
extern int encode(int, byte_t[KEY_SIZE],...);
/*  */extern int add_physical_block(inode_t*,block_no_t, block_no_t);
/*  */extern dir_entry_t dir_lookup(inode_t *, const char *, offset_t*);
/*  */extern int add_dir_entry(inode_t* ,dir_entry_t);
/*  */extern int rem_dir_entry(inode_t* ,offset_t);
#endif