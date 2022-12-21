#include "dir.h"
#include "inode.h"
#include "buffer_cache.h"
#ifdef DIR_FIXED_ENTRY_SIZE_TYPE
dir_entry_t dir_lookup(inode_t *inode, const char *name, offset_t *found_at)
{
	dir_entry_t dir_entry;
	*found_at = -1;
	dir_entry.inode_no = 0;
	if (inode->disk_inode.type != FT_DIR)
	{
		perror("dir_lookup: trying to look up in a non directory\n");
		return dir_entry;
	}
	char formatted_name[MAX_FILE_NAME_SIZE];
	{
		memset(formatted_name, 0, MAX_FILE_NAME_SIZE);
		int l = strlen(name);
		if (l > MAX_FILE_NAME_SIZE)
			return dir_entry;
		memcpy(formatted_name, name, l);
		name = formatted_name;
	}
	u_int32_t num_entries = ((u_int32_t)inode->disk_inode.size) / DIR_ENTRY_SIZE, entries_seen = 0;
	if (num_entries == 0)
		return dir_entry;
	offset_t off = 0, byte_off;
	buffer_t buffer;
	size_t t;
	block_no_t block_no;
	inode_no_t inode_no = 0;
	while (entries_seen < num_entries)
	{
		bmap(inode, off, &block_no, &byte_off, &t);
		bread(block_no, &buffer);
		while (t > DIR_ENTRY_SIZE)
		{
			memcpy(&dir_entry, buffer.data->b + byte_off, DIR_ENTRY_SIZE);
			entries_seen++;
			t -= DIR_ENTRY_SIZE;
			byte_off += DIR_ENTRY_SIZE;
			if (dir_entry.inode_no != 0 && memcmp(dir_entry.name, name, MAX_FILE_NAME_SIZE) == 0)
			{
				brelse(&buffer);
				*found_at = (entries_seen - 1) * DIR_ENTRY_SIZE;
				return dir_entry;
			}
		}
		brelse(&buffer);
		off += MY_BLK_SIZE;
	}
	dir_entry.inode_no = 0;
	return dir_entry;
}
int add_dir_entry(inode_t *dir, dir_entry_t new_entry)
{
	if (dir->disk_inode.type != FT_DIR)
	{
		return -1;
	}
	u_int32_t num_entries = ((u_int32_t)dir->disk_inode.size) / DIR_ENTRY_SIZE, entries_seen = 0;
	offset_t off = 0, byte_off;
	buffer_t buffer;
	size_t t;
	block_no_t block_no;
	offset_t loc = -1;
	while (entries_seen < num_entries)
	{
		/* see all entries */
		bmap(dir, off, &block_no, &byte_off, &t);
		bread(block_no, &buffer);
		while (t != 0)
		{
			dir_entry_t dir_entry;
			memcpy(&dir_entry, buffer.data->b + byte_off, DIR_ENTRY_SIZE);
			if (loc == -1 && dir_entry.inode_no == 0)
			{ /* look for empty entry */
				loc = off + byte_off;
			}
			entries_seen++;
			t -= DIR_ENTRY_SIZE;
			byte_off += DIR_ENTRY_SIZE;
			if (dir_entry.inode_no != 0 && memcmp(dir_entry.name, new_entry.name, MAX_FILE_NAME_SIZE) == 0) /* look for duplicates */
			{
				/* no duplicate records allowed*/
				brelse(&buffer);
				return -1;
			}
		}
		brelse(&buffer);
		off += MY_BLK_SIZE;
	}
	if (loc == -1 && num_entries >= MAX_FILE_SIZE / DIR_ENTRY_SIZE) /* no space for new record */
	{
		return -1;
	}
	if (loc == -1)
	{
		loc = dir->disk_inode.size;
		dir->disk_inode.size += DIR_ENTRY_SIZE;
		INO_SET_FIELD(dir, INODE_MODIFIED);
	}
	bmap(dir, loc, &block_no, &byte_off, &t);
	if (block_no == 0)
	{
		balloc(&buffer);
	}
	else
		bread(block_no, &buffer);
	memcpy(buffer.data->b + byte_off, &new_entry, DIR_ENTRY_SIZE);
	BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
	block_no_t physical_block_no = buffer.header->block_no;
	brelse(&buffer);
	dir->disk_inode.links++;
	INO_SET_FIELD(dir, INODE_MODIFIED);
	if (block_no == 0) /* if data was written on a new block */
	{
		dir->disk_inode.size_on_disk += MY_BLK_SIZE;
		INO_SET_FIELD(dir, INODE_MODIFIED);
		add_physical_block(dir, loc / MY_BLK_SIZE, physical_block_no);
	}
	return 0;
}

int rem_dir_entry(inode_t *dir, offset_t loc)
{
	block_no_t block_no;
	offset_t byte_offset;
	size_t num_bytes;
	bmap(dir, loc, &block_no, &byte_offset, &num_bytes);
	if (block_no == 0)
		return -1;
	buffer_t buffer;
	bread(block_no, &buffer);
	memset(buffer.data->b + byte_offset, 0, DIR_ENTRY_SIZE);
	dir->disk_inode.links--;
	INO_SET_FIELD(dir, INODE_MODIFIED);
	brelse(&buffer);
	return 0;
}

