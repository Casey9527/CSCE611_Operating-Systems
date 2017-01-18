/*
     File: simple_disk.c

     Description : Block-level READ/WRITE operations on a simple LBA28 disk 
                   using Programmed I/O.
                   
                   The disk must be MASTER or SLAVE on the PRIMARY IDE controller.

                   The code is derived from the "LBA HDD Access via PIO" 
                   tutorial by Dragoniz3r. (google it for details.)
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "simple_disk.H"
#include "interrupts.H"

/*--------------------------------------------------------------------------*/
/* STATIC MEMBER */
/*--------------------------------------------------------------------------*/

BOOLEAN SimpleDisk::write_lock = FALSE;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

SimpleDisk::SimpleDisk(DISK_ID _disk_id, unsigned int _size) {
	disk_id   = _disk_id;
	disk_size = _size;
}

/*--------------------------------------------------------------------------*/
/* DISK CONFIGURATION */
/*--------------------------------------------------------------------------*/

unsigned int SimpleDisk::size() {
	return disk_size;
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void SimpleDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no) {

	outportb(0x1F1, 0x00); /* send NULL to port 0x1F1         */
	outportb(0x1F2, 0x01); /* send sector count to port 0X1F2 */
	outportb(0x1F3, (unsigned char)_block_no); 
                         /* send low 8 bits of block number */
	outportb(0x1F4, (unsigned char)(_block_no >> 8)); 
                         /* send next 8 bits of block number */
	outportb(0x1F5, (unsigned char)(_block_no >> 16)); 
                         /* send next 8 bits of block number */
	outportb(0x1F6, ((unsigned char)(_block_no >> 24)&0x0F) | 0xE0 | (disk_id << 4));
                         /* send drive indicator, some bits, 
                            highest 4 bits of block no */

	outportb(0x1F7, (_op == READ) ? 0x20 : 0x30);

}

BOOLEAN SimpleDisk::is_ready() {
	return (inportb(0x1F7) & 0x08);
}

void SimpleDisk::read(unsigned long _block_no, unsigned char * _buf) {
/* Reads 512 Bytes in the given block of the given disk drive and copies them 
   to the given buffer. No error check! */
	issue_operation(READ, _block_no);

	wait_until_ready();

	/* read data from port */
	int i;
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = inportw(0x1F0);
		_buf[i*2]   = (unsigned char)tmpw;
		_buf[i*2+1] = (unsigned char)(tmpw >> 8);
	}
}

void SimpleDisk::write(unsigned long _block_no, unsigned char * _buf) {
/* Writes 512 Bytes from the buffer to the given block on the given disk drive. */
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	issue_operation(WRITE, _block_no);
	
	wait_until_ready();

	/* write data to port */
	int i; 
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		outportw(0x1F0, tmpw);
	}
	write_lock = FALSE;   //free the lock
}


/*--------------------------------------------------------------------------*/
/* DERIVED CLASS -- BLOCKING DISK */
/*--------------------------------------------------------------------------*/

extern Scheduler* SYSTEM_SCHEDULER;
Thread* BlockingDisk::block_queue[MAX_THREADS];
int BlockingDisk::head = 0;
int BlockingDisk::tail = 0;

BlockingDisk::BlockingDisk(DISK_ID _disk_id, unsigned int _size) 
	: SimpleDisk(_disk_id, _size) {}

void BlockingDisk::read(unsigned long _block_no, unsigned char* _buf) {
	// register the thread that is to block as well as the corresponding disk number
	block_queue[tail] = Thread::CurrentThread();
	tail = (tail + 1) % MAX_THREADS;
	
	// tell the disk the thread is about to apply I/O operation
	issue_operation(READ, _block_no);
	
	// any time the thread gains CPU, it checks if the disk is ready
	while (!is_ready()) {
		// if not, gives up CPU and wait
		SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
		SYSTEM_SCHEDULER->yield();
	}
	
	// if yes, apply I/O operation;
	int i;
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = inportw(0x1F0);
		_buf[i*2]   = (unsigned char)tmpw;
		_buf[i*2+1] = (unsigned char)(tmpw >> 8);
	}
	// after the I/O operation is done
	// dequeue the thread in the block queue
	// dequeue the disk in the disk queue
	head = (head + 1) % MAX_THREADS;
}

void BlockingDisk::write(unsigned long _block_no, unsigned char* _buf) {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	// register the thread that is to block as well as the corresponding disk
	block_queue[tail] = Thread::CurrentThread();
	tail = (tail + 1) % MAX_THREADS;

	// tell the disk the thread is about to apply I/O operation
	issue_operation(WRITE, _block_no);
	
	// any time the thread gains CPU, it checks if the disk is ready
	while (!is_ready()) {
		SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
		SYSTEM_SCHEDULER->yield();
	}
	
	// if yes, apply I/O operation;
	int i; 
	unsigned short tmpw;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		outportw(0x1F0, tmpw);
	}
	// after the I/O operation is done
	// dequeue the thread in the block queue
	// dequeue the disk in the disk queue
	head = (head + 1) % MAX_THREADS;
	write_lock = FALSE;   //free the lock
}

