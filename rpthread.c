// File:  rpthread.c
// List all group member's name: Sunny Chen, Michael Zhao

#include <sys/syscall.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpthread.h"


/********** Local Function Definitions **********/

void init_scheduler();
static void schedule();

void handle_timeout(int signum);
void handle_exit();
void enable_timer();
void disable_timer();


/********** Static Variable Definitions **********/

static Scheduler *scheduler;
static struct itimerval itimer;
static struct sigaction sa;


/********** Rpthread Public Functions **********/

/* 
 * init_scheduler() is a local function that will be called first time 
 * rpthread_create() is run. Sets up scheduler struct and creates tcb 
 * for the main function, which is the first function to call rpthread_create(). 
 * Initializes exit_context for threads that finish. 
 */
void init_scheduler() {
	scheduler = malloc(sizeof(*scheduler));

	// setup mlfq queues
	// if sched==RR, only first queue will be used
	for (int i=0; i < MLFQ_LEVELS; i++) {
		scheduler->thread_queues[i] = new_queue();
	}

	// setup tcb_arr to hold tcb refs
	scheduler->t_count = 1;  // 1 for main thread
	scheduler->t_max = 32;
	scheduler->tcb_arr = malloc(scheduler->t_max * sizeof(*(scheduler->tcb_arr)));
	
	// create main thread
	tcb_t *main_tcb = new_tcb(0, NULL, NULL);
	getcontext(main_tcb->uctx);
	scheduler->tcb_arr[0] = main_tcb;
	scheduler->running = main_tcb;
	
	// setup exit context
	// every non-main thread will switch to this context after finished
	scheduler->exit_uctx = malloc(sizeof(*(scheduler->exit_uctx)));
	getcontext(scheduler->exit_uctx);
	scheduler->exit_uctx->uc_stack.ss_sp = malloc(SS_SIZE);
	scheduler->exit_uctx->uc_stack.ss_size = SS_SIZE;
	scheduler->exit_uctx->uc_link = NULL;
	makecontext(scheduler->exit_uctx, handle_exit, 0);

	// initialize timer signals
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &handle_timeout;
	sigaction(SIGPROF, &sa, NULL);
}


/* 
 * Creates and adds thread to scheduler queue. No support for
 * pthread_attr_t, just there to match signiture of pthread_create.
 * This function may take a while to run, so we disable_timer()
 * to make it thread safe.
 */
int rpthread_create(rpthread_t *thread, pthread_attr_t *attr,
					void *(*function)(void *), void *arg) {
	if (scheduler == NULL) {  // first time running
		init_scheduler();
	}
	else {
		disable_timer();
	}

	*thread = scheduler->t_count;
	scheduler->t_count++;

	tcb_t *tcb = new_tcb(*thread, function, arg);
	setup_tcb_context(tcb->uctx, scheduler->exit_uctx, tcb);

	// resize ts_arr if too many threads
	if (scheduler->t_count > scheduler->t_max) {
		scheduler->t_max += 32;
		scheduler->tcb_arr = realloc(scheduler->tcb_arr, scheduler->t_max * sizeof(*(scheduler->tcb_arr)));
	}
	scheduler->tcb_arr[*thread] = tcb;

	enqueue(scheduler->thread_queues[0], tcb);  // new thread starts at top queue 
	enable_timer(TIMESLICE);
    return 0;
};


/* 
 * No need to do anything. The scheduler assumes that whenever it is called,
 * the previously running thread wants to stop running, so it will automatically
 * put it back into queue.
 */
int rpthread_yield() {
	schedule();
	return 0;
};


/* 
 * Same as rpthread_yield, the scheduler will handle all the work with freeing
 * the finished thread. If value_ptr is not NULL, the retval made avaliable to
 * rpthread_join() is set to value_ptr.
 */
void rpthread_exit(void *value_ptr) {
	if (value_ptr)
		scheduler->running->retval = value_ptr;
	scheduler->running->state = FINISHED;  // tell scheduler to terminate running thread
	schedule();
};


/* 
 * Pause calling thread execution until the thread with id `thread` finishes. Store 
 * calling thread in a `joined` queue under `thread` and have scheduler remove it 
 * from the scheduler's queue. Once `thread` finishes, the calling thread will be 
 * put back in scheduler queue. We can retrieve the retval and store it under *value_ptr 
 * if applicable.
 */
int rpthread_join(rpthread_t thread, void **value_ptr) {
	tcb_t *awaiting = scheduler->tcb_arr[thread];
	if (awaiting->state != FINISHED) {
		scheduler->running->state = BLOCKED;  // tell scheduler to remove it from ready queue
		enqueue(awaiting->joined, scheduler->running);  // add to joined queue
		schedule();
	}

	if (value_ptr != NULL) {
		*value_ptr = awaiting->retval;  // store retval
	}
	return 0;
};


/* Initialize the mutex lock and blocked queue */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	mutex->lock = 0;  // 0 = unlocked, 1 = locked
	mutex->tid = -1;  // 
	mutex->blocked_queue = new_queue();
	return 0;
};


/*
 * Use atomic test_and_set to lock mutex and block calling thread until
 * mutex is unlocked. If mutex is locked, remove calling thread from
 * scheduler queue and store it under mutex blocked queue.
 */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {

	/* While testing, our timer would go off in the middle of this function
	and cause a segfault. We weren't sure why that happens but disabled
	timer anyways to prevent it. */
	disable_timer();

	while (__sync_lock_test_and_set(&(mutex->lock), 1) == 1) {
		scheduler->running->state = BLOCKED;  // tell scheduler to remove from queue
		enqueue(mutex->blocked_queue, scheduler->running);  // store in mutex
		schedule();
	}

	mutex->tid = scheduler->running->tid;  // keep track of thread that locked mutex
	return 0;
};


