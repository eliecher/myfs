#include "myfs.h"
#include "inode.h"
#include "buffer_cache.h"
inode_t inode_table[MAX_ACTIVE_INODES];

/* if found, gives index.
 ! if found but locked, error (-1)
 * if not found, -(2+inactive)
 */
int find_in_inode_table(inode_no_t inode_no)
{
	int h1 = inode_no % MAX_ACTIVE_INODES;
	int h2 = 1;
	int i = h1;
	int inactive = MAX_ACTIVE_INODES;
	do
	{												   /* exhaustive search is done on the table */
		if (INO_IS_SET(inode_table + i, INODE_ACTIVE)) /* if table entry is active */
		{
			inactive--;
			if (inode_table[i].inode_no == inode_no)
			{
				if (inode_table[i].status & INODE_LOCKED)
				{
					/* 					sprintf(err, "needed inode (%u) is locked\n", inode_no);
										perror(err); */
					return -1;
				}
				return i;
			}
		}
		i = (i + h2) % MAX_ACTIVE_INODES;
	} while (i != h1);
	return -2 - inactive;
}

int init_table_entry(inode_no_t inode_no)
{
	int h1 = inode_no % MAX_ACTIVE_INODES;
	int h2 = 1;
	int i = h1;
	do
	{
		if (!INO_IS_SET(inode_table + i, INODE_ACTIVE))
		{
			inode_table[i].inode_no = inode_no;
			inode_table[i].status = INODE_DEFAULT_STATUS | INODE_ACTIVE;
			return i;
		}
	} while (i != h1);
	return -1;
}

offset_t siz_index[] = {SIZ_0DEG_INDEX, SIZ_1DEG_INDEX, SIZ_2DEG_INDEX, SIZ_3DEG_INDEX};

int iget(inode_no_t inode_no, inode_t **inode)
{
	if (inode_no > super_block.num_inodes || inode_no == 0)
	{
		// perror("iget: invalid inode number\n");
		return -1;
	}
	int i = find_in_inode_table(inode_no);
	inode_t *inode_ptr = NULL;
	if (i >= 0) /* if in active inodes */
	{
		inode_ptr = inode_table + i;
		inode_ptr->status |= INODE_LOCKED;
		inode_ptr->reference_count++;
		*inode = inode_ptr;
		return 0;
	}
	else if (i == -2) /* not in table and no free space */
	{
		/* 		perror("iget: no free space in inode table\n"); */
		return -1;
	}
	else if (i == -1) /* other errors */
	{
		/* 		perror("iget: error\n"); */
		return -1;
	}
	/* inode should be read from the disk */
	block_no_t block_no = INODE_NO_TO_BLOCK_NO(inode_no);
	buffer_t buffer;
	bread(block_no, &buffer);
	i = init_table_entry(inode_no);
	inode_ptr = *inode = inode_table + i;
	offset_t inode_byte_offset = INODE_NO_TO_BYTE_OFF(inode_no);
	memcpy(&(inode_ptr->disk_inode), buffer.data->b + inode_byte_offset, DISK_INODE_SIZE);
	brelse(&buffer);
	inode_ptr->reference_count++;			/* increase reference count */
	INO_SET_FIELD(inode_ptr, INODE_LOCKED); /* lock the inode */
	return 0;
}

int iput(inode_t *inode)
{
	if (!INO_IS_SET(inode, INODE_LOCKED))
		INO_SET_FIELD(inode, INODE_LOCKED);
	inode->reference_count--;
	if (inode->reference_count == 0)
	{
		if (inode->disk_inode.links == 0)
		{
			free_all_blocks(inode);
			inode->disk_inode.type = FT_NONE;
			// todo clear inode
			INO_SET_FIELD(inode, INODE_MODIFIED);
			ifree(inode->inode_no);
			return 0;
		}
		else if (inode->status & INODE_MODIFIED)
		{
			block_no_t block_no = INODE_NO_TO_BLOCK_NO(inode->inode_no);
			buffer_t buffer;
			bread(block_no, &buffer);
			offset_t inode_byte_offset = INODE_NO_TO_BYTE_OFF(inode->inode_no);
			memcpy(buffer.data->b + inode_byte_offset, &(inode->disk_inode), DISK_INODE_SIZE);
			BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
			brelse(&buffer);
		}
		inode->status = INODE_DEFAULT_STATUS;
		inode->inode_no = 0;
		inode->reference_count = 0;
	}
	INO_REM_FIELD(inode, INODE_LOCKED);
	return 0;
}