int mymkdir(const char *parent_dir, const char *dir_name)
{
	inode_t *par_dir_inode, *dir_inode;
	if (namei(parent_dir, &parent_dir) != 0)
	{
		return -1;
	}
	dir_entry_t new_entry;
	{
		int l = strlen(dir_name);
		if (l > MAX_FILE_NAME_SIZE)
		{
			iput(par_dir_inode);
			return -1;
		}
		memset(new_entry.name, 0, MAX_FILE_NAME_SIZE);
		memcpy(new_entry.name, dir_name, l);
	}
	if (ialloc(&dir_inode) != 0)
	{
		iput(par_dir_inode);
		return -1;
	}
	new_entry.type = dir_inode->disk_inode.type = FT_DIR;
	INO_SET_FIELD(dir_inode, INODE_MODIFIED);
	new_entry.inode_no = dir_inode->inode_no;
	dir_entry_t parent_dir_entry;
	{
		memset(parent_dir_entry.name, 0, MAX_FILE_NAME_SIZE);
		parent_dir_entry.name[0] = parent_dir_entry.name[1] = '.';
	}
	parent_dir_entry.inode_no = par_dir_inode->inode_no;
	parent_dir_entry.type = FT_DIR;
	add_dir_entry(dir_inode, parent_dir_entry);
	dir_inode->disk_inode.links = 1;
	INO_SET_FIELD(dir_inode, INODE_MODIFIED);
	iput(dir_inode);
	add_dir_entry(parent_dir, new_entry);
	iput(par_dir_inode);
	return 0;
}

int myrmdir(const char *dir_path)
{
	char dir_name[MAX_FILE_NAME_SIZE + 1];
	char par_path[100];
	{
		strcpy(par_path, dir_path);
		char *dirn = par_path + strlen(par_path) - 1;
		if (dirn[0] == '/')
			*(dirn--) = '\0';
		while (dirn[0] != '\0' && dirn[0] != '/')
		{
			dirn--;
		}
		if (dirn[0] == '\0')
			return -1;
		memcpy(dir_name, dirn + 1, MAX_FILE_NAME_SIZE);
		dir_name[MAX_FILE_NAME_SIZE] = '\0';
		int dnl = strlen(dir_name);
		memset(dir_name + dnl, 0, MAX_FILE_NAME_SIZE - dnl);
		dirn[1] = '\0';
	}
	inode_t *par, *dir;
	if (namei(par_path, &par) != 0)
		return -1;
	offset_t found_at;
	dir_entry_t dir_entry = dir_lookup(par, dir_name, &found_at);
	if (dir_entry.inode_no != 0)
	{
		if (iget(dir_entry.inode_no, &dir) == 0)
		{
			if (dir->disk_inode.links == 1)
			{
				rem_dir_entry(par, found_at);
				dir->disk_inode.links = 0;
				INO_SET_FIELD(dir, INODE_MODIFIED);
				iput(dir);
				return 0;
			}
			iput(dir);
		}
	}
	iput(par);
	return -1;
}

#endif
int mylink(const char *existing_path, const char *new_path)
{
	char fil_name[MAX_FILE_NAME_SIZE + 1];
	char par_path[100];
	{
		strcpy(par_path, new_path);
		char *dirn = par_path + strlen(par_path) - 1;
		if (dirn[0] == '/')
			return -1;
		while (dirn[0] != '\0' && dirn[0] != '/')
		{
			dirn--;
		}
		if (dirn[0] == '\0')
			return -1;
		memcpy(fil_name, dirn + 1, MAX_FILE_NAME_SIZE);
		fil_name[MAX_FILE_NAME_SIZE] = '\0';
		dirn[1] = '\0';
		int fnl = strlen(fil_name);
		memset(fil_name + fnl, 0, MAX_FILE_NAME_SIZE - fnl);
	}
	inode_t *inode, *par_dir;
	if (namei(existing_path, &inode) == 0)
	{
		if (inode->disk_inode.type == FT_FIL)
		{
			if (namei(par_path, &par_dir) == 0)
			{
				if (par_dir->disk_inode.type == FT_DIR)
				{
					dir_entry_t dir_entry;
					inode->disk_inode.links++;
					INO_SET_FIELD(inode, INODE_MODIFIED);
					dir_entry.inode_no = inode->inode_no;
					iput(inode);
					memcpy(dir_entry.name, fil_name, MAX_FILE_NAME_SIZE);
					dir_entry.type = FT_FIL;
					if (add_dir_entry(par_dir, dir_entry) == 0)
					{
						iput(par_dir);
						return 0;
					}
					iput(par_dir);
					return -1;
				}
				iput(par_dir);
			}
		}
		iput(inode);
	}
	return -1;
}
int myunlink(const char *fil_path)
{
	char fil_name[MAX_FILE_NAME_SIZE + 1];
	char par_path[100];
	{
		strcpy(par_path, fil_path);
		char *dirn = par_path + strlen(par_path) - 1;
		if (dirn[0] == '/')
			return -1;
		while (dirn[0] != '\0' && dirn[0] != '/')
		{
			dirn--;
		}
		if (dirn[0] == '\0')
			return -1;
		memcpy(fil_name, dirn + 1, MAX_FILE_NAME_SIZE);
		fil_name[MAX_FILE_NAME_SIZE] = '\0';
		dirn[1] = '\0';
		int fnl = strlen(fil_name);
		memset(fil_name + fnl, 0, MAX_FILE_NAME_SIZE - fnl);
	}
	inode_t *par, *inode;
	if (namei(par_path, &par) == 0)
	{
		offset_t found_at;
		dir_entry_t dir_entry = dir_lookup(par, fil_name, &found_at);
		if (dir_entry.inode_no != 0)
		{
			rem_dir_entry(par, found_at);
			iput(par);
			iget(dir_entry.inode_no, &inode);
			inode->disk_inode.links--;
			INO_SET_FIELD(inode, INODE_MODIFIED);
			iput(inode);
			return 0;
		}
		iput(par);
	}
	return -1;
}