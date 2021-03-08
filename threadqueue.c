#include "threadqueue.h"
#include <stdlib.h>

ThreadQueue* new_queue() {
	ThreadQueue *queue = (ThreadQueue *) malloc(sizeof(*queue));
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	return queue;
}

void enqueue(ThreadQueue *queue, tcb_t *node) {
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

tcb_t* dequeue(ThreadQueue *queue) {
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

void remove_tcb(ThreadQueue *queue, tcb_t *tcb) {
	if (tcb == queue->head) {
		dequeue(queue);
		return;
	}

	if (tcb == queue->tail) {
		tcb_t *prev = NULL;
		tcb_t *curr = queue->head;
		while (curr->next != NULL) {
			prev = curr;
			curr = curr->next;
		}

		prev->next = NULL;
		queue->tail = prev;
		queue->size--;
		return;
	}

	tcb_t *prev = queue->head;
	tcb_t *curr = queue->head->next;
	while (curr != NULL) {
		if (curr == tcb) {
			prev->next = curr->next;
			curr->next = NULL;
			queue->size--;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

tcb_t* peek(ThreadQueue *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	return queue->head;
}


tcb_t* new_tcb(rpthread_t tid, int thread_priority) {
	tcb_t *tcb = malloc(sizeof(*tcb));
	tcb->tid = tid;
	tcb->thread_priority = thread_priority;
	tcb->next = NULL;
	return tcb;
}

void free_tcb(tcb_t *tcb) {
	free(tcb->uctx.uc_stack.ss_sp);
	free(tcb);
}