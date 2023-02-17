#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "ufs.h"
#include "msg.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE (4096)

char name[256]; //name of fs image
int fd; //file descriptor
int sd; //socket descriptor
super_t *s; //super block of fs
struct sockaddr_in addr; //sockaddr in struct
void* image; //mmapped fsimg

int server_lookup(int pinum, char* name);
int server_stat(int inum, MFS_Stat_t m);
int server_write(int inum, char *buffer, int offset, int nbytes);
int server_read(int inum, char *buffer, int offset, int nbytes);
int server_creat(int pinum, int type, char *name);
int server_unlink(int pinum, char *name);
int server_shutdown();

typedef struct {
    dir_ent_t entries[128];
} dir_block_t;

void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

unsigned int get_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |= 0x1 << offset;
}

// server code
int main(int argc, char *argv[])
{
    if(argc != 3){
        fprintf(stderr, "Usage: server [portnum] [file-system-image]\n");
        exit(0);
    }

    int port = atoi(argv[1]);
    strcpy(name, argv[2]);

    sd = UDP_Open(port);
    assert(sd > -1);

    signal(SIGINT, intHandler);

    fd = open(name, O_RDWR);
    if(fd < 0){
        fprintf(stderr, "image does not exist\n");
        exit(1);
    }

    struct stat sbuf;
    int rc = fstat(fd, &sbuf);
    assert(rc > -1);

	int size = (int) sbuf.st_size;

    image = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(image != MAP_FAILED);

    s = (super_t *) image;

	printf("server:: initialized\n");

    while (1)
    {
        msg_t m;
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *)&m, sizeof(msg_t));
        
        if (rc > 0)
        {
            //parse message type and call appropriate server function
            switch (m.mtype)
            {
                case LOOKUP: ;
                    printf("server:: read message of type: LOOKUP\n");
                    server_lookup(m.pinum, m.name);
                    break;
                case STAT: ;
                    printf("server:: read message of type: STAT\n");
                    MFS_Stat_t stat;
                    server_stat(m.inum, stat);
                    break;
                case WRITE: ;
                    printf("server:: read message of type: WRITE\n");
                    server_write(m.inum, m.buffer, m.offset, m.nbytes);
                    break;
                case READ: ;
                    printf("server:: read message of type: READ\n");
                    server_read(m.inum, m.buffer, m.offset, m.nbytes);
                    break;
                case CREAT: ;
                    printf("server:: read message of type: CREAT\n");
                    server_creat(m.pinum, m.creat_type, m.name);
                    break;
                case UNLINK: ;
                    printf("server:: read message of type: UNLINK\n");
                    server_unlink(m.pinum, m.name);
                    break;
                case SHUTDOWN: ;
                    printf("server:: read message of type: SHUTDOWN\n");
                    server_shutdown();
                    break;
            }
        }
    }
    return 0;
}



