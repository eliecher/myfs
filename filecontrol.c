#include "filecontrol.h"
#include "inode.h"
#include "buffer_cache.h"
#include <stdarg.h>

#define MAX_OPEN_FILES 10

open_file_info_t file_table[MAX_OPEN_FILES];

int myopen(const char *filename, int mode, ...)
{
	inode_t *inode = NULL;
	if (namei(filename, &inode) != 0)
	{
		/* file does not exist. if M_CREAT is given, try to create the file */
		if (IS_SET(mode, M_CREAT))
		{
			va_list l;
			va_start(l, mode);
			permission_t perm = va_arg(l, permission_t);
			va_end(l);
			if (mycreat(filename, perm) != 0)
			{
				/* 				perror("open: failed to create file\n"); */
				return -1;
			}
			if (namei(filename, &inode) != 0)
			{
				/* 				perror("open: failed to open file\n"); */
				return -1;
			}
		}
		else
		{
			/* 			perror("open: failed to open file\n"); */
			return -1;
		}
	}
	int fd;
	for (fd = 0; fd < MAX_OPEN_FILES; fd++)
	{
		if (IS_SET(file_table->mode, S_OPEN))
			continue;
		break;
	}
	if (fd == MAX_OPEN_FILES)
	{
		/* 		perror("open: no free file descriptor\n"); */
		iput(inode);
		return -1;
	}
	file_table[fd].inode = inode;
	file_table[fd].mode = M_DEFAULT_MODE | S_OPEN;
	if (IS_SET(mode, M_RD))
		file_table[fd].mode |= M_RD;
	if (IS_SET(mode, M_WR))
	{
		file_table[fd].mode |= M_WR;
		if (IS_SET(mode, M_TRUNC))
			free_all_blocks(inode);
	}
	if (IS_SET(mode, M_APP))
		file_table[fd].mode |= M_APP;
	file_table[fd].offset = 0;
	INO_REM_FIELD(inode, INODE_LOCKED);
	return fd;
}

offset_t mylseek(int fd, offset_t relative_offset, int whence)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || (file_table[fd].mode & S_OPEN) == 0)
	{
		perror("lseek: bad fd\n");
		return -1;
	}
	inode_t *inode = file_table[fd].inode;
	INO_SET_FIELD(inode, INODE_LOCKED);
	switch (whence)
	{
	case WH_CUR:
		relative_offset += file_table[fd].offset;
		break;
	case WH_END:
		relative_offset += inode->disk_inode.size;
		break;
	case WH_SET:
	default:
		break;
	}
	INO_REM_FIELD(inode, INODE_LOCKED);
	if (relative_offset < 0 || relative_offset >= MAX_FILE_SIZE)
	{
		// ! final position is more than myfs allows or before the beginning
		perror("lseek: bad position seeked\n");
		return -1;
	}
	file_table[fd].offset = relative_offset;
	return relative_offset;
}
int myclose(int fd)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || (file_table[fd].mode & S_OPEN) == 0)
	{
		perror("close: bad fd\n");
		return -1;
	}
	inode_t *inode = file_table[fd].inode;
	INO_SET_FIELD(inode, INODE_LOCKED);
	/*
		todo: check changes in inode before closing. (like access time, modified time etc.)
	*/
	iput(inode);
	file_table[fd].inode = NULL;
	file_table[fd].mode = M_DEFAULT_MODE;
	file_table[fd].offset = -1;
	return 0;
}
int mycreat(const char *path, permission_t perm)
{
	char dir_path[100];
	dir_path[0] = 0;
	strcpy(dir_path + 1, path);
	int l = strlen(dir_path);
	char *filename = dir_path + l - 1;
	while (*filename != '/' || *filename != 0)
		filename--;
	int fnamelen = dir_path + l - filename;
	if (fnamelen <= 0)
	{
		return -1;
	}
	if (fnamelen > MAX_FILE_NAME_SIZE)
	{
		perror("creat: very long filename\n");
		return -1;
	}
	inode_t *dir;
	if (*filename == 0)
	{
		if (namei("/", &dir) != 0)
		{
			return -1;
		}
	}
	else
	{
		*filename = 0;
		if (namei(dir_path + 1, &dir) != 0)
		{
			return -1;
		}
	}
	inode_t *fil_inode;
	if (ialloc(&fil_inode) != 0)
	{
		iput(dir);
		return -1;
	}
	add_dir_entry(dir, fil_inode->inode_no, filename + 1);
	/*
		todo: set entries of inode for new file
	*/
	fil_inode->disk_inode.links++;
	fil_inode->disk_inode.type = FT_FIL;
	INO_SET_FIELD(fil_inode, INODE_MODIFIED);
	iput(fil_inode);
	return 0;
}

