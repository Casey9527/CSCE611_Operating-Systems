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

int Scheduler::time_interval = 2;
//the timer ticks every 10ms, and the time interval is set to be 20ms

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
	Thread::dispatch_to(next_thread);
	if (Thread::clean) {
		MEMORY_POOL->release((unsigned long)Thread::clean_stack);
		MEMORY_POOL->release((unsigned long)Thread::clean_thread_ptr);
		Thread::clean = false;
		Thread::clean_stack = NULL;
	}
	if (!Machine::interrupts_enabled()) {
		Machine::enable_interrupts();
	}
	TIMER->reset();
}

void Scheduler::resume(Thread* _thread) {
	if (Machine::interrupts_enabled()) {
		Machine::disable_interrupts();
	}

	ready_queue[tail] = _thread;
	tail = (tail + 1) % MAX_THREADS;
}

void Scheduler::add(Thread* _thread) {
	if (Machine::interrupts_enabled()) {
		Machine::disable_interrupts();
	}

	ready_queue[tail] = _thread;
	tail = (tail + 1) % MAX_THREADS;
}

void Scheduler::terminate(Thread* _thread) {
	if (_thread == Thread::CurrentThread()) {
		yield();
	}
}