int server_lookup(int pinum, char* name){
    int rc = -1;
    msg_t m;
    m.rc = -1;

    int numinodes = s->num_inodes;
    if(pinum < 0 || pinum >= numinodes){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    inode_t *inode = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    inode += pinum; 
	
    //if parent inode is not dir
    if(inode->type != MFS_DIRECTORY){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    //look for entry in dir blocks with name 
    int found = 0;
    for (int i = 0; i < DIRECT_PTRS; i++){

        if(inode->direct[i] == -1){
            continue;
        }
	
        dir_block_t *dirb = image + (inode->direct[i] * UFS_BLOCK_SIZE);
        
        for (int j = 0; j < 128; j++){
            if(strcmp(dirb->entries[j].name, name) == 0 && dirb->entries[j].inum != -1){
                rc = dirb->entries[j].inum;
                found = 1;
                break;
            }
        }
        if(found == 1){
            break;
        }
    }
    
    m.rc = rc;
    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
    
    return 0;
}



int server_stat(int inum, MFS_Stat_t m){

    int numinodes = s->num_inodes;
    if(inum < 0 || inum >= numinodes){
        msg_t ms;
        ms.rc = -1;
        UDP_Write(sd, &addr, (char*)&ms, sizeof(msg_t));
        return 0;
    }

    //go to specific inode
    inode_t *inode = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    inode += inum; 

    msg_t ms;
    ms.rc = 0;
    //pointers are weird, so send info through ms.stat variable in msg_t
    ms.stat.size = inode->size;
    ms.stat.type = inode->type;
    UDP_Write(sd, &addr, (char*)&ms, sizeof(msg_t));
    return 0;
}



int server_write(int inum, char *buffer, int offset, int nbytes){

    msg_t m;
    m.rc = -1;

    if(buffer == NULL){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    int numinodes = s->num_inodes;
    if(inum < 0 || inum >= numinodes || offset < 0 || nbytes < 0 || nbytes > 4096){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }
    //if trying to write to more than max file size
    if(offset+nbytes > DIRECT_PTRS*UFS_BLOCK_SIZE){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    inode_t *inode = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    inode += inum;

    //cant write to dir
    if(inode->type == MFS_DIRECTORY){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    //direct block index
    int blockindex = offset / UFS_BLOCK_SIZE;

    if(blockindex >= DIRECT_PTRS){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    //if -1 then need to allocate data block to write 
    if(inode->direct[blockindex] == -1){
        
        //look for open data block
        int dataix = -1;
        for (int i = 0; i < s->num_data; i++){
            int bit = get_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), i);
            if(bit == 0){
                dataix = i;
                set_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), i);
                break;
            }
        }
        if(dataix == -1){
            UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
            return 0;
        }
        inode->direct[blockindex] = dataix + s->data_region_addr;
    }

    //offset into specific direct block
    int blockoffset = offset % UFS_BLOCK_SIZE;

    //need to write to two blocks
    if((blockoffset + nbytes) > UFS_BLOCK_SIZE){

        //some math to know how many bytes to write first and next
        int nextnumbytes = (blockoffset + nbytes) - UFS_BLOCK_SIZE;
        int numbytesfirst = nbytes - nextnumbytes;

        pwrite(fd, buffer, numbytesfirst, 
                (UFS_BLOCK_SIZE * inode->direct[blockindex]) + blockoffset);


        //NO NEED TO ALLOCATE DATA BLOCK if not -1
        if(inode->direct[blockindex +1] != -1){
            pwrite(fd, buffer + numbytesfirst, nextnumbytes, 
                UFS_BLOCK_SIZE * inode->direct[blockindex+1]);
        } 

        //NEED TO ALLOCATE DATA BLOCK since -1
        else{
            //look for open data block
            int dataix = -1;
            for (int i = 0; i < s->num_data; i++){
                int bit = get_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), i);
                if(bit == 0){
                    dataix = i;
                    set_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), i);
                    break;
                }
            }
            if(dataix == -1){
                UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
                return 0;
            }
            inode->direct[blockindex +1] = dataix + s->data_region_addr;
            
            pwrite(fd, buffer + numbytesfirst, nextnumbytes, 
                UFS_BLOCK_SIZE * inode->direct[blockindex+1]);
            
        }
        
    
    } 
    //only need to write to one block
    else{
        pwrite(fd, buffer, nbytes, (UFS_BLOCK_SIZE * inode->direct[blockindex]) + blockoffset);
    }

    //increase size of inode
    if((offset + nbytes) > inode->size){
        inode->size = offset + nbytes;
    }

    fsync(fd);

    m.rc = 0;
    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));

    return 0;
}

