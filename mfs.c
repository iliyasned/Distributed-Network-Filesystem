#include "udp.h"
#include "mfs.h"
#include "msg.h"
#include <time.h>

struct sockaddr_in addrSnd, addrRcv;
int portnum;
int sd;

//takes a host name and port number and uses those to find the server exporting the file system.
int MFS_Init(char *hostname, int port){
    
    portnum = port;

    int MIN_PORT = 20000;
    int MAX_PORT = 40000;

    srand(time(0));
    int rand_port_num = (rand() % (MAX_PORT - MIN_PORT) + MIN_PORT);

    // Bind random client port number
    sd = UDP_Open(rand_port_num);

    int rc = UDP_FillSockAddr(&addrSnd, hostname, portnum);
    assert(rc == 0);
    return 0;
}

int MFS_Lookup(int pinum, char *name){
    msg_t m;
    m.pinum = pinum;
    strcpy(m.name, name);
    m.mtype = LOOKUP;
    int rc = UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));

    fd_set r;
    FD_ZERO(&r);
    FD_SET(sd, &r);
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;
	rc = select(sd + 1, &r, NULL, NULL, &t);
    if(rc <= 0){
        UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    }

    UDP_Read(sd, &addrRcv, (char *) &m, sizeof(msg_t));

    return m.rc;
}

int MFS_Stat(int inum, MFS_Stat_t *m){
    msg_t ms;
    ms.inum = inum;
    //ms.stat = m;
    ms.mtype = STAT;
    int rc = UDP_Write(sd, &addrSnd, (char*)&ms, sizeof(msg_t));

    fd_set r;
    FD_ZERO(&r);
    FD_SET(sd, &r);
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;
   	rc = select(sd + 1, &r, NULL, NULL, &t);
    if(rc <= 0){
        UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    }

    UDP_Read(sd, &addrRcv, (char *) &ms, sizeof(msg_t));

    if(ms.rc != -1){
        m->size = ms.stat.size;
        m->type = ms.stat.type;
    }
    
    return ms.rc;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes){
    msg_t m;

    m.inum = inum;
    memset(m.buffer, 0, 4096);
    memcpy(m.buffer, buffer, nbytes);
    m.offset = offset;
    m.nbytes = nbytes;
    m.mtype = WRITE;
    
    int rc = UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));

	fd_set r;
    FD_ZERO(&r);
    FD_SET(sd, &r);
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;
	rc = select(sd + 1, &r, NULL, NULL, &t);
    if(rc <= 0){
        UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    }

    UDP_Read(sd, &addrRcv, (char *) &m, sizeof(msg_t));
    return m.rc;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    m.inum = inum;
    m.offset = offset;
    m.nbytes = nbytes;
    m.mtype = READ;
    memset(m.buffer, 0, 4096);
    int rc = UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));

	fd_set r;
    FD_ZERO(&r);
    FD_SET(sd, &r);
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;
	rc = select(sd + 1, &r, NULL, NULL, &t);
    if(rc <= 0){
        UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    }
    
    UDP_Read(sd, &addrRcv, (char *) &m, sizeof(msg_t));
    memcpy(buffer, m.buffer, nbytes);
    return m.rc;
}

int MFS_Creat(int pinum, int type, char *name){
    msg_t m;
    m.pinum = pinum;
    strcpy(m.name, name);
    m.creat_type = type;
    m.mtype = CREAT;
    int rc = UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));

	fd_set r;
    FD_ZERO(&r);
    FD_SET(sd, &r);
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;
	rc = select(sd + 1, &r, NULL, NULL, &t);
    if(rc <= 0){
        UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    }

    UDP_Read(sd, &addrRcv, (char *) &m, sizeof(msg_t));

    return m.rc;
}

int MFS_Unlink(int pinum, char *name){
    msg_t m;
    m.pinum = pinum;
    strcpy(m.name, name);
    m.mtype = UNLINK;
    int rc = UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));

	fd_set r;
    FD_ZERO(&r);
    FD_SET(sd, &r);
    struct timeval t;
    t.tv_sec = 5;
    t.tv_usec = 0;
	rc = select(sd + 1, &r, NULL, NULL, &t);
    if(rc <= 0){
        UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    }

    UDP_Read(sd, &addrRcv, (char *) &m, sizeof(msg_t));

    return m.rc;
}

int MFS_Shutdown(){
    msg_t m;
    m.mtype = SHUTDOWN;
    UDP_Write(sd, &addrSnd, (char*)&m, sizeof(msg_t));
    UDP_Close(sd);
    return 0;
}
