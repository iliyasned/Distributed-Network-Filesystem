#ifndef __msg_h__
#define __msg_h__

#include "mfs.h"

#define INIT (0)
#define LOOKUP (1)
#define STAT (2)
#define WRITE (3)
#define READ (4)
#define CREAT (5)
#define UNLINK (6)
#define SHUTDOWN (7)

typedef struct __msg_t {
    int rc;
    int mtype;
    char name[28];
    char buffer[4096];
    int offset;
    int inum;
    int pinum;
    int nbytes;
    int creat_type;
    MFS_Stat_t stat;
} msg_t;

#endif