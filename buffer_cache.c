#include "buffer_cache.h"

block_t buffer_data;
buffer_header_t buffer_header = {0, BUFF_DEFAULT_STATUS};
buffer_t buffer = {&buffer_header, &buffer_data};

/* gives a free(unoccupied) buffer that can be used to store and track a disk block's content. */
int getblk(block_no_t block_no, buffer_t *o_buffer)
{
	/* simply 1 buffer cache */
	if (buffer.header->status & BUFF_OCCUPIED)
	{
		// ! this scenario shouldn't occur in non-multiprogramming environment
		// perror("buffer unavailable: logical error in program\n");
		return -1;
	}
	if (buffer.header->block_no == block_no)
	{
		/* buffer is already of same block number. Just occupy the buffer. */
		buffer.header->status |= BUFF_OCCUPIED;
		*o_buffer = buffer;
		return 0;
	}
	/* buffer is available but its content must be saved before using it */
	bwrite(&buffer);
	buffer.header->status = BUFF_DEFAULT_STATUS | BUFF_OCCUPIED;
	buffer.header->block_no = block_no;
	*o_buffer = buffer;
	return 0;
}

/* releases(unoccupies) a buffer. writing to disk is lazy. */
int brelse(buffer_t *i_buffer)
{
	/* just remove buffer from caller. mark buffer as unoccupied.*/
	BUFF_REM_FIELD(*i_buffer,BUFF_OCCUPIED);
	i_buffer->header = NULL;
	i_buffer->data = NULL;
	return 0;
}

/* read a block from disk. returns a buffer with block's contents. */
int bread(block_no_t block_no, buffer_t *o_buffer)
{
	if (block_no >= super_block.num_blocks)
	{
/* 		sprintf(err, "bread: block %u is out of range\n", block_no);
		perror(err); */
		return -1;
	}
	if (getblk(block_no, o_buffer)!=0)
	{
		return -1;
	}
	if (o_buffer->header->status & BUFF_VALIDDATA)
	{
		/* buffer contains valid data. No need to do anything */
		return 0;
	}
	lseek(disk_fd, block_no * MY_BLK_SIZE, SEEK_SET);
	read(disk_fd, o_buffer->data, MY_BLK_SIZE);
	BUFF_SET_FIELD(*o_buffer,BUFF_VALIDDATA);
	BUFF_REM_FIELD(*o_buffer,BUFF_MODIFIED);
	return 0;
}
/* writes a buffer to disk. */
int bwrite(buffer_t *i_buffer)
{
	if (BUFF_IS_SET(*i_buffer,BUFF_MODIFIED | BUFF_VALIDDATA))
	{ /* write skipped if data is unmodified or invalid */
		lseek(disk_fd, MY_BLK_SIZE * i_buffer->header->block_no, SEEK_SET);
		write(disk_fd, i_buffer->data, MY_BLK_SIZE);
	}
	i_buffer->header->status &= ~BUFF_MODIFIED;
	/* validity of data remains the same */
	return 0;
}

int bclearcache()
{
	bwrite(&buffer);
	buffer.header->block_no = 0;
	buffer.header->status = BUFF_DEFAULT_STATUS;
	memset(buffer.data->b, 0, MY_BLK_SIZE);
	return 0;
}