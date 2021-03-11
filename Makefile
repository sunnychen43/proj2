CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib

SCHED = MLFQ
TSLICE=15 ##timeslice variable

all: rpthread.a

rpthread.a: rpthread.o threadqueue.o
	$(AR) librpthread.a rpthread.o threadqueue.o
	$(RANLIB) librpthread.a

rpthread.o: rpthread.h
threadqueue.o: threadqueue.h

ifeq ($(SCHED), RR)
	$(CC) -pthread $(CFLAGS) rpthread.c -DTIMESLICE=$(TSLICE)
	$(CC) $(CFLAGS) threadqueue.c
else ifeq ($(SCHED), MLFQ)
	$(CC) -pthread $(CFLAGS) rpthread.c -DMLFQ -DTIMESLICE=$(TSLICE)
	$(CC) $(CFLAGS) threadqueue.c
else
	echo "no such scheduling algorithm"
endif

clean:
	rm -rf testfile *.o *.a
