#include "rpthread.h"


/********** Queue Implementation **********/
ThreadQueue* new_queue() {
	ThreadQueue *queue = (ThreadQueue *) malloc(sizeof(*queue));
	queue->head = NULL;
	queue->tail = NULL;
	queue->size = 0;
	return queue;
}

void enqueue(ThreadQueue *queue, ThreadNode *node) {
	if (queue->tail == NULL) {
		queue->head = node;
	}
	else {
		queue->tail->next = node;
	}
	queue->tail = node;
	queue->size++;
}

ThreadNode* dequeue(ThreadQueue *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	ThreadNode *node = queue->head;
	if (node->next == NULL) {
		queue->tail = NULL;
	}
	queue->head = node->next;
	queue->size--;

	return node;
}

ThreadNode* peek(ThreadQueue *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	return queue->head;
}

ThreadNode* new_node(tcb_t *tcb) {
	ThreadNode *node = (ThreadNode *) malloc(sizeof(*node));
	node->tcb = tcb;
	node->next = NULL;
	return node;
}


tcb_t *new_tcb(rpthread_t tid, int thread_state, int thread_priority) {
	tcb_t *tcb = (tcb_t *) malloc(sizeof(*tcb));
	tcb->tid = tid;
	tcb->thread_state = thread_state;
	tcb->thread_priority = thread_priority;
	return tcb;
}


void handle_timeout(int signum);
void handle_exit();


static scheduler_t *scheduler;
struct itimerval itimer;

static void schedule();


void init_scheduler() {

	scheduler = (scheduler_t *) malloc(sizeof(*scheduler));

	// setup scheduler context
	ucontext_t *sch_uctx = &(scheduler->sch_uctx);
	getcontext(sch_uctx);
	sch_uctx->uc_stack.ss_sp = malloc(16384);
	sch_uctx->uc_stack.ss_size = 16384;
	sch_uctx->uc_link = NULL;
	makecontext(sch_uctx, schedule, 0);

	ucontext_t *exit_uctx = &(scheduler->exit_uctx);
	getcontext(exit_uctx);
	sch_uctx->uc_stack.ss_sp = malloc(1000);
	sch_uctx->uc_stack.ss_size = 1000;
	sch_uctx->uc_link = NULL;
	makecontext(sch_uctx, handle_exit, 0);

	// create main thread
	tcb_t *main_tcb = new_tcb(0, SCHEDULED, 0);
	getcontext(&(main_tcb->uctx));
	ThreadNode *main_node = new_node(main_tcb);
	scheduler->running = main_node;

	scheduler->tqueue = new_queue();
	scheduler->thread_counter = 1;

	signal(SIGALRM, handle_timeout);
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 20000;
	itimer.it_value = itimer.it_interval;
	setitimer(ITIMER_REAL, &itimer, NULL);
}


int rpthread_create(rpthread_t *thread, pthread_attr_t *attr,
					void *(*function)(void *), void *arg) {
	
	if (scheduler == NULL) {init_scheduler();}

	tcb_t *tcb = new_tcb(scheduler->thread_counter, READY, 0);
	ucontext_t *uctx = &(tcb->uctx);
	getcontext(uctx);
	uctx->uc_stack.ss_sp = malloc(SSIZE);
	uctx->uc_stack.ss_size = SSIZE;
	uctx->uc_link = &(scheduler->exit_uctx);
	makecontext(uctx, function, 1, arg);

	scheduler->thread_counter++;
	enqueue(scheduler->tqueue, new_node(tcb));
	
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
	printf("a\n");

	tcb_t *running_tcb = scheduler->running->tcb;

	if (running_tcb->thread_state == FINISHED) {
		free(running_tcb->uctx.uc_stack.ss_sp);
		free(running_tcb);
		free(scheduler->running);
		scheduler->running = NULL;
		if (scheduler->tqueue->size == 0) {return;}
	}

	if (scheduler->tqueue->size > 0) {
		enqueue(scheduler->tqueue, scheduler->running);
		scheduler->running = dequeue(scheduler->tqueue);
	}

	swapcontext(&(running_tcb->uctx), &(scheduler->running->tcb->uctx));
}


void handle_exit() {
	tcb_t *tcb = scheduler->running->tcb;
	tcb->thread_state = FINISHED;
	schedule();
}

void handle_timeout(int signum) {
	printf("timeout\n");
	schedule();
}


void funcA() {
	int i=0;
    while (1) {
		printf("a: %d\n", i);
		i++;
	}
}

void funcB() {
	int i=0;
    while (1) {
		printf("b: %d\n", i);
		i++;
	}
}

int main() {

	rpthread_create(NULL, NULL, funcA, NULL);
	rpthread_create(NULL, NULL, funcB, NULL);

	while (1) {
		printf("main\n");
	}

	return 0;
}

