#ifndef THREADQUEUE_H
#define THREADQUEUE_H

#include <ucontext.h>
#include <stdint.h>
#include <time.h>

typedef uint8_t rpthread_t;

typedef struct ThreadQueue {
	struct tcb_t *head;
	struct tcb_t *tail;
	int size;

} ThreadQueue;

typedef struct tcb_t {
        rpthread_t   tid;
        uint8_t      priority;
        char         state;

        void*        retval;
        ThreadQueue *joined;

        ucontext_t   uctx;
        
        void *(*func_ptr)(void *);
        void *args;

        clock_t last_run;
        int timeslice;

        struct tcb_t *next;
} tcb_t;

void enqueue(ThreadQueue *queue, tcb_t *tcb);
tcb_t* dequeue(ThreadQueue *queue);
void remove_tcb(ThreadQueue *queue, tcb_t *tcb);
tcb_t* peek(ThreadQueue *queue);

ThreadQueue* new_queue();
tcb_t* new_tcb(rpthread_t tid, void *(*func_ptr)(void *), void *args);
void free_tcb(tcb_t *tcb);

#endif