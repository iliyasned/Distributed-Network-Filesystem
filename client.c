#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "msg.h"


#define BUFFER_SIZE (1000)


// client code
int main(int argc, char *argv[])
{
    MFS_Init("localhost", 2000);
    printf("%d\n",MFS_Creat(0, MFS_REGULAR_FILE, "test"));
    printf("%d\n", MFS_Lookup(0, "test"));
    MFS_Shutdown();
    // struct sockaddr_in addrSnd, addrRcv;

    // int sd = UDP_Open(20000);
    // int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    // //char message[BUFFER_SIZE];
    // msg_t m;
    // m.type = 100;

    // sprintf(m.buffer, "hello world");

    // printf("client:: send message [%s]\n", m.buffer);
    // rc = UDP_Write(sd, &addrSnd, (char *) &m, sizeof(msg_t));

    // if (rc < 0)
    // {
    //     printf("client:: failed to send\n");
    //     exit(1);
    // }

    // fd_set r;
    // FD_ZERO(&r);
    // FD_SET(sd, &r);
    // struct timeval t;
    // t.tv_sec = 5;
    // t.tv_usec = 0;

    // rc = select(sd + 1, &r, NULL, NULL, &t);

    // if (rc <= 0)
    // {
    //     printf("client:: timed out\n");
    //     exit(1);
    // }

    // printf("client:: wait for reply...\n");
    // rc = UDP_Read(sd, &addrRcv, (char *) &m, sizeof(msg_t));
    // printf("client:: got reply [size:%d contents:(%s)\n", rc, m.buffer);
    return 0;
}