int server_read(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    m.rc = -1;

    int numinodes = s->num_inodes;
    if(inum < 0 || inum >= numinodes || offset < 0 || nbytes < 0 || nbytes > 4096){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    inode_t *inode = image + (s->inode_region_addr * UFS_BLOCK_SIZE);

    inode += inum; 

    //if trying to read beyond inode size
    if(inode->size < offset || offset+nbytes > inode->size){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    //get direct block index
    int blockindex = offset / UFS_BLOCK_SIZE;

    if(blockindex >= DIRECT_PTRS){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    //if the index is not allocated return failure
    if(inode->direct[blockindex] == -1){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }
    //if reading more than max file size
    if(offset+nbytes > DIRECT_PTRS*UFS_BLOCK_SIZE){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }
    //if we need to read from two blocks but the next direct block is not allocated
    if(offset + nbytes > UFS_BLOCK_SIZE && inode->direct[blockindex +1] == -1){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }
    
    //some math to get offset into specific block and how many bytes to read 
    int blockoffset = offset % UFS_BLOCK_SIZE;
    int nextnumbytes = (blockoffset + nbytes) - UFS_BLOCK_SIZE;
    int numbytesfirst = nbytes - nextnumbytes;

    memset(m.buffer, 0, 4096);

    //need to read from 2 blocks 
    if(offset + nbytes > UFS_BLOCK_SIZE){
        
        pread(fd, m.buffer, numbytesfirst, UFS_BLOCK_SIZE * inode->direct[blockindex] + blockoffset);
        pread(fd, m.buffer + numbytesfirst, nextnumbytes, UFS_BLOCK_SIZE * inode->direct[blockindex + 1]);
        
    }
    //just read from one block
    else{
        pread(fd, m.buffer, nbytes, UFS_BLOCK_SIZE * inode->direct[blockindex] + blockoffset);
    }
    
    m.rc = 0;
    
    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));

    return 0;
}


int server_creat(int pinum, int type, char *name){
    msg_t m;
    m.rc = -1;

    int numinodes = s->num_inodes;
    if(pinum < 0 || pinum >= numinodes || name == NULL || strlen(name)+1 > 28){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    inode_t *inode = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
	
    inode += pinum; 

    if(inode->type != MFS_DIRECTORY){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    int inodeix = -1;
    int found = 0;
    
    //find open dir ent with inum -1
    for (int i = 0; i < DIRECT_PTRS; i++){

		//if we get here then we need to allocate a data block for this directory inode
        if(inode->direct[i] == -1){
        	
            int dataix = -1;
            for (int k = 0; k < s->num_data; k++){
            	int bit = get_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), k);
            	if(bit == 0){
            		dataix = k;
                	set_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), k);
                	break;
            	}
          	}
            if(dataix == -1){
               	UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
                return 0;
           	}
           	inode->direct[i] = dataix + s->data_region_addr;
           	dir_block_t *dirb = image + (inode->direct[i] * UFS_BLOCK_SIZE);
           	for(int k = 0; k < 128; k++)
           		dirb->entries[k].inum = -1;
        }

        dir_block_t *dirb = image + (inode->direct[i] * UFS_BLOCK_SIZE);
        for (int j = 0; j < 128; j++){
            
            //if the file or dir already exists return success
            if(dirb->entries[j].inum != -1) {
                if(strcmp(dirb->entries[j].name, name) == 0){
                    m.rc = 0;
                    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
                    return 0;
                }
            }
            
            if(dirb->entries[j].inum == -1){
                
                //find open inode spot in inode bitmap
                for (int k = 0; k < s->num_inodes; k++){
                    int bit = get_bit(image + (s->inode_bitmap_addr * UFS_BLOCK_SIZE), k);
                    if(bit == 0){
                        inodeix = k;
                        set_bit(image + (s->inode_bitmap_addr * UFS_BLOCK_SIZE), k);
                        break;
                    }   
                }
                
                if(inodeix == -1){
                    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
                    return 0;
                }

                strcpy(dirb->entries[j].name, name);
                dirb->entries[j].inum = inodeix;
                found = 1;
                break;
            }
        }
        if(found == 1){
            break;
        }
    }
    
    inode_t *newinode =  image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    newinode += inodeix; 

    //CREATING DIR OR FILE MAKES A DIFFERENCE

    if(type == MFS_REGULAR_FILE){
        newinode->type = type;
        newinode->size = 0;
        for (int i = 0; i < DIRECT_PTRS; i++) 
           newinode->direct[i] = -1;
    } 

    else{
        //find open data block 
        int dataix = -1;
        for (int i = 0; i < s->num_data; i++){
            int bit = get_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), i);
            if(bit == 0){
                dataix = i;
                set_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), i);
                break;
            }
        }

        if(dataix == -1){
            UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
            return 0;
        }

        newinode->type = type;
        newinode->size = 2 * sizeof(dir_ent_t);
        for (int i = 1; i < DIRECT_PTRS; i++){
            newinode->direct[i] = -1;
        }

        //empty directories have . and ..
        newinode->direct[0] = dataix + s->data_region_addr;

        dir_block_t *dirb = image + (newinode->direct[0] * UFS_BLOCK_SIZE);
        strcpy(dirb->entries[0].name, ".");
        dirb->entries[0].inum = inodeix;

        strcpy(dirb->entries[1].name, "..");
        dirb->entries[1].inum = pinum;

        for (int i = 2; i < 128; i++)
            dirb->entries[i].inum = -1;

    }

	inode->size += sizeof(dir_ent_t);

	fsync(fd);
    m.rc = 0;
    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));

    return 0;
}


