#include "rpthread.h"

static void schedule();

/********** Queue Implementation **********/
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
	
	return;
}

tcb_t* peek(ThreadQueue *queue) {
	if (queue->size == 0) {
		return NULL;
	}
	return queue->head;
}


tcb_t* new_tcb(rpthread_t tid, int thread_priority) {
	tcb_t *tcb = (tcb_t *) malloc(sizeof(*tcb));
	tcb->tid = tid;
	tcb->thread_priority = thread_priority;
	tcb->next = NULL;
	return tcb;
}

void free_tcb(tcb_t *tcb) {
	free(tcb->uctx.uc_stack.ss_sp);
	free(tcb);
}


void enable_timer();
void disable_timer();

static Scheduler *scheduler;
static struct itimerval itimer, pause_itimer;
static struct sigaction sa;

void print_queue() {
	printf("%d->", scheduler->running->tid);
	tcb_t *curr = scheduler->thread_queue->head;
	while (curr != NULL) {
		printf("%d->", curr->tid);
		curr = curr->next;
	}
	printf("\n");
}

void setup_context(ucontext_t *uc, void (*func)(), ucontext_t *uc_link, void *arg) {
	getcontext(uc);
	uc->uc_stack.ss_sp = malloc(SS_SIZE);
	uc->uc_stack.ss_size = SS_SIZE;
	uc->uc_link = uc_link;

	if (arg != NULL) {
		makecontext(uc, func, 1, arg);
	}
	else {
		makecontext(uc, func, 0);
	}
}


// typedef struct Scheduler {
// 		ThreadQueue *thread_queue;
// 		tcb_t 	   	*running;
//
// 		char		*ts_arr;
// 		uint8_t		 ts_count;
// 		uint8_t		 ts_size;
//
// 		ucontext_t 	 exit_uctx;
//
// } Scheduler;

void init_scheduler() {

	scheduler = (Scheduler *) malloc(sizeof(*scheduler));

	// setup queue
	scheduler->thread_queue = new_queue();

	// create main thread
	tcb_t *main_tcb = new_tcb(0, 0);
	getcontext(&(main_tcb->uctx));
	scheduler->running = main_tcb;

	// setup thread info
	scheduler->ts_arr = (char *) malloc(32 * sizeof(char));
	scheduler->ts_arr[0] = SCHEDULED;  // main thread scheduled
	scheduler->ts_count = 1;
	scheduler->ts_size = 32;

	// setup exit context
	setup_context(&(scheduler->exit_uctx), handle_exit, NULL, NULL);

	//setup itimer
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = TIMESLICE;
	itimer.it_value = itimer.it_interval;

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &handle_timeout;
	sigaction(SIGVTALRM, &sa, NULL);
}


int rpthread_create(rpthread_t *thread, pthread_attr_t *attr,
					void *(*function)(void *), void *arg) {
	if (scheduler == NULL) {
		init_scheduler();
	}
	else {
		disable_timer();
	}

	*thread = scheduler->ts_count;
	scheduler->ts_count++;

	tcb_t *tcb = new_tcb(*thread, 0);
	setup_context(&(tcb->uctx), function, &(scheduler->exit_uctx), arg);

	// resize ts_arr
	if (scheduler->ts_count > scheduler->ts_size) {
		scheduler->ts_size *= 2;
		scheduler->ts_arr = (char *) realloc(scheduler->ts_arr, scheduler->ts_size * sizeof(char));
	}
	scheduler->ts_arr[*thread] = READY;
	enqueue(scheduler->thread_queue, tcb);

	enable_timer();
    return 0;
};

int rpthread_yield() {
	scheduler->ts_arr[scheduler->running->tid] = YIELD;
	schedule();
	return 0;
};

void rpthread_exit(void *value_ptr) {
	handle_exit();
};

int rpthread_join(rpthread_t thread, void **value_ptr) {
	while (scheduler->ts_arr[thread] != FINISHED) {
		rpthread_yield();
	}
	return 0;
};

tcb_t *find_next_ready(ThreadQueue *thread_queue) {
	tcb_t *curr = thread_queue->head;
	while (curr != NULL) {
		if (scheduler->ts_arr[curr->tid] == READY)
			return curr;
		curr = curr->next;
	}
	return NULL;
}

static void schedule() {
	disable_timer();

	tcb_t *old_tcb = scheduler->running;

	// called from handle_exit
	if (scheduler->ts_arr[old_tcb->tid] == FINISHED) {
		free_tcb(old_tcb);
		if (scheduler->thread_queue->size == 0)  // no threads left
			exit(0);

		// load next thread
		tcb_t *next_thread = find_next_ready(scheduler->thread_queue);
		remove_tcb(scheduler->thread_queue, next_thread);
		
		scheduler->running = next_thread;
		scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;

		enable_timer();
		setcontext(&(scheduler->running->uctx));
	}

	// called from handle_timeout
	else {
		if (scheduler->thread_queue->size == 0) {  // skip scheduling, resume current thread
			scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;
			enable_timer();
			return;
		}
		
		scheduler->ts_arr[scheduler->running->tid] = READY;
		enqueue(scheduler->thread_queue, scheduler->running);

		tcb_t *next_thread = find_next_ready(scheduler->thread_queue);
		remove_tcb(scheduler->thread_queue, next_thread);

		scheduler->running = next_thread;
		scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;

		enable_timer();
		swapcontext(&(old_tcb->uctx), &(scheduler->running->uctx));
	}
}


void handle_exit() {
	tcb_t *tcb = scheduler->running;
	scheduler->ts_arr[tcb->tid] = FINISHED;

	schedule();
}

void enable_timer() {
	setitimer(ITIMER_VIRTUAL, &itimer, NULL);
}

void disable_timer() {
	setitimer(ITIMER_VIRTUAL, &pause_itimer, NULL);
}

void handle_timeout(int signum) {
	schedule();
}


void funcA() {
	// int i=0;
    // while (1) {
	// 	printf("a: %d\n", i);
	// 	i++;
	// }

	for (int i=0; i < 100000; i++) {
		printf("a: %d\n", i);
	}
}

void funcB() {
	// int i=0;
    // while (1) {
	// 	printf("b: %d\n", i);
	// 	i++;
	// }

	for (int i=0; i < 100000; i++) {
		printf("b: %d\n", i);
	}
}

int main() {

	rpthread_t a, b;
	
	rpthread_create(&a, NULL, funcA, NULL);
	rpthread_create(&b, NULL, funcB, NULL);

	rpthread_join(a, NULL);
	rpthread_join(b, NULL);

	printf("done\n");

	return 0;
}

