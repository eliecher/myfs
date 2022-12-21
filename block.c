#include "myfs.h"
#include "buffer_cache.h"
int balloc(buffer_t *buffer)
{
	if (super_block.bfreeptr == 0)
	{
		perror("balloc: no free blocks\n");
		return -1;
	}
	buffer_t buff;
	if (bread(super_block.bfreeptr, &buff))
	{
		// ! free list is corrupted
		perror("balloc: freelist pointer is invalid\n");
		return -1;
	}
	block_no_t freeb_no;
	memcpy(&freeb_no, buff.data->b + MY_BLK_SIZE - sizeof(block_no_t) * super_block.bfreecount, sizeof(block_no_t));
	if (super_block.bfreecount == 1)
	{
		/* last block in list. that is free block. replace super_block.bfreeptr */
		block_no_t t = freeb_no;
		freeb_no = super_block.bfreeptr;
		super_block.bfreeptr = t;
		memset(buff.data->b, 0, MY_BLK_SIZE);
		BUFF_SET_FIELD(buff, BUFF_MODIFIED);
		super_block.bfreecount = INDEX_SIZE;
		return 0;
	}
	brelse(&buff);
	bread(freeb_no, buffer);
	return 0;
}
int bfree(block_no_t block_no)
{
	buffer_t buffer;
	if (super_block.bfreecount == INDEX_SIZE)
	{
		/* first block is full, add new block */
		super_block.bfreecount = 1;
		if (bread(block_no, &buffer) != 0)
		{
			perror("bfree: cannot access free block\n");
			return -1;
		}
		memset(buffer.data->b, 0, MY_BLK_SIZE);
		memcpy(buffer.data->b + MY_BLK_SIZE - sizeof(block_no_t) * super_block.bfreecount, &(super_block.bfreeptr), sizeof(block_no_t));
		super_block.bfreeptr = block_no;
		BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
		brelse(&buffer);
		return 0;
	}
	/* first block has space */
	bread(super_block.bfreeptr, &buffer);
	super_block.bfreecount++;
	memcpy(buffer.data->b + MY_BLK_SIZE - sizeof(block_no_t) * super_block.bfreecount, &block_no, sizeof(block_no_t));
	BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
	brelse(&buffer);
	return 0;
}