/* 
 * Release mutex lock, put 1 thread from blocked queue back into scheduler
 * queue.
 */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	if (mutex->tid == scheduler->running->tid) {  // only thread that locked can unlock
		__sync_lock_test_and_set(&(mutex->lock), 0);

		/* If we put all threads back it becomes expensive, so we
		only let the first thread that called rpthread_mutex_lock() 
		through. Eventually all threads will be removed from this 
		queue. */
		if (mutex->blocked_queue->size > 0) {  // 
			tcb_t *tcb = dequeue(mutex->blocked_queue);
			enqueue(scheduler->thread_queues[tcb->priority], tcb);
		}
	}
	
	return 0;
};


/* Destroy mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	free(mutex->blocked_queue);
	return 0;
};


/********** Rpthread Private Functions **********/

/* Set timer to time (ms) */
void enable_timer(int time) {
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = time * 1000;
	itimer.it_value = itimer.it_interval;

	setitimer(ITIMER_PROF, &itimer, NULL);
	scheduler->enabled = true;
}

/* Disable timer for thread safety. We got weird results when we used 
 * setitimer() on a zeroed itimerval, so we just set a bool to ignore
 * timeouts.
 */
void disable_timer() {
	scheduler->enabled = false;
}


/* Signal handler for timer timeouts */
void handle_timeout(int signum) {
	if (scheduler->enabled) {  // ignore timeout if enabled=false
		tcb_t *running = scheduler->running;
		if (running->priority < MLFQ_LEVELS-1) {  // thread used all its timeslice 
			running->priority++;
		}
		schedule();
	}
}

/* Runs after thread completes */
void handle_exit() {
	scheduler->running->state = FINISHED;
	schedule();
}


/* 
 * Simple RR scheduler. Assumes that all threads in the queue are ready,
 * (blocked threads from rpthread_join() and rpthread_mutex_lock() are already
 * removed from queue by scheduler). After this function returns, 
 * scheduler->running will be the next thread to run.
 */
static void sched_rr() {
	queue_t *queue = scheduler->thread_queues[0];  // only use first queue

	if (queue->size == 0)  // no other threads avaliable (except running)
		return;  		   // let running continue

	enqueue(queue, scheduler->running);
	scheduler->running = dequeue(queue);  // schedule from front of queue
}

/* 
 * MLFQ scheduler with 8 levels. Searches all levels starting from highest
 * priority for a ready thread. If scheduler->running is the highest priority,
 * it will continue execution. After this function returns, scheduler->running 
 * will be the next thread to run.
 */
static void sched_mlfq() {
	tcb_t *running = scheduler->running;

	/* If scheduler->running is the highest priority out of all ready
	 * threads, there's no need to enqueue() and dequeue() it, we can
	 * just let it keep running. We need to search for highest priority
	 * in the queue and compare it to scheduler->running. */

	int level = 0;  // set level to first level with ready thread
	for (; level < MLFQ_LEVELS; level++) {
		if (scheduler->thread_queues[level]->size > 0) {
			break;
		}
	}

	// If running == NULL bc of blocking, we cant access running->priority
	if (running != NULL) {
		if (level > running->priority)  // scheduler->running is highest priority
			return;
		else {
			enqueue(scheduler->thread_queues[running->priority], running);  // put back in queue
		}
	}
	scheduler->running = dequeue(scheduler->thread_queues[level]);
}


/* 
 * Main scheduler function. Called everytime we want to switch threads or handle
 * a finished thread. On function call, scheduler->running is the previously running
 * thread. A call to schedule() will return when thread exectuion is handed back
 * to the calling thread. Since this function is critical and may take a while to
 * run, timer is disabled to make it thread safe.
 */
static void schedule() {
	disable_timer();  // disable itimer

	clock_t curr_time = clock();  // get time to calculate thread runtime

	tcb_t *old_tcb = scheduler->running;
	bool no_save = (old_tcb->state == FINISHED);  // use set_context() instead of swap_context()

	// called from rpthread_exit()
	if (old_tcb->state == FINISHED) {
		// add threads back from joined queue
		tcb_t *curr = old_tcb->joined->head;
		while (curr != NULL) {
			curr->state = READY;
			enqueue(scheduler->thread_queues[curr->priority], curr);
			curr = curr->next;
		}

		free(old_tcb->joined);
		free(old_tcb->uctx->uc_stack.ss_sp);
		free(old_tcb->uctx);

		scheduler->running = NULL;
	}
	else if (old_tcb->state == BLOCKED) {
		scheduler->running = NULL;  // blocked thread doesn't belong in queue
	}


	/* This section prevents gaming MLFQ. If a thread uses the whole TIMESLICE, we
	reduce its priority. This prevents a thread from calling rpthread_yield() to
	stay at highest priority level. */
	if (old_tcb->priority < MLFQ_LEVELS-1) {
		/* Calculate thread runtime from clock() */
		double ms_used = (old_tcb->last_run == 0) ? 
			0 : ((double)(curr_time - old_tcb->last_run)) / CLOCKS_PER_SEC * 1000;

		old_tcb->timeslice -= ms_used;
		if (old_tcb->timeslice <= 0) {  // exhausted timeslice, increase priority
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

	scheduler->running->last_run = clock();  // record start time
	enable_timer(scheduler->running->timeslice);

	if (scheduler->running == old_tcb) {  // no context change
		return;
	}
	else if (no_save) {  // previous thread finished, dont need to save context
		setcontext(scheduler->running->uctx);
	}
	else {
		swapcontext(old_tcb->uctx, scheduler->running->uctx);
	}
}