void BlockingDisk::interrupt_handler(REGS* _regs) {
	// since we apply context switch in the interrupt handler,
	// if the scheduler is in critical section, then the interrupt will sabotage the scheduler.
	// Therefore, we check if the scheduler is in critical section,
	// if this is the case, we simply return and let the blocking thread wait to issue 
	// I/O operation when it gains CPU later.
	if (SYSTEM_SCHEDULER->lock) {
		return;
	}
	// whenever the disk is ready to have I/O operation, this interrupt is trigerred
	Thread* thread_to_active = block_queue[head];
	/* chances are that the disk is ready immediatedly after the thread applies,
	 * and the thread can apply I/O operation within the same CPU burst; therefore,
	 * the thread may be doing the I/O operation at this moment.
	 */
	
	if (thread_to_active == Thread::CurrentThread()) {
		// if this is the case, simply return and do no operations
		return;
	}
	else {
		SYSTEM_SCHEDULER->preempt(thread_to_active);
		return;
	}
}


/*--------------------------------------------------------------------------*/
/* DERIVED CLASS -- MIRRORED DISK */
/*--------------------------------------------------------------------------*/

extern Scheduler* SYSTEM_SCHEDULER;
Thread* MirroredDisk::block_queue[MAX_THREADS_FOR_MIRRORING];
DISK_ID MirroredDisk::disk_type_queue[MAX_THREADS_FOR_MIRRORING];
int MirroredDisk::head = 0;
int MirroredDisk::tail = 0;

MirroredDisk::MirroredDisk(DISK_ID _disk_id1, DISK_ID _disk_id2, unsigned int _size) 
	: SimpleDisk(_disk_id1, _size) {
	// initially, use the master disk
	master_disk = _disk_id1;
	slave_disk = _disk_id2;
}

void MirroredDisk::read(unsigned long _block_no, unsigned char* _buf) {
	/* for reading, send the request to both disks */
	// master disk
	disk_id = master_disk;
	// register the thread that is to block as well as the corresponding disk
	block_queue[tail] = Thread::CurrentThread();
	disk_type_queue[tail] = MASTER;
	is_completed[tail] = FALSE;
	tail = (tail + 1) % MAX_THREADS_FOR_MIRRORING;
	
	// tell the disk the thread is about to apply I/O operation
	issue_operation(READ, _block_no);
	
	// slave disk
	disk_id = slave_disk;
	// register the thread that is to block as well as the corresponding disk
	block_queue[tail] = Thread::CurrentThread();
	disk_type_queue[tail] = SLAVE;
	is_completed[tail] = FALSE;
	tail = (tail + 1) % MAX_THREADS_FOR_MIRRORING;
	
	// tell the disk the thread is about to apply I/O operation
	issue_operation(READ, _block_no);
	master_is_ready = FALSE;
	slave_is_ready = FALSE;
	// every time the thread obtains CPU, it check if either of the disk is ready
	while (true) {
		// check if master disk is ready
		disk_id = master_disk;
		if (is_ready()) {
			master_is_ready = TRUE;
			break;
		}
		// check if slave disk is ready
		disk_id = slave_disk;
		if (is_ready()) {
			slave_is_ready = TRUE;
			break;
		}
		// if both not, gives up CPU and wait
		SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
		SYSTEM_SCHEDULER->yield();
	}

	// apply I/O operation
	int i;
	unsigned short tmpw;
	// complete I/O operation according to the disk type
	if (master_is_ready) {
		// check if the request has already been completed
		BOOLEAN need_to_complete = FALSE;
		int iterator = head;
		for (unsigned long j = 0; j < MAX_THREADS_FOR_MIRRORING; ++j) {
			iterator = (iterator + j) % MAX_THREADS_FOR_MIRRORING;
			if ((block_queue[iterator] == Thread::CurrentThread())
				&& (disk_type_queue[iterator] == MASTER)) {   //find it!
					need_to_complete = TRUE;
					break;
			}
		}
		if (need_to_complete) {   //we need to complete I/O, otherwise the thread has not been found
			// mark the same request in the queue as completed
			iterator = head;
			for (unsigned long j = 0; j < MAX_THREADS_FOR_MIRRORING; ++j) {
				iterator = (iterator + j) % MAX_THREADS_FOR_MIRRORING;
				if ((block_queue[iterator] == Thread::CurrentThread())
					&& (disk_type_queue[iterator] == SLAVE)) {
					is_completed[iterator] = TRUE;
				}
			}
			// complete I/O request
			disk_id = master_disk;
			for (i = 0; i < 256; i++) {
				tmpw = inportw(0x1F0);
				_buf[i*2]   = (unsigned char)tmpw;
				_buf[i*2+1] = (unsigned char)(tmpw >> 8);
			}
			// after the I/O operation is done
			is_completed[head] = TRUE;   //mark as completed
			while (is_completed[head] && head != tail) {
				// move head until meets the next unfinished task or the tail
				head = (head + 1) % MAX_THREADS_FOR_MIRRORING;
			}
			return;
		}
		// if reach there, means that the thread is not found in the queue, simply return
		return;
	}
	if (slave_is_ready) {
		// check if the request has already been completed
		BOOLEAN need_to_complete = FALSE;
		int iterator = head;
		for (unsigned long j = 0; j < MAX_THREADS_FOR_MIRRORING; ++j) {
			iterator = (iterator + j) % MAX_THREADS_FOR_MIRRORING;
			if ((block_queue[iterator] == Thread::CurrentThread())
				&& (disk_type_queue[iterator] == SLAVE)) {   //find it!
					need_to_complete = TRUE;
					break;
			}
		}
		if (need_to_complete) {   //we need to complete I/O, otherwise the thread has not been found
			// mark the same request in the queue as completed
			iterator = head;
			for (unsigned long j = 0; j < MAX_THREADS_FOR_MIRRORING; ++j) {
				iterator = (iterator + j) % MAX_THREADS_FOR_MIRRORING;
				if ((block_queue[iterator] == Thread::CurrentThread())
					&& (disk_type_queue[iterator] == MASTER)) {
					is_completed[iterator] = TRUE;
				}
			}
			// complete I/O request
			disk_id = master_disk;
			for (i = 0; i < 256; i++) {
				tmpw = inportw(0x1F0);
				_buf[i*2]   = (unsigned char)tmpw;
				_buf[i*2+1] = (unsigned char)(tmpw >> 8);
			}
			// after the I/O operation is done
			is_completed[head] = TRUE;   //mark as completed
			while (is_completed[head] && head != tail) {
				// move head until meets the next unfinished task or the tail
				head = (head + 1) % MAX_THREADS_FOR_MIRRORING;
			}
			return;
		}
		// if reach there, means that the thread is not found in the queue, simply return
		return;
	}
}

