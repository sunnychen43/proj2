#include "rpthread.h"

static void schedule();

/********** Queue Implementation **********/

void handle_timeout(int signum);
void enable_timer();
void disable_timer();

static Scheduler *scheduler;
static struct itimerval itimer, pause_itimer;
static struct sigaction sa;
static bool enabled;


void thread_wrapper(tcb_t *tcb) {
	scheduler->ret_arr[tcb->tid] = tcb->func_ptr(tcb->args);
}

void setup_tcb_context(ucontext_t *uc, ucontext_t *uc_link, tcb_t *tcb) {
	getcontext(uc);
	uc->uc_stack.ss_sp = malloc(SS_SIZE);
	uc->uc_stack.ss_size = SS_SIZE;
	uc->uc_link = uc_link;
	makecontext(uc, thread_wrapper, 1, tcb);
}


// typedef struct Scheduler {
// 		ThreadQueue *thread_queue;
// 		tcb_t 	   	*running;
//
// 		char		*ts_arr;
// 		uint8_t		 t_count;
// 		uint8_t		 ts_size;
//
// 		ucontext_t 	 exit_uctx;
//
// } Scheduler;

void init_scheduler() {

	scheduler = (Scheduler *) malloc(sizeof(*scheduler));

	// setup queue
	for (int i=0; i < MLFQ_LEVELS; i++) {
		scheduler->thread_queues[i] = new_queue();
	}

	// create main thread
	tcb_t *main_tcb = new_tcb(0, NULL, NULL);
	scheduler->running = main_tcb;
	getcontext(&(main_tcb->uctx));

	// setup thread info
	scheduler->ts_arr = malloc(32 * sizeof(char));
	scheduler->ts_arr[0] = SCHEDULED;  // main thread scheduled

	scheduler->ret_arr = malloc(32 * sizeof(*(scheduler->ret_arr)));
	scheduler->ret_arr[0] = NULL;

	scheduler->join_arr = malloc(32 * sizeof(*(scheduler->join_arr)));
	scheduler->join_arr[0] = new_queue();

	scheduler->t_count = 1;
	scheduler->t_max = 32;

	// setup exit context
	getcontext(&(scheduler->exit_uctx));
	scheduler->exit_uctx.uc_stack.ss_sp = malloc(SS_SIZE);
	scheduler->exit_uctx.uc_stack.ss_size = SS_SIZE;
	scheduler->exit_uctx.uc_link = NULL;
	makecontext(&(scheduler->exit_uctx), rpthread_exit, 0);

	//setup itimer
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = TIMESLICE*1000;
	itimer.it_value = itimer.it_interval;

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &handle_timeout;
	sigaction(SIGPROF, &sa, NULL);
	setitimer(ITIMER_PROF, &itimer, NULL);
	disable_timer();
}


int rpthread_create(rpthread_t *thread, pthread_attr_t *attr,
					void *(*function)(void *), void *arg) {
	if (scheduler == NULL) {
		init_scheduler();
	}
	else {
		disable_timer();
	}

	*thread = scheduler->t_count;
	scheduler->t_count++;

	tcb_t *tcb = new_tcb(*thread, function, arg);
	setup_tcb_context(&(tcb->uctx), &(scheduler->exit_uctx), tcb);

	// resize ts_arr
	if (scheduler->t_count > scheduler->t_max) {
		scheduler->t_max *= 2;
		scheduler->ts_arr = realloc(scheduler->ts_arr, scheduler->t_max * sizeof(char));
		scheduler->ret_arr = realloc(scheduler->ret_arr, scheduler->t_max * sizeof(*(scheduler->ret_arr)));
		scheduler->join_arr = realloc(scheduler->join_arr, scheduler->t_max * sizeof(*(scheduler->join_arr)));
	}
	scheduler->ts_arr[*thread] = READY;
	scheduler->ret_arr[*thread] = NULL;
	scheduler->join_arr[*thread] = new_queue();

	enqueue(scheduler->thread_queues[0], tcb);

	enable_timer();
    return 0;
};

int rpthread_yield() {
	scheduler->ts_arr[scheduler->running->tid] = YIELD;
	schedule();
	return 0;
};

void rpthread_exit(void *value_ptr) {
	scheduler->ts_arr[scheduler->running->tid] = FINISHED;
	schedule();
};

int rpthread_join(rpthread_t thread, void **value_ptr) {
	if (scheduler->ts_arr[thread] != FINISHED) {
		scheduler->ts_arr[scheduler->running->tid] = BLOCKED;
		enqueue(scheduler->join_arr[thread], scheduler->running);
		schedule();
	}

	if (value_ptr != NULL) {
		*value_ptr = scheduler->ret_arr[thread];
	}

	return 0;
};