int server_unlink(int pinum, char *name){
    msg_t m;
    m.rc = -1;
    int numinodes = s->num_inodes;
    if(pinum < 0 || pinum >= numinodes || name == NULL){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    inode_t *inode = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    inode += pinum; 

    //if parent inode is not directory or size is 0 return failure
    if(inode->type != MFS_DIRECTORY || inode->size == 0){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    int found = 0;
    int inodeix = -1;

    //find file or dir to remove
    for (int i = 0; i < DIRECT_PTRS; i++){
        if(inode->direct[i] == -1){
            continue;
        }
        dir_block_t *dirb = image + (inode->direct[i] * UFS_BLOCK_SIZE);
        for (int j = 0; j < 128; j++){
            if(strcmp(dirb->entries[j].name, name) == 0 && dirb->entries[j].inum != -1){
                found = 1;
                inodeix = dirb->entries[j].inum;
                dirb->entries[j].inum = -1;
                break;
            }
        }
    }

    //did not find dir or file to remove, just return success
    if(found != 1){
    	m.rc = 0;
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    inode_t *inodetoclean = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
    inodetoclean += inodeix; 

	int emptydirsize = 2 * sizeof(MFS_DirEnt_t);

    //if were removing dir and its not empty then return failure
    if(inodetoclean->type == MFS_DIRECTORY && inodetoclean->size != emptydirsize){
        UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));
        return 0;
    }

    for (int i = 0; i < DIRECT_PTRS; i++){
        //if direct points to a allocated block, clear it and clear its bit in data bitmap
        if(inodetoclean->direct[i] != -1){
            set_bit(image + (s->data_bitmap_addr * UFS_BLOCK_SIZE), inodetoclean->direct[i] - s->data_region_addr);
            unsigned char *empty_buffer;
            empty_buffer = calloc(UFS_BLOCK_SIZE, 1);
            if (empty_buffer == NULL){
                perror("calloc");
                exit(1);
            }
            pwrite(fd, empty_buffer, UFS_BLOCK_SIZE, inodetoclean->direct[i] * UFS_BLOCK_SIZE);
            free(empty_buffer);
        }
        inodetoclean->direct[i] = -1;
    }

    inodetoclean->type = -1;
    inodetoclean->size = 0;
    
    //set bit in inode bitmap to clear it
    set_bit(image + (UFS_BLOCK_SIZE * s->inode_bitmap_addr), inodeix);

    inode->size -= sizeof(MFS_DirEnt_t);

    fsync(fd);

    m.rc = 0;
    UDP_Write(sd, &addr, (char*)&m, sizeof(msg_t));

    return 0;
}


int server_shutdown(){
    fsync(fd);
    close(fd);
    UDP_Close(sd);
    exit(0);
}


