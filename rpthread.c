#include "rpthread.h"


/********** Queue Implementation **********/
ThreadQueue* new_queue() {
	ThreadQueue *queue = (ThreadQueue *) malloc(sizeof(*queue));
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	return queue;
}

void enqueue(ThreadQueue *queue, tcb_t *tcb) {
	ThreadNode *node = (ThreadNode *) malloc(sizeof(*node));
	node->tcb = tcb;
	node->next = NULL;

	if (queue->tail == NULL) {
		queue->head = node;
	}
	else {
		queue->tail->next = node;
	}
	queue->tail = node;
	queue->size++;
}

tcb_t* dequeue(ThreadQueue *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	tcb_t *tcb = queue->head->tcb;
	ThreadNode *node = queue->head;
	if (node->next == NULL) {
		queue->tail = NULL;
	}
	queue->head = node->next;
	queue->size--;
	free(node);

	return tcb;
}

tcb_t* peek(ThreadQueue *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	return queue->head->tcb;
}


tcb_t *new_tcb(rpthread_t tid, int thread_state) {
	tcb_t *tcb = (tcb_t *) malloc(sizeof(*tcb));
	tcb->tid = tid;
	tcb->thread_state = thread_state;
	tcb->thread_priority = 0;
	return tcb;
}


static scheduler_t *scheduler;


void init_scheduler() {

	scheduler = (scheduler_t *) malloc(sizeof(*scheduler));

	// setup scheduler context
	ucontext_t *sch_uctx = &(scheduler->sch_uctx);
	getcontext(sch_uctx);
	sch_uctx->uc_link = NULL;
	sch_uctx->uc_stack.ss_sp = scheduler->stack;
	sch_uctx->uc_stack.ss_size = sizeof(scheduler->stack);
	makecontext(sch_uctx, &schedule, 0);

	tcb_t *main_tcb = new_tcb(0, SCHEDULED);
	getcontext(&(main_tcb->uctx));
	scheduler->running = main_tcb;

	scheduler->tqueue = new_queue();
}


int rpthread_create(rpthread_t *thread, pthread_attr_t *attr,
					void *(*function)(void *), void *arg) {
	
	if (scheduler == NULL) {init_scheduler();}

	
	
    return 0;
};

int rpthread_yield() {

	return 0;
};

void rpthread_exit(void *value_ptr) {

};

int rpthread_join(rpthread_t thread, void **value_ptr) {

	return 0;
};

static void schedule() {

}

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquiring mutex fails, push current thread into block list and //  
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init

	return 0;
};



/* Round Robin (RR) scheduling algorithm */
static void sched_rr() {
	// Your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

