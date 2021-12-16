// File:  tcb.h
// List all group member's name: Sunny Chen, Michael Zhao

#ifndef THREADQUEUE_H
#define THREADQUEUE_H

#include <ucontext.h>
#include <stdint.h>
#include <time.h>

typedef uint8_t rpthread_t;

/* queue for tcb nodes */
typedef struct queue_t {
	struct tcb_t *head;
	struct tcb_t *tail;
	int size;
} queue_t;


/* tcb struct, contains all info about a thread */
typedef struct tcb_t {
        /* thread info */
        rpthread_t  tid;
        uint8_t     priority;  /* (high prio) 0 - 7 (low prio) */
        uint8_t     state;     /* states defined in rpthread.h */
        ucontext_t  *uctx;

        /* accounting to prevent gaming */
        clock_t  last_run;  
        int      timeslice;

        /* execution info */
        void*   (*func_ptr)(void *);  
        void*    args;
        void*    retval;

        queue_t* joined; /* threads awaiting */
        struct tcb_t *next;  /* tcbs are stored as LL */
} tcb_t;


/* queue functions */
queue_t*  new_queue();
void      enqueue(queue_t *queue, tcb_t *tcb);
tcb_t*    dequeue(queue_t *queue);


/* tcb functions */
tcb_t*  new_tcb(rpthread_t tid, void *(*func_ptr)(void *), void *args);
void    setup_tcb_context(ucontext_t *uc, ucontext_t *uc_link, tcb_t *tcb);
void    free_tcb(tcb_t *tcb);
void    thread_wrapper(tcb_t *tcb);

#endif