#include "myfs.h"
#ifndef INODE_H
#define INODE_H
#define INDEX_SIZE (MY_BLK_SIZE / sizeof(block_no_t))

#define MAX_ACTIVE_INODES 16
#define INODE_DEFAULT_STATUS 0b0
#define INODE_ACTIVE 0b1
#define INODE_LOCKED 0b10
#define INODE_MODIFIED 0b100
#define FT_NONE 0b0
#define FT_DIR 0b1
#define FT_FIL 0b10


#define INODES_PER_BLOCK (MY_BLK_SIZE) / (DISK_INODE_SIZE)
#define SIZ_0DEG_INDEX ((offset_t)MY_BLK_SIZE)
#define SIZ_1DEG_INDEX (INDEX_SIZE * SIZ_0DEG_INDEX)
#define SIZ_2DEG_INDEX (INDEX_SIZE * SIZ_1DEG_INDEX)
#define SIZ_3DEG_INDEX (INDEX_SIZE * SIZ_2DEG_INDEX)
#define CAP_0DEG_INDEX (SIZ_0DEG_INDEX * NUM_0DEG_INDEX)
#define CAP_1DEG_INDEX (SIZ_1DEG_INDEX * NUM_1DEG_INDEX)
#define CAP_2DEG_INDEX (SIZ_2DEG_INDEX * NUM_2DEG_INDEX)
#define CAP_3DEG_INDEX (SIZ_3DEG_INDEX * NUM_3DEG_INDEX)

#define MAX_FILE_SIZE (CAP_0DEG_INDEX + CAP_1DEG_INDEX + CAP_2DEG_INDEX + CAP_3DEG_INDEX)

#define INODE_NO_TO_BLOCK_NO(ino) (NUM_SUPER_BLOCKS + ((ino)-1) / INODES_PER_BLOCK)
#define INODE_NO_TO_BYTE_OFF(ino) (((ino)-1) % INODES_PER_BLOCK * DISK_INODE_SIZE)

#define INO_SET_FIELD(inoptr,field) ((inoptr)->status |= (field))
#define INO_REM_FIELD(inoptr,field) ((inoptr)->status &= (~field))
#define INO_IS_SET(inoptr,field) (((inoptr)->status & (field)) == (field))

extern void clear_inode(disk_inode_t*);
#endif
