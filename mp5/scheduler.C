/* 		
	A thread scheduler.

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "simple_timer.H"

/*--------------------------------------------------------------------------*/
/* SCHEDULER */
/*--------------------------------------------------------------------------*/

BOOLEAN Scheduler::lock = FALSE;

/*--------------------------------------------------------------------------*/
/* EXTERNS */
/*--------------------------------------------------------------------------*/

extern SimpleTimer* TIMER; 
extern MemPool* MEMORY_POOL;

Scheduler::Scheduler() {
	head = 0;   //points to the next thread ready to run
	tail = 0;   //points to the position for the next thread to be added in the queue
}

void Scheduler::yield() {
	Thread* next_thread = ready_queue[head];
	ready_queue[head] = NULL;
	head = (head + 1) % MAX_THREADS;
	lock = TRUE;   //set the lock
	Thread::dispatch_to(next_thread);
	lock = FALSE;   //free the lock
}

void Scheduler::resume(Thread* _thread) {
	lock = TRUE;   //set the lock
	ready_queue[tail] = _thread;
	tail = (tail + 1) % MAX_THREADS;
	lock = FALSE;   //free the lock
}

void Scheduler::add(Thread* _thread) {
	lock = TRUE;   //set the lock
	ready_queue[tail] = _thread;
	tail = (tail + 1) % MAX_THREADS;
	lock = FALSE;   //free the lock
}

void Scheduler::terminate(Thread* _thread) {
	if (_thread == Thread::CurrentThread()) {
		yield();
	}
}

void Scheduler::preempt(Thread* _thread) {
	lock = TRUE;   //set the lock
	head = (head + MAX_THREADS - 1) % MAX_THREADS;
	ready_queue[head] = Thread::CurrentThread();
	Thread::dispatch_to(_thread);
	lock = FALSE;   //free the lock
}

