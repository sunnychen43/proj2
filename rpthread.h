// File:	rpthread_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef RTHREAD_T_H
#define RTHREAD_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_RTHREAD macro */
#define USE_RTHREAD 1

#ifndef TIMESLICE
/* defined timeslice to 5 ms, feel free to change this while testing your code
 * it can be done directly in the Makefile*/
#define TIMESLICE 5
#endif

#define READY 0
#define SCHEDULED 1
#define BLOCKED 2

#define SSIZE 16384

typedef unsigned int rpthread_t;

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>

typedef struct thread_control_block {
        rpthread_t tid;
        int thread_state;
        int thread_priority;
	ucontext_t context;
	void *(*function)(void *);
	int done;
} tcb_t;


typedef struct ThreadNode {
	tcb_t *tcb;
	struct ThreadNode *next;
} ThreadNode;

typedef struct ThreadQueue {
	struct ThreadNode *head, *tail;
	int size;
} ThreadQueue;

typedef struct scheduler_t {
	ucontext_t context;
	char stack[SIGSTKSZ];
	tcb_t *running;
	ThreadQueue *tqueue;
} scheduler_t;

typedef struct rpthread_mutex_t {

} rpthread_mutex_t;



ThreadQueue* new_queue();
void enqueue(ThreadQueue *queue, tcb_t *tcb);
tcb_t* dequeue(ThreadQueue *queue);
tcb_t* peek(ThreadQueue *queue);

int rpthread_create(rpthread_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg);
int rpthread_yield();
void rpthread_exit(void *value_ptr);
int rpthread_join(rpthread_t thread, void **value_ptr);

int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int rpthread_mutex_lock(rpthread_mutex_t *mutex);
int rpthread_mutex_unlock(rpthread_mutex_t *mutex);
int rpthread_mutex_destroy(rpthread_mutex_t *mutex);



#ifdef USE_RTHREAD
#define pthread_t rpthread_t
#define pthread_mutex_t rpthread_mutex_t
#define pthread_create rpthread_create
#define pthread_exit rpthread_exit
#define pthread_join rpthread_join
#define pthread_mutex_init rpthread_mutex_init
#define pthread_mutex_lock rpthread_mutex_lock
#define pthread_mutex_unlock rpthread_mutex_unlock
#define pthread_mutex_destroy rpthread_mutex_destroy
#endif

#endif
