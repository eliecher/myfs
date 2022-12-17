#include "dir.h"
#include "inode.h"
#include "buffer_cache.h"
#ifdef DIR_FIXED_ENTRY_SIZE_TYPE
inode_no_t dir_lookup(inode_t *inode, const char *name)
{
	if (inode->disk_inode.type != FT_DIR)
	{
		perror("dir_lookup: trying to look up in a non directory\n");
		return 0;
	}
	u_int32_t num_entries = ((u_int32_t)inode->disk_inode.size) / DIR_ENTRY_SIZE, entries_seen = 0;
	if (num_entries == 0)
		return 0;
	offset_t off = 0, byte_off;
	buffer_t buffer;
	size_t t;
	block_no_t block_no;
	inode_no_t inode_no = 0;
	int found = 0;
	while (!found && entries_seen < num_entries)
	{
		bmap(inode, off, &block_no, &byte_off, &t);
		bread(block_no, &buffer);
		while (t != 0)
		{
			dir_entry_t dir_entry;
			memcpy(&dir_entry, buffer.data->b + byte_off, DIR_ENTRY_SIZE);
			entries_seen++;
			t -= DIR_ENTRY_SIZE;
			byte_off += DIR_ENTRY_SIZE;
			if (dir_entry.inode_no != 0 && strcmp(dir_entry.name, name) == 0)
			{
				found = 1;
				inode_no = dir_entry.inode_no;
				break;
			}
		}
		brelse(&buffer);
	}
	return inode_no;
}
#endif