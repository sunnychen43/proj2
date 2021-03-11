#include "rpthread.h"
#include <time.h>

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
	tcb->retval = tcb->func_ptr(tcb->args);
}

void setup_tcb_context(ucontext_t *uc, ucontext_t *uc_link, tcb_t *tcb) {
	getcontext(uc);
	uc->uc_stack.ss_sp = malloc(SS_SIZE);
	uc->uc_stack.ss_size = SS_SIZE;
	uc->uc_link = uc_link;
	makecontext(uc, thread_wrapper, 1, tcb);
}

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
	scheduler->t_count = 1;
	scheduler->t_max = 32;
	scheduler->tcb_arr = malloc(scheduler->t_max * sizeof(*(scheduler->tcb_arr)));
	scheduler->tcb_arr[0] = main_tcb;

	// setup exit context
	getcontext(&(scheduler->exit_uctx));
	scheduler->exit_uctx.uc_stack.ss_sp = malloc(SS_SIZE);
	scheduler->exit_uctx.uc_stack.ss_size = SS_SIZE;
	scheduler->exit_uctx.uc_link = NULL;
	makecontext(&(scheduler->exit_uctx), rpthread_exit, 0);

	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &handle_timeout;
	sigaction(SIGPROF, &sa, NULL);
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
		scheduler->t_max += 32;
		scheduler->tcb_arr = realloc(scheduler->tcb_arr, scheduler->t_max * sizeof(*(scheduler->tcb_arr)));
	}
	scheduler->tcb_arr[*thread] = tcb;

	enqueue(scheduler->thread_queues[0], tcb);
	enable_timer(TIMESLICE);
    return 0;
};

int rpthread_yield() {
	scheduler->tcb_arr[scheduler->running->tid]->state = YIELD;
	schedule();
	return 0;
};

void rpthread_exit(void *value_ptr) {
	scheduler->tcb_arr[scheduler->running->tid]->state = FINISHED;
	schedule();
};

int rpthread_join(rpthread_t thread, void **value_ptr) {

	if (scheduler->tcb_arr[thread]->state != FINISHED) {
		scheduler->tcb_arr[scheduler->running->tid]->state = BLOCKED;
		enqueue(scheduler->tcb_arr[thread]->joined, scheduler->running);
		schedule();
	}

	if (value_ptr != NULL) {
		*value_ptr = scheduler->tcb_arr[thread]->retval;
	}

	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex
	mutex->lock = 0;
	mutex->tid = -1;
	mutex->blocked_queue = new_queue;
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
	disable_timer();
	while (__sync_lock_test_and_set(&(mutex->lock), 1) == 1) {
		enqueue(mutex->blocked_queue, scheduler->running);
		scheduler->tcb_arr[scheduler->running->tid]->state = BLOCKED;
		schedule();
	}
	mutex->tid = scheduler->running->tid;
	return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	if (mutex->tid == scheduler->running->tid) {
		__sync_lock_test_and_set(&(mutex->lock), 0);
		if (mutex->blocked_queue->size > 0) {
			tcb_t *tcb = dequeue(mutex->blocked_queue);
			enqueue(scheduler->thread_queues[tcb->priority], tcb);
		}
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

void print_queue(tcb_t *head) {
	while (head != NULL) {
		printf("%d->", head->tid);
		head = head->next;
	}
	printf("\n");
}

static void sched_rr() {
	ThreadQueue *queue = scheduler->thread_queues[0];
	if (queue->size == 0)
		return;

	enqueue(queue, scheduler->running);
	scheduler->running = dequeue(queue);
}


static void sched_mlfq() {
	tcb_t *running = scheduler->running;
	int level = 0;
	for (; level < MLFQ_LEVELS; level++) {
		if (scheduler->thread_queues[level]->size > 0) {
			break;
		}
	}

	if (running != NULL && level > running->priority)
		return;

	if (running != NULL)
		enqueue(scheduler->thread_queues[running->priority], running);

	scheduler->running = dequeue(scheduler->thread_queues[level]);
}

static void schedule() {
	disable_timer();
	clock_t curr_time = clock();

	tcb_t *old_tcb = scheduler->running;
	bool no_save = (old_tcb->state == FINISHED);

	// called from handle_exit
	if (old_tcb->state == FINISHED) {
		tcb_t *curr = old_tcb->joined->head;
		while (curr != NULL) {
			curr->state = READY;
			enqueue(scheduler->thread_queues[curr->priority], curr);
			curr = curr->next;
		}
		free(old_tcb->uctx.uc_stack.ss_sp);
		scheduler->running = NULL;
	}
	else if (old_tcb->state == BLOCKED) {
		scheduler->running = NULL;
	}

	if (old_tcb->priority < MLFQ_LEVELS-1) {
		double ms_used = (old_tcb->last_run == 0) ? 
			0 : ((double)(curr_time - old_tcb->last_run)) / CLOCKS_PER_SEC * 1000;

		old_tcb->timeslice -= ms_used;
		if (old_tcb->timeslice <= 0) {
			if (old_tcb->priority < MLFQ_LEVELS-1) {
				old_tcb->priority++;
			}
			old_tcb->timeslice = TIMESLICE;
		}
	}

	#ifdef MLFQ
		sched_mlfq();
	#else
		sched_rr();
	#endif

	scheduler->running->last_run = clock();
	enable_timer(scheduler->running->timeslice);
	if (scheduler->running == old_tcb){
		return;
	}
	else if (no_save) {
		setcontext(&(scheduler->running->uctx));
	}
	else {
		swapcontext(&(old_tcb->uctx), &(scheduler->running->uctx));
	}
}

void enable_timer(int time) {
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = time * 1000;
	itimer.it_value = itimer.it_interval;

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
		if (running->priority < MLFQ_LEVELS-1) {
			running->priority++;
		}
		schedule();
	}
}
