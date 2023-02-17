all: mkfs server mfs.so.l mfs.o client 

mkfs: mkfs.c ufs.h
	gcc mkfs.c -o mkfs

server: server.c ufs.h udp.h udp.c msg.h
	gcc server.c udp.c -o server

mfs.so.l: mfs.o udp.c
	gcc -shared -Wl,-soname,libmfs.so -o libmfs.so mfs.o udp.o -lc

mfs.o: mfs.c udp.c
	gcc -fPIC -g -c -Wall mfs.c udp.c

client: client.c mfs.h udp.c
	gcc client.c udp.c -Wall -L. -lmfs -o client



# CC     := gcc
# CFLAGS := -Wall -Werror 

SRCS   := client.c \
	server.c \
	mfs.c \
	mkfs.c \
	udp.c 

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

# .PHONY: all
# all: ${PROGS}

# ${PROGS} : % : %.o Makefile
# 	${CC} $< -o $@ udp.c

clean:
	rm -f ${PROGS} ${OBJS} libmfs.so 

# %.o: %.c Makefile
# 	${CC} ${CFLAGS} -c $<