/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex
	mutex->lock = 0;
	mutex->tid = -1;
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
	while (__sync_lock_test_and_set(&(mutex->lock), 1) == 1) {
		tcb_t *running = scheduler->running;
		if (running->thread_priority < MLFQ_LEVELS-1) {
			running->thread_priority++;
		}
		rpthread_yield();
	}
	mutex->tid = scheduler->running->tid;
	return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	if (mutex->tid == scheduler->running->tid) {
		__sync_lock_test_and_set(&(mutex->lock), 0);
	}
	else {
		printf("mutex error\n");
	}
	
	return 0;
};


/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
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

tcb_t* mlfq_find_next_ready() {
	for (int i=0; i < MLFQ_LEVELS; i++) {
		ThreadQueue *tq = scheduler->thread_queues[i];
		if (tq->size == 0)
			continue;
		
		tcb_t *next_thread = find_next_ready(tq);
		if (next_thread == NULL)
			continue;

		remove_tcb(tq, next_thread);
		return next_thread;
	}
	return NULL;
}

void print_queue(tcb_t *head) {
	while (head != NULL) {
		printf("%d->", head->tid);
		head = head->next;
	}
	printf("\n");
}

static void schedule() {
	disable_timer();
	tcb_t *old_tcb = scheduler->running;

	// for (int i=0; i < MLFQ_LEVELS; i++) {
	// 	printf("%d ", scheduler->thread_queues[i]->size);
	// }
	// printf("\n");
	// if (scheduler->thread_queues[0]->size > 0)
	// 	print_queue(scheduler->thread_queues[0]->head);
	// printf("%d\n", scheduler->running->tid);
	// called from handle_exit
	if (scheduler->ts_arr[old_tcb->tid] == FINISHED) {
		tcb_t *curr = scheduler->join_arr[old_tcb->tid]->head;
		while (curr != NULL) {
			scheduler->ts_arr[curr->tid] = READY;
			enqueue(scheduler->thread_queues[curr->thread_priority], curr);
			curr = curr->next;
		}

		free_tcb(old_tcb);

		// load next thread
		tcb_t *next_thread = mlfq_find_next_ready();
		if (next_thread == NULL) {
			for (int i=0; i < MLFQ_LEVELS; i++) {
				free(scheduler->thread_queues[i]);
			}
			exit(0);
		}
		
		scheduler->running = next_thread;
		scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;

		enable_timer();
		setcontext(&(scheduler->running->uctx));
	}
	else if (scheduler->ts_arr[old_tcb->tid] == BLOCKED) {
		tcb_t *next_thread = mlfq_find_next_ready();

		scheduler->running = next_thread;
		scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;

		enable_timer();
		swapcontext(&(old_tcb->uctx), &(scheduler->running->uctx));
	}
	else {
		bool first = true;
		for (int i = old_tcb->thread_priority; i >= 0; i--) {
			if (scheduler->thread_queues[i]->size > 0) {
				first = false;
				break;
			}
		}
		if (first) {
			scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;
			enable_timer();
			return;
		}

		scheduler->ts_arr[scheduler->running->tid] = READY;
		enqueue(scheduler->thread_queues[scheduler->running->thread_priority], scheduler->running);
		tcb_t *next_thread = mlfq_find_next_ready();
		
		scheduler->running = next_thread;
		scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;

		enable_timer();
		swapcontext(&(old_tcb->uctx), &(scheduler->running->uctx));
	}
}

void enable_timer() {
	setitimer(ITIMER_PROF, &itimer, NULL);
	enabled = true;
}

void disable_timer() {
	// setitimer(ITIMER_PROF, &pause_itimer, NULL);
	enabled = false;
}

void handle_timeout(int signum) {
	if (enabled) {
		tcb_t *running = scheduler->running;
		if (running->thread_priority < MLFQ_LEVELS-1) {
			running->thread_priority++;
		}
		schedule();
	}
}

rpthread_mutex_t mutex;
int i=0;

void funcA() {
	// int i=0;
    // while (1) {
	// 	printf("a: %d\n", i);
	// 	i++;
	// }
	int i=0;
	while (i < 1000000) {
		printf("a");
		i++;
	}
}

void funcB() {
	// int i=0;
    // while (1) {
	// 	printf("b: %d\n", i);
	// 	i++;
	// }
	int i=0;
	while (i < 1000000) {
		printf("b");
		i++;
	}
}

void funcC() {
	int i=0;
	while (i < 1000000) {
		printf("c");
		i++;
	}
}

// int main() {

// 	rpthread_t a, b, c;
// 	rpthread_mutex_init(&mutex, NULL);
// 	printf("a\n");
	
// 	rpthread_create(&a, NULL, funcA, NULL);
// 	rpthread_create(&b, NULL, funcB, NULL);
// 	rpthread_create(&c, NULL, funcC, NULL);

// 	rpthread_join(a, NULL);
// 	rpthread_join(b, NULL);
// 	rpthread_join(c, NULL);

// 	printf("\n");

// 	printf("done\n");

// 	return 0;
// }

