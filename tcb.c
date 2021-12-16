// File:  tcb.h
// List all group member's name: Sunny Chen, Michael Zhao

#include <stdlib.h>
#include "tcb.h"
#include "rpthread.h"


queue_t* new_queue() {
	queue_t *queue = malloc(sizeof(*queue));
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	return queue;
}

/* Put node at end of queue */
void enqueue(queue_t *queue, tcb_t *node) {
	if (node == NULL)
		return;

	if (queue->tail == NULL) {
		queue->head = node;
	}
	else {
		queue->tail->next = node;
	}
	queue->tail = node;
	node->next = NULL;
	queue->size++;
}

/* Remove front node and return pointer */
tcb_t* dequeue(queue_t *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	tcb_t *node = queue->head;
	if (node->next == NULL) {
		queue->tail = NULL;
	}
	queue->head = node->next;
	queue->size--;

	return node;
}

tcb_t* new_tcb(rpthread_t tid, void *(*func_ptr)(void *), void *args) {
	tcb_t *tcb = malloc(sizeof(*tcb));

	tcb->tid = tid;
	tcb->priority = 0;
	tcb->state = READY;
	tcb->uctx = malloc(sizeof(*(tcb->uctx)));

	tcb->last_run = 0;
	tcb->timeslice = TIMESLICE;

	tcb->func_ptr = func_ptr;
	tcb->args = args;
	tcb->retval = NULL;

	tcb->joined = new_queue();
	tcb->next = NULL;

	return tcb;
}

/* setup ucontext for thread and allocate stack */
void setup_tcb_context(ucontext_t *uc, ucontext_t *uc_link, tcb_t *tcb) {
	getcontext(uc);
	uc->uc_stack.ss_sp = malloc(SS_SIZE);
	uc->uc_stack.ss_size = SS_SIZE;
	uc->uc_link = uc_link;

	/* all threads are run inside thread_wrapper() so we can store the retval */
	makecontext(uc, (void (*)())thread_wrapper, 1, tcb);
}

void free_tcb(tcb_t *tcb) {
	free(tcb->uctx->uc_stack.ss_sp);
	free(tcb->uctx);
	free(tcb);
}

void thread_wrapper(tcb_t *tcb) {
	tcb->retval = tcb->func_ptr(tcb->args);  // store retval in tcb
}