ssize_t myread(int fd, byte_t *dst, size_t n)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || (file_table[fd].mode & S_OPEN) == 0)
	{
		perror("read: bad file descriptor\n");
		return -1;
	}
	if (!IS_SET(file_table[fd].mode, M_RD))
	{
		// ! no permission to read
		return -1;
	}
	inode_t *inode = file_table[fd].inode;
	offset_t byte_offset, offset = file_table[fd].offset;
	size_t bytes_in_block, read = 0;
	block_no_t block_no;
	buffer_t buffer;
	INO_SET_FIELD(inode, INODE_LOCKED);
	do
	{
		if (bmap(inode, offset, &block_no, &byte_offset, &bytes_in_block) != 0)
		{
			INO_REM_FIELD(inode, INODE_LOCKED);
			if (read > 0)
			{
				file_table[fd].offset += read;
				return read;
			}
			return -1;
		}
		if (n <= bytes_in_block)
		{
			if (bread(block_no, &buffer) != 0)
			{
				INO_REM_FIELD(inode, INODE_LOCKED);
				if (read > 0)
				{
					file_table[fd].offset += read;
					return read;
				}
				perror("read: cannot read block\n");
				return -1;
			}
			memcpy(dst + read, buffer.data->b + byte_offset, n);
			read += n;
			n = 0;
			brelse(&buffer);
			INO_REM_FIELD(inode, INODE_LOCKED);
			file_table[fd].offset += read;
			return read;
		}
		if (bread(block_no, &buffer) != 0)
		{
			INO_REM_FIELD(inode, INODE_LOCKED);
			if (read > 0)
			{
				file_table[fd].offset += read;
				return read;
			}
			perror("read: cannot read block\n");
			return -1;
		}
		memcpy(dst + read, buffer.data->b + byte_offset, bytes_in_block);
		read += bytes_in_block;
		n -= bytes_in_block;
		brelse(&buffer);

	} while (n > 0);
	return -1;
}
ssize_t mywrite(int fd, byte_t *src, size_t n)
{
	if (fd < 0 || fd >= MAX_OPEN_FILES || (file_table[fd].mode & S_OPEN) == 0)
	{
		perror("read: bad file descriptor\n");
		return -1;
	}
	if (!IS_SET(file_table[fd].mode, M_WR))
	{
		// ! no permission to write
		return -1;
	}
	if (IS_SET(file_table[fd].mode, M_APP))
	{
		/* move offset to end for each write operation in append mode */
		mylseek(fd, 0, WH_END);
	}
	inode_t *inode = file_table[fd].inode;
	offset_t byte_offset, offset = file_table[fd].offset;
	size_t bytes_in_block, written = 0;
	block_no_t block_no;
	buffer_t buffer;
	INO_SET_FIELD(inode, INODE_LOCKED);
	if (offset + n > MAX_FILE_SIZE)
	{
		n = MAX_FILE_SIZE - offset;
	}
	if (n == 0)
	{
		INO_REM_FIELD(inode, INODE_LOCKED);
		return 0;
	}
	while (n > 0)
	{
		if (bmap(inode, offset, &block_no, &byte_offset, &bytes_in_block) != 0)
		{
			if (written == 0)
				return -1;
			break;
		}
		offset_t remaining = MY_BLK_SIZE - byte_offset;
		size_t to_write;
		if (n <= remaining)
		{
			to_write = n;
		}
		else
		{
			to_write = remaining;
		}
		if (block_no == 0)
		{
			/*
			 *	cases:
			 *	1) the offset is inside file but the block is 0
			 *	2) the offset is out of the file
			 *
			 */
			if (offset >= inode->disk_inode.size_on_disk)
			{
				/* in this case, the file has to be extended */
				inode->disk_inode.size = offset;
			}
			inode->disk_inode.size_on_disk += MY_BLK_SIZE;
			INO_SET_FIELD(inode, INODE_MODIFIED);
			balloc(&buffer);
		}
		else
			bread(block_no, &buffer);
		/*
		 * cases possible:
		 * 1) write can be completed within this block
		 * 2) we use whole block but write cannot be completed
		 */
		if (offset + to_write > inode->disk_inode.size) /* if write goes beyond file, increase file size */
			inode->disk_inode.size = offset + to_write;
		memcpy(buffer.data->b + byte_offset, src + written, to_write);
		written += to_write;
		offset += to_write;
		n -= to_write;
		BUFF_SET_FIELD(buffer, BUFF_MODIFIED);
		block_no_t physical_block_no = buffer.header->block_no;
		brelse(&buffer);
		if (block_no == 0)
		{
			block_no_t logical_block_no = offset / MY_BLK_SIZE;
			add_physical_block(inode, logical_block_no, physical_block_no);
		}
	}
	file_table[fd].offset += written;
	INO_REM_FIELD(inode, INODE_LOCKED);
	return written;
}