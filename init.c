#include "myfs.h"
#include "inode.h"
struct bfreelist
{
	block_no_t freeptr;
	u_int32_t freecount;
};

struct ifreelist
{
	inode_no_t freeptr;
	u_int32_t freecount;
};

struct bfreelist create_bfreelist(int fd, block_no_t from, block_no_t to)
{
	struct bfreelist freelist;
	freelist.freecount = INDEX_SIZE;
	freelist.freeptr = 0;
	int left = (int)((long)to - (long)from + 1);
	while (left >= INDEX_SIZE)
	{
		block_t block;
		block_no_t *entry = (block_no_t *)(block.b + MY_BLK_SIZE) - 1, *first = (block_no_t *)&block;
		*(entry--) = freelist.freeptr;
		while (entry >= first)
			*(entry--) = to--;
		freelist.freeptr = to--;
		left -= INDEX_SIZE;
		lseek(fd, (freelist.freeptr + NUM_SUPER_BLOCKS - 1) * MY_BLK_SIZE, SEEK_SET);
		write(fd, &block, MY_BLK_SIZE);
	}
	if (left > 0)
	{
		freelist.freecount = left;
		block_t block;
		block_no_t *entry = (block_no_t *)(block.b + MY_BLK_SIZE) - 1;
		*(entry--) = freelist.freeptr;
		left--;
		while (left--)
			*(entry--) = to--;
		freelist.freeptr = to--;
		left = 0;
		lseek(fd, (freelist.freeptr + NUM_SUPER_BLOCKS - 1) * MY_BLK_SIZE, SEEK_SET);
		write(fd, &block, MY_BLK_SIZE);
	}
	return freelist;
}

struct ifreelist create_ifreelist(int fd, inode_no_t from, inode_no_t to)
{
	struct ifreelist freelist;
	freelist.freecount = INODE_INDEX_COUNT;
	freelist.freeptr = 0;
	int left = (int)((long)to - (long)from + 1);
	while (left >= INODE_INDEX_COUNT)
	{
		disk_inode_t inode = model_unused_inode;
		inode_no_t *entry = (inode_no_t *)(&(inode.index) + 1) - 1, *first = (inode_no_t *)(&(inode.index));
		while (entry >= first)
			*(entry--) = to--;
		freelist.freeptr = to--;
		left -= INODE_INDEX_COUNT;
		block_no_t block_no = INODE_NO_TO_BLOCK_NO(freelist.freeptr);
		offset_t offset = INODE_NO_TO_BYTE_OFF(freelist.freecount);
		lseek(fd, block_no * MY_BLK_SIZE + offset, SEEK_SET);
		write(fd, &inode, DISK_INODE_SIZE);
	}
	if (left > 0)
	{
		freelist.freecount = left;
		disk_inode_t inode = model_unused_inode;
		inode_no_t *entry = (inode_no_t *)(&(inode.index) + 1) - 1;
		*(entry--) = freelist.freeptr;
		left--;
		while (left--)
			*(entry--) = to--;
		freelist.freeptr = to--;
		left = 0;
		block_no_t block_no = INODE_NO_TO_BLOCK_NO(freelist.freeptr);
		offset_t offset = INODE_NO_TO_BYTE_OFF(freelist.freecount);
		lseek(fd, block_no * MY_BLK_SIZE + offset, SEEK_SET);
		write(fd, &inode, DISK_INODE_SIZE);
	}
	return freelist;
}

int create_volume(const char *name, block_no_t number_of_blocks, inode_no_t number_of_inodes)
{
	/* number of blocks + num_super_blocks (for super block) blocks */
	int inode_array_blocks = (number_of_inodes * DISK_INODE_SIZE) / MY_BLK_SIZE;
	if (inode_array_blocks * MY_BLK_SIZE < number_of_inodes * DISK_INODE_SIZE)
		inode_array_blocks++;
	if (number_of_blocks < inode_array_blocks)
	{
		perror("failed\n");
		return -1;
	}
	int fd = open(name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (fd < 0)
	{
		perror("failed\n");
		return -1;
	}
	lseek(fd, (number_of_blocks + NUM_SUPER_BLOCKS) * MY_BLK_SIZE - 1, SEEK_SET);
	write(fd, "\0", 1);
	block_t default_inode_array_block = {.b = {0}};
	offset_t off = 0;
	for (int i = 0; i < INODES_PER_BLOCK; i++)
	{
		memcpy(default_inode_array_block.b + off, &model_unused_inode, DISK_INODE_SIZE);
		off += DISK_INODE_SIZE;
	}
	for (block_no_t i = NUM_SUPER_BLOCKS, lim = NUM_SUPER_BLOCKS + inode_array_blocks; i < lim; i++)
	{
		lseek(fd, i * MY_BLK_SIZE, SEEK_SET);
		write(fd, default_inode_array_block.b, MY_BLK_SIZE);
	}
	block_no_t to = number_of_blocks, from = inode_array_blocks + 1;
	struct bfreelist bfreelist = create_bfreelist(fd, from, to);
	struct ifreelist ifreelist = create_ifreelist(fd, 2, number_of_inodes);
	super_block_t sup = {
		.bfreecount = bfreelist.freecount, .bfreeptr = bfreelist.freeptr, .ifreecount = ifreelist.freecount, .ifreeptr = ifreelist.freeptr, .num_blocks = number_of_blocks + NUM_SUPER_BLOCKS, .num_inodes = number_of_inodes, .root = 1};
	
	close(fd);
}