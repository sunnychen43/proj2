#include "rpthread.h"

static void schedule();

/********** Queue Implementation **********/

void handle_timeout(int signum);
void handle_exit();
void enable_timer();
void disable_timer();

static Scheduler *scheduler;
static struct itimerval itimer;
static struct sigaction sa;
static bool enabled;

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
	for (int i=0; i < MLFQ_LEVELS; i++) {
		scheduler->thread_queues[i] = new_queue();
	}

	// create main thread
	tcb_t *main_tcb = new_tcb(0, 0);
	scheduler->running = main_tcb;
	getcontext(&(main_tcb->uctx));

	// setup thread info
	scheduler->ts_arr = (char *) malloc(32 * sizeof(char));
	scheduler->ts_arr[0] = SCHEDULED;  // main thread scheduled
	scheduler->ts_count = 1;
	scheduler->ts_size = 32;

	// setup exit context
	setup_context(&(scheduler->exit_uctx), rpthread_exit, NULL, NULL);

	//setup itimer
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = TIMESLICE*1000;
	itimer.it_value = itimer.it_interval;

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &handle_timeout;
	sigaction(SIGPROF, &sa, NULL);
	setitimer(ITIMER_PROF, &itimer, NULL);
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
	while (scheduler->ts_arr[thread] != FINISHED) {
		rpthread_yield();
	}
	return 0;
};



/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex
	mutex->lock = 0;
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
	while (__sync_lock_test_and_set(&(mutex->lock), 1) == 1) {
		rpthread_yield();
	}
	return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	__sync_lock_test_and_set(&(mutex->lock), 0);
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

static void schedule() {
	disable_timer();
	tcb_t *old_tcb = scheduler->running;

	// for (int i=0; i < MLFQ_LEVELS; i++) {
	// 	printf("%d ", scheduler->thread_queues[i]->size);
	// }
	// printf("\n");
	// called from handle_exit
	if (scheduler->ts_arr[old_tcb->tid] == FINISHED) {
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
	else {
		tcb_t *next_thread = mlfq_find_next_ready();
		if (next_thread == NULL) {
			scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;
			enable_timer();
			return;
		}
		
		scheduler->ts_arr[scheduler->running->tid] = READY;
		enqueue(scheduler->thread_queues[scheduler->running->thread_priority], scheduler->running);

		scheduler->running = next_thread;
		scheduler->ts_arr[scheduler->running->tid] = SCHEDULED;
		enable_timer();
		swapcontext(&(old_tcb->uctx), &(scheduler->running->uctx));
	}
}

void enable_timer() {
	// setitimer(ITIMER_PROF, &itimer, NULL);
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

	while (i < 100000) {
		rpthread_mutex_lock(&mutex);
		i++;
		printf("a: %d\n", i);
		rpthread_mutex_unlock(&mutex);
	}
}

void funcB() {
	// int i=0;
    // while (1) {
	// 	printf("b: %d\n", i);
	// 	i++;
	// }

	while (i < 100000) {
		rpthread_mutex_lock(&mutex);
		i++;
		printf("b: %d\n", i);
		rpthread_mutex_unlock(&mutex);
	}
}

// int main() {

// 	rpthread_t a, b;
// 	rpthread_mutex_init(&mutex, NULL);
// 	printf("a\n");
	
// 	rpthread_create(&a, NULL, funcA, NULL);
// 	rpthread_create(&b, NULL, funcB, NULL);

// 	rpthread_join(a, NULL);
// 	rpthread_join(b, NULL);

// 	printf("done\n");

// 	return 0;
// }