/* maps byte offset to block number. tells at what byte offset in the block does the offset lie. tells number of bytes of file in the block from the offset. */
int bmap(inode_t *inode, offset_t offset, block_no_t *block_no, offset_t *byte_offset, size_t *num_bytes_in_block)
{
	// inode is locked
	/* offset has a limit */
	int indirection_lvl = -1;
	block_no_t index_block;
	offset_t fsz = inode->disk_inode.size;
	if (offset >= MAX_FILE_SIZE || offset < 0)
	{
		return -1;
	}
	if (offset >= fsz)
	{
		/* offset beyond EOF */
		if (fsz != 0 && offset / MY_BLK_SIZE == (fsz - 1) / MY_BLK_SIZE)
		{
			/* case: offset being mapped is in same logical block as last byte of the file */
			bmap(inode, fsz - 1, block_no, byte_offset, num_bytes_in_block);
		}
		else
		{
			/* case: offset being mapped is not in same logical block as last byte of file */
			*block_no = 0;
		}
		*byte_offset = offset % MY_BLK_SIZE; /* byte offset in the blocks */
		*num_bytes_in_block = 0;			 /* as offset is beyond EOF, no bytes of file in the block */
		return 0;
	}
	if (offset < CAP_0DEG_INDEX)
	{
		indirection_lvl = 0;
		/* handle direct block address */
	}
	else if ((offset -= CAP_0DEG_INDEX) < CAP_1DEG_INDEX)
	{
		indirection_lvl = 1;
		/* handle 1 degree index */
		int index = offset / siz_index[1];
		index_block = inode->disk_inode.index.deg1[index];
		offset = offset % siz_index[1];
		fsz = fsz - index * siz_index[1];
	}
	else if ((offset -= CAP_1DEG_INDEX) < CAP_2DEG_INDEX)
	{
		indirection_lvl = 2;
		/* handle 2 degree index */
	}
	else if ((offset -= CAP_2DEG_INDEX) < CAP_3DEG_INDEX)
	{
		indirection_lvl = 3;
		/* handle 3 degree index */
	}
	buffer_t buffer;
	while (index_block != 0 && indirection_lvl != 0)
	{
		int index = offset / siz_index[indirection_lvl - 1];
		/* logical index entry number in index */
		int loc_of_index; /* actual index entry number */
		if (inode->disk_inode.type == FT_FIL)
			loc_of_index = encode(index, inode->key);
		else
			loc_of_index = index;
		bread(index_block, &buffer);
		memcpy(&index_block, buffer.data->b + (loc_of_index * sizeof(block_no_t)), sizeof(block_no_t));
		brelse(&buffer);
		offset = offset % siz_index[indirection_lvl - 1];
		fsz -= index * siz_index[indirection_lvl - 1];
		indirection_lvl--;
	}
	*block_no = index_block;
	if ((fsz - 1) / MY_BLK_SIZE == offset / MY_BLK_SIZE)
	{
		*num_bytes_in_block = fsz - offset;
	}
	else
	{
		*num_bytes_in_block = MY_BLK_SIZE - *byte_offset;
	}
	return 0;
}

int extract_name(const char *p, char *dest)
{
	if (p[0] != '/')
		return -1;
	p++;
	int l = 0;
	while (p[0] != '/' && p[0] != '\0')
	{
		*(dest++) = *p;
		l++;
		p++;
	}
	*dest = '\0';
	return l;
}

int namei(const char *path, inode_t **inode)
{
	inode_t *cur = NULL;
	if (iget(super_block.root, &cur) != 0)
	{
		perror("namei: cannot get root\n");
		return -1;
	}
	char partpath[MAX_FILE_NAME_SIZE + 1];
	while (path[0] != '\0' && (path[0] != '/' || path[1] != '\0'))
	{
		int l = extract_name(path, partpath);
		if (l == 0 || cur->disk_inode.type != FT_DIR)
		{
			iput(cur);
			perror("namei: cannot resolve path\n");
			return -1;
		}
		path += l + 1;
		if (l == 1 && partpath[0] == '.')
			continue;
		inode_no_t nextinode = dir_lookup(cur, partpath);
		iput(cur);
		if (nextinode == 0)
		{
			perror("namei: cannot resolve path\n");
			return -1;
		}
		iget(nextinode, &cur);
	}
	*inode = cur;
	return 0;
}
int ialloc(inode_t **inode)
{
	if (super_block.ifreeptr == 0)
		return -1;
	block_no_t block_no;
	offset_t offset;
	inode_no_t inode_no;
	/* get location of inode on disk */
	block_no = INODE_NO_TO_BLOCK_NO(super_block.ifreeptr - 1);
	offset = INODE_NO_TO_BYTE_OFF(super_block.ifreeptr - 1);
	buffer_t buffer;
	bread(block_no, &buffer);
	/* calculate offset of list element in the inode */
	offset += offsetof(disk_inode_t, index) + sizeof(inode_no_t) * (INODE_INDEX_COUNT - super_block.ifreecount);
	memcpy(&inode_no, buffer.data->b + offset, sizeof(inode_no_t));
	memset(buffer.data->b + offset, 0, sizeof(inode_no_t));
	BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
	brelse(&buffer);
	if (super_block.ifreecount == 1)
	{
		inode_no_t t = inode_no;
		inode_no = super_block.ifreeptr;
		super_block.ifreeptr = t;
		super_block.ifreecount = INODE_INDEX_COUNT;
	}
	else
	{
		super_block.ifreecount--;
	}
	if (iget(inode_no, inode) != 0)
	{
		perror("ialloc: could not get free inode\n");
		return -1;
	}
	inode_t *inode_ptr = *inode;
	/*
		todo: set parameters for new inode
	*/
	INO_SET_FIELD(inode_ptr, INODE_MODIFIED);
	return 0;
}
int ifree(inode_no_t inode_no)
{
	block_no_t block_no;
	offset_t offset;
	buffer_t buffer;
	disk_inode_t disk_inode;
	if (super_block.ifreecount == INODE_INDEX_COUNT)
	{
		block_no = INODE_NO_TO_BLOCK_NO(inode_no);
		offset = INODE_NO_TO_BYTE_OFF(inode_no);
		if (bread(block_no, &buffer) != 0)
		{
			perror("ifree: cannot access free inode\n");
			return -1;
		}
		memcpy(&disk_inode, buffer.data->b + offset, DISK_INODE_SIZE);
		clear_inode(&disk_inode);
		*((block_no_t *)(&disk_inode.index) + INODE_INDEX_COUNT - 1) = super_block.ifreeptr;
		memcpy(buffer.data->b + offset, &disk_inode, DISK_INODE_SIZE);
		BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
		brelse(&buffer);
		super_block.ifreeptr = inode_no;
		super_block.ifreecount = 1;
	}
	else
	{
		block_no = INODE_NO_TO_BLOCK_NO(inode_no);
		offset = INODE_NO_TO_BYTE_OFF(inode_no);
		bread(block_no, &buffer);
		memcpy(&disk_inode, buffer.data->b + offset, DISK_INODE_SIZE);
		clear_inode(&disk_inode);
		memcpy(buffer.data->b + offset, &disk_inode, DISK_INODE_SIZE);
		BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
		brelse(&buffer);
		block_no = INODE_NO_TO_BLOCK_NO(super_block.ifreeptr);
		offset = INODE_NO_TO_BYTE_OFF(super_block.ifreeptr);
		bread(block_no, &buffer);
		memcpy(&disk_inode, buffer.data->b + offset, DISK_INODE_SIZE);
		super_block.ifreecount++;
		*((block_no_t *)(&disk_inode.index) + INODE_INDEX_COUNT - super_block.ifreecount) = inode_no;
		memcpy(buffer.data->b + offset, &disk_inode, DISK_INODE_SIZE);
		BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
		brelse(&buffer);
	}
	return 0;
}

