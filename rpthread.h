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
#define FINISHED 3
#define YIELD 4

#define SS_SIZE 16384

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef uint8_t rpthread_t;

typedef struct tcb_t {
        rpthread_t      tid;
        uint8_t         thread_priority;

        ucontext_t      uctx;
        struct tcb_t *next;

} tcb_t;

typedef struct ThreadQueue {
	struct tcb_t *head;
	struct tcb_t *tail;
	int size;

} ThreadQueue;

typedef struct rpthread_mutex_t {
	unsigned char lock;
	rpthread_t tid;
	ThreadQueue *blocked_queue;
} rpthread_mutex_t;

typedef struct Scheduler {
	ThreadQueue *thread_queue;
	tcb_t 	   	*running;

	char		*ts_arr;
	uint8_t		 ts_count;
	uint8_t		 ts_size;

	uint8_t		 mut_count;
	uint8_t		 mut_size;

	ucontext_t 	 exit_uctx;

} Scheduler;

ThreadQueue* new_queue();
void enqueue(ThreadQueue *queue, tcb_t *tcb);
tcb_t* dequeue(ThreadQueue *queue);
tcb_t* peek(ThreadQueue *queue);
tcb_t* new_node(tcb_t *tcb);

int rpthread_create(rpthread_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg);
int rpthread_yield();
void rpthread_exit(void *value_ptr);
int rpthread_join(rpthread_t thread, void **value_ptr);

int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int rpthread_mutex_lock(rpthread_mutex_t *mutex);
int rpthread_mutex_unlock(rpthread_mutex_t *mutex);
int rpthread_mutex_destroy(rpthread_mutex_t *mutex);

void handle_timeout(int signum);
void handle_exit();


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
