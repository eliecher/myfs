#include "myfs.h"
#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H
#define BUFF_MODIFIED 0b1
#define BUFF_VALIDDATA 0b100
#define BUFF_OCCUPIED 0b10
#define BUFF_DEFAULT_STATUS 0b0


#define BUFF_SET_FIELD(buffer,field) ((buffer).header->status |= (field))
#define BUFF_REM_FIELD(buffer,field) ((buffer).header->status &= ~(field))
#endif