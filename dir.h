#include "myfs.h"
#ifndef DIR_H
#define DIR_H
#define DIR_FIXED_ENTRY_SIZE_TYPE 
#ifdef DIR_FIXED_ENTRY_SIZE_TYPE
#define DIR_ENTRY_SIZE (sizeof(dir_entry_t))
#define DIR_ENTRIES_PER_BLOCK (MY_BLK_SIZE/DIR_ENTRY_SIZE)
#endif
#endif