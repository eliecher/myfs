#include "myfs.h"
#ifndef FILECONTROL_H
#define FILECONTROL_H
#define M_DEFAULT_MODE 0b0
#define M_RD 0b1
#define M_WR 0b10
#define M_APP 0b100
#define M_TRUNC 0b1000
#define M_CREAT 0b10000
#define S_OPEN 0b100000
#define M_RDWR (M_RD | M_WR)

#define WH_SET 0
#define WH_CUR 1
#define WH_END 2

#define IS_SET(mode, field) ((mode & (field)) == (field))

#endif