void MirroredDisk::write(unsigned long _block_no, unsigned char* _buf) {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	int i; 
	unsigned short tmpw;
	
	// tell the master disk the thread is about to apply I/O operation
	// register the thread that is to block as well as the corresponding disk number
	block_queue[tail] = Thread::CurrentThread();
	disk_type_queue[tail] = MASTER;
	is_completed[tail] = FALSE;
	tail = (tail + 1) % MAX_THREADS_FOR_MIRRORING;
	disk_id = master_disk;
	issue_operation(WRITE, _block_no);
	
	// any time the thread gains CPU, it checks if the disk is ready
	while (!is_ready()) {
		SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
		SYSTEM_SCHEDULER->yield();
	}
	
	// if yes, apply I/O operation;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		outportw(0x1F0, tmpw);
	}
	// after the I/O operation is done
	// dequeue the thread in the block queue
	// dequeue the disk in the disk queue
	head = (head + 1) % MAX_THREADS_FOR_MIRRORING;
	
	// tell the slave disk the thread is about to apply I/O operation
	// register the thread that is to block as well as the corresponding disk number
	block_queue[tail] = Thread::CurrentThread();
	disk_type_queue[tail] = SLAVE;
	is_completed[tail] = FALSE;
	tail = (tail + 1) % MAX_THREADS_FOR_MIRRORING;
	disk_id = slave_disk;
	issue_operation(WRITE, _block_no);
	
	// any time the thread gains CPU, it checks if the disk is ready
	while (!is_ready()) {
		SYSTEM_SCHEDULER->resume(Thread::CurrentThread());
		SYSTEM_SCHEDULER->yield();
	}
	
	// if yes, apply I/O operation;
	for (i = 0; i < 256; i++) {
		tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
		outportw(0x1F0, tmpw);
	}
	// after the I/O operation is done
	// dequeue the thread in the block queue
	// dequeue the disk in the disk queue
	head = (head + 1) % MAX_THREADS_FOR_MIRRORING;
	write_lock = FALSE;   //free the lock
}

void MirroredDisk::interrupt_handler(REGS* _regs) {
	// since we apply context switch in the interrupt handler,
	// if the scheduler is in critical section, then the interrupt will sabotage the scheduler.
	// Therefore, we check if the scheduler is in critical section,
	// if this is the case, we simply return and let the blocking thread wait to issue 
	// I/O operation when it gains CPU later.
	if (SYSTEM_SCHEDULER->lock) {
		return;
	}
	// whenever the disk is ready to have I/O operation, this interrupt is trigerred
	Thread* thread_to_active = block_queue[head];
    
	/* chances are that the disk is ready immediatedly after the thread applies,
	 * and the thread can apply I/O operation within the same CPU burst; therefore,
	 * the thread may be doing the I/O operation at this moment.
	 */
	
	if (thread_to_active == Thread::CurrentThread()) {
		// if this is the case, simply return and do no operations
		return;
	}
	else {
		SYSTEM_SCHEDULER->preempt(thread_to_active);
		return;
	}
}