int add_physical_block(inode_t *inode, block_no_t logical_block_no, block_no_t block_no)
{
	buffer_t buffer;
	offset_t offset = logical_block_no * MY_BLK_SIZE;
	block_no_t index_block;
	int indirection_lvl = 1;
	if (offset < CAP_0DEG_INDEX)
	{
		indirection_lvl = 0;
		/* handle direct block address */
	}
	else if ((offset -= CAP_0DEG_INDEX) < CAP_1DEG_INDEX)
	{
		indirection_lvl = 1;
		/* handle 1 degree index */
		int index = offset / siz_index[1];
		index_block = inode->disk_inode.index.deg1[index];
		if (index_block == 0)
		{
			balloc(&buffer);
			index_block = inode->disk_inode.index.deg1[index] = buffer.header->block_no;
			INO_SET_FIELD(inode, INODE_MODIFIED);
			brelse(&buffer);
		}
	}
	else if ((offset -= CAP_1DEG_INDEX) < CAP_2DEG_INDEX)
	{
		indirection_lvl = 2;
		/* handle 2 degree index */
	}
	else if ((offset -= CAP_2DEG_INDEX) < CAP_3DEG_INDEX)
	{
		indirection_lvl = 3;
		/* handle 3 degree index */
	}
	while (indirection_lvl != 1)
	{
		int index = offset / siz_index[indirection_lvl - 1];
		/* logical index entry number in index */
		int loc_of_index; /* actual index entry number */
		if (inode->disk_inode.type == FT_FIL)
			loc_of_index = encode(index, inode->key);
		else
			loc_of_index = index;
		block_no_t entry;
		bread(index_block, &buffer);
		memcpy(&entry, buffer.data->b + (loc_of_index * sizeof(block_no_t)), sizeof(block_no_t));
		brelse(&buffer);
		if (entry == 0)
		{
			balloc(&buffer);
			entry = buffer.header->block_no;
			INO_SET_FIELD(inode, INODE_MODIFIED);
			brelse(&buffer);
			bread(index_block, &buffer);
			memcpy(buffer.data->b + (loc_of_index * sizeof(block_no_t)), &entry, sizeof(block_no_t));
			BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
			brelse(&buffer);
		}
		offset = offset % siz_index[indirection_lvl - 1];
		indirection_lvl--;
		index_block = entry;
	}
	int index = offset / siz_index[0];
	int loc_of_index;
	if (inode->disk_inode.type == FT_FIL)
		loc_of_index = encode(index, inode->key);
	else
		loc_of_index = index;
	block_no_t entry;
	bread(index_block, &buffer);
	memcpy(&entry, buffer.data->b + (loc_of_index * sizeof(block_no_t)), sizeof(block_no_t));
	memcpy(buffer.data->b + (loc_of_index * sizeof(block_no_t)), sizeof(block_no_t), sizeof(block_no_t));
	BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
	brelse(&buffer);
	if (entry != 0)
		bfree(entry);
	return 0;
}