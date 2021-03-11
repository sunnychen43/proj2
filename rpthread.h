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


#define SS_SIZE SIGSTKSZ
#define MLFQ_LEVELS 8

#define READY 0
#define BLOCKED 1
#define FINISHED 2

/* include lib header files that you need here: */
#include <stdbool.h>
#include <signal.h>
#include <ucontext.h>
#include "tcb.h"


typedef struct rpthread_mutex_t {
	unsigned char  lock;
	rpthread_t 	   tid;
	queue_t*       blocked_queue;
} rpthread_mutex_t;


typedef struct Scheduler {
	queue_t*    thread_queues[MLFQ_LEVELS];
	tcb_t*      running;

	tcb_t**     tcb_arr;
	uint8_t		t_count;
	uint8_t		t_max;

	ucontext_t* exit_uctx;

	bool enabled;

} Scheduler;


int  rpthread_create(rpthread_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg);
int  rpthread_yield();
void rpthread_exit(void *value_ptr);
int  rpthread_join(rpthread_t thread, void **value_ptr);

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
