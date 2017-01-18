/* 
    File: kernel.C

    This file has the main entry point to the operating system.

    MAIN FILE FOR MACHINE PROBLEM "KERNEL-LEVEL DEVICE MANAGEMENT
    AND SIMPLE FILE SYSTEM"

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define MB * (0x1 << 20)
#define KB * (0x1 << 10)
#define KERNEL_POOL_START_FRAME ((2 MB) / (4 KB))
#define KERNEL_POOL_SIZE ((2 MB) / (4 KB))

/* -- COMMENT/UNCOMMENT THE FOLLOWING LINE TO EXCLUDE/INCLUDE SCHEDULER CODE */

#define _USES_SCHEDULER_
/* This macro is defined when we want to force the code below to use 
   a scheduler.
   Otherwise, no scheduler is used, and the threads pass control to each 
   other in a co-routine fashion.
*/

#define _USES_DISK_
/* This macro is defined when we want to exercise the disk device.
   If defined, the system defines a disk and has Thread 2 read from it.
   Leave the macro undefined if you don't want to exercise the disk code.
*/

#define _USES_FILESYSTEM_
/* This macro is defined when we want to exercise file-system code.
   If defined, the system defines a file system, and Thread 3 issues 
   issues operations to it.
   Leave the macro undefined if you don't want to exercise file system code.
*/

//#define _EXERCISE_SMALL_FILE_
//#define _EXERCISE_LARGE_FILE_

/* This macro is for testing file system.
 * If defined, it will stop right after the corresponding test complete.
 * Note that only one of two macro can be uncommented at one time!
 */

#define _USES_MIRRORED_DISK_

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "machine.H"         /* LOW-LEVEL STUFF   */
#include "console.H"
#include "gdt.H"
#include "idt.H"             /* EXCEPTION MGMT.   */
#include "irq.H"
#include "exceptions.H"     
#include "interrupts.H"
#include "utils.H"

#include "simple_timer.H"    /* TIMER MANAGEMENT  */

#include "frame_pool.H"      /* MEMORY MANAGEMENT */
#include "mem_pool.H"

#include "thread.H"         /* THREAD MANAGEMENT */

#ifdef _USES_SCHEDULER_
#include "scheduler.H"
#endif

#ifdef _USES_DISK_
#include "simple_disk.H"
#endif

#ifdef _USES_FILESYSTEM_
#include "file_system.H"
#endif
/*--------------------------------------------------------------------------*/
/* MEMORY MANAGEMENT */
/*--------------------------------------------------------------------------*/

#ifdef MSDOS
typedef unsigned long size_t;
#else
typedef unsigned int size_t;
#endif

/* -- A POOL OF FRAMES FOR THE SYSTEM TO USE */
FramePool * SYSTEM_FRAME_POOL;

/* -- A POOL OF CONTIGUOUS MEMORY FOR THE SYSTEM TO USE */
MemPool * MEMORY_POOL;

/* -- Timer */
SimpleTimer* SIMPLE_TIMER;

/*--------------------------------------------------------------------------*/
/* SCHEDULER */
/*--------------------------------------------------------------------------*/

#ifdef _USES_SCHEDULER_

/* -- A POINTER TO THE SYSTEM SCHEDULER */
Scheduler * SYSTEM_SCHEDULER;

#endif

/*--------------------------------------------------------------------------*/
/* DISK */
/*--------------------------------------------------------------------------*/

#ifdef _USES_DISK_

/* -- A POINTER TO THE SYSTEM DISK */
SimpleDisk * SYSTEM_DISK;

#define SYSTEM_DISK_SIZE 10485760

#endif

/*--------------------------------------------------------------------------*/
/* FILE SYSTEM */
/*--------------------------------------------------------------------------*/

#ifdef _USES_FILESYSTEM_

/* -- A POINTER TO THE SYSTEM FILE SYSTEM */
FileSystem * FILE_SYSTEM;

#endif

/*--------------------------------------------------------------------------*/
/* JUST AN AUXILIARY FUNCTION */
/*--------------------------------------------------------------------------*/

void pass_on_CPU(Thread * _to_thread) {

#ifndef _USES_SCHEDULER_

	/* We don't use a scheduler. Explicitely pass control to the next
	 * thread in a co-routine fashion. */
	Thread::dispatch_to(_to_thread); 

#else

	/* We use a scheduler. Instead of dispatching to the next thread,
	 * we pre-empt the current thread by putting it onto the ready
	 * queue and yielding the CPU. */

	SYSTEM_SCHEDULER->resume(Thread::CurrentThread()); 
	SYSTEM_SCHEDULER->yield();

#endif

}


/*--------------------------------------------------------------------------*/
/* CODE TO EXERCISE THE FILE SYSTEM */
/*--------------------------------------------------------------------------*/

#ifdef _USES_FILESYSTEM_

int rand() {
	/* Rather silly random number generator. */
	
	unsigned long dummy_sec;
	int dummy_tic;

	SIMPLE_TIMER->current(&dummy_sec, &dummy_tic);

	return dummy_tic;
}

void exercise_file_system(FileSystem* _file_system, SimpleDisk* _disk) {
	
	_file_system->Format(_disk, SYSTEM_DISK_SIZE);
	_file_system->Mount(_disk);
	
	
	for(int j = 0;; j++) {
		Console::puts("---------------------------------------\n");
		Console::puts("|      Exercise the file system       |\n");
		Console::puts("---------------------------------------\n");
		File small_file = File(_file_system);
		File* _file1 = &small_file;
		File large_file = File(_file_system);
		File* _file2 = &large_file;
		// first test the small file
		// test write
		unsigned char str1[50] = "the quick brown fox jumps over the lazy dog.\n";
		const unsigned int str_len1 = strlen((char*)str1);
		Console::puts("Write onto disk:\n");
		Console::puts((char*)str1);
		Console::puts("\n");
		_file1->Write(str_len1 + 1, str1);
		// test append
		unsigned char str2[50] = "the lazy dog wakes up and gets really angry.\n";
		const unsigned int str_len2 = strlen((char*)str2);
		Console::puts("Write onto disk:\n");
		Console::puts((char*)str2);
		Console::puts("\n");
		_file1->Write(str_len2 + 1, str2);
		// test reset
		_file1->Reset();
		// test read
		unsigned char buf1[str_len1+str_len2+1];
		_file1->Read(str_len1 + str_len2 + 1, buf1);
		Console::puts("Read from the disk:\n");
		Console::puts((char*)buf1);
		Console::puts("\n");
		// test rewrite
		_file1->Rewrite();
		unsigned char str3[] = "what an interesting story!\n";
		const unsigned int str_len3 = strlen((char*)str3);
		Console::puts("Write onto disk:\n");
		Console::puts((char*)str3);
		Console::puts("\n");
		_file1->Write(str_len3 + 1, str3);
		_file1->Reset();
		unsigned char buf2[str_len3+1];
		_file1->Read(str_len3 + 1, buf2);
		Console::puts("Read from the disk:\n");
		Console::puts((char*)buf2);
		Console::puts("\n");

#ifdef _EXERCISE_SMALL_FILE_
		
		for(;;);

#endif
		// then test large file (more than one block)
		unsigned char str4[600];
			Console::puts("Generate random large file:\n");
		for (unsigned long i = 0; i < 590; ++i) {
			str4[i] = 'a' + i % 26;
			Console::putch(str4[i]);
		}
		const unsigned int str_len4 = strlen((char*)str4);
		Console::puts("\n");
		_file2->Write(str_len4 + 1, str4);
		_file2->Reset();
		unsigned char buf3[str_len4];
		_file2->Read(str_len4 + 1, buf3);
		Console::puts("\nRead the large file:\n");
		Console::puts((char*)buf3);
		Console::puts("\n");

#ifdef _EXERCISE_LARGE_FILE_
		
		for(;;);

#endif
		_file_system->DeleteFile(_file1->FileName());
		_file_system->DeleteFile(_file2->FileName());
		extern Thread* thread4;	
		pass_on_CPU(thread4);
	}
}

#endif

/*--------------------------------------------------------------------------*/
/* A FEW THREADS (pointer to TCB's and thread functions) */
/*--------------------------------------------------------------------------*/

Thread * thread1;
Thread * thread2;
Thread * thread3;
Thread * thread4;

void fun1() {
	Console::puts("THREAD: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");

	Console::puts("FUN 1 INVOKED!\n");
	
	for(int j = 0;; j++) {
		Console::puts("FUN 1 IN ITERATION["); Console::puti(j); Console::puts("]\n");
		for (int i = 0; i < 10; i++) {
			Console::puts("FUN 1: TICK ["); Console::puti(i); Console::puts("]\n");
		}
		pass_on_CPU(thread2);
	}
}



void fun2() {
	Console::puts("THREAD: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");

#ifdef _USES_DISK_

	unsigned char buf[512];
	int read_block  = 1;
	int write_block = 0;

#endif

	for(int j = 0;; j++) {
		Console::puts("FUN 2 IN ITERATION["); Console::puti(j); Console::puts("]\n");

#ifdef _USES_DISK_

		/* -- Read */
		Console::puts("-------------------------------\n");
		Console::puts("|  Reading a block from disk  |\n");
		Console::puts("-------------------------------\n");
		/* UNCOMMENT THE FOLLOWING LINE IN FINAL VERSION. */
		SYSTEM_DISK->read(read_block, buf);

		/* -- Display */
		int i;
		for (i = 0; i < 512; i++) {
			Console::putch(buf[i]);
		}

#ifndef _USES_FILESYSTEM_
		/* -- Write -- ONLY IF THERE IS NO FILE SYSTEM BEING EXERCISED! */
		/*             Running this piece of code on a disk with a      */
		/*             file system would corrupt the file system.       */

		Console::puts("------------------------------\n");
		Console::puts("|  Writing a block to disk   |\n");
		Console::puts("------------------------------\n");
		/* UNCOMMENT THE FOLLOWING LINE IN FINAL VERSION. */
		SYSTEM_DISK->write(write_block, buf); 

#endif

		/* -- Move to next block */
		write_block = read_block;
		read_block  = (read_block + 1) % 10;

#else

		for (int i = 0; i < 10; i++) {
			Console::puts("FUN 2: TICK ["); Console::puti(i); Console::puts("]\n");
		}
     
#endif
		
		/* -- Give up the CPU */
		pass_on_CPU(thread3);
	}
}

void fun3() {
	Console::puts("THREAD: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");

	Console::puts("FUN 3 INVOKED!\n");

	for(int j = 0;; j++) {

#ifdef _USES_FILESYSTEM_

		Console::puts("FUN 3:\n");
		
		exercise_file_system(FILE_SYSTEM, SYSTEM_DISK);

#else

		Console::puts("FUN 3 IN BURST["); Console::puti(j); Console::puts("]\n");
		for (int i = 0; i < 10; i++) {
			Console::puts("FUN 3: TICK ["); Console::puti(i); Console::puts("]\n");
		}
		pass_on_CPU(thread4);

#endif

	}
}

void fun4() {
	Console::puts("THREAD: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
	if (!machine_interrupts_enabled()) 
		for(;;);
	
	for(int j = 0;; j++) {
		Console::puts("FUN 4 IN BURST["); Console::puti(j); Console::puts("]\n");
		for (int i = 0; i < 10; i++) {
			Console::puts("FUN 4: TICK ["); Console::puti(i); Console::puts("]\n");
		}
		pass_on_CPU(thread1);
	}
}

/*--------------------------------------------------------------------------*/
/* MEMORY MANAGEMENT */
/*--------------------------------------------------------------------------*/

void * operator new (size_t size) {
	unsigned long a = MEMORY_POOL->allocate((unsigned long)size);
	return (void *)a;
}

//replace the operator "new[]"
void * operator new[] (size_t size) {
	unsigned long a = MEMORY_POOL->allocate((unsigned long)size);
	return (void *)a;
}

//replace the operator "delete"
void operator delete (void * p) {
	MEMORY_POOL->release((unsigned long)p);
}

//replace the operator "delete[]"
void operator delete[] (void * p) {
	MEMORY_POOL->release((unsigned long)p);
}

/*--------------------------------------------------------------------------*/
/* MAIN ENTRY INTO THE OS */
/*--------------------------------------------------------------------------*/

int main() {

	GDT::init();
	Console::init();
	IDT::init();
	ExceptionHandler::init_dispatcher();
	IRQ::init();
	InterruptHandler::init_dispatcher();

    /* -- EXAMPLE OF AN EXCEPTION HANDLER -- */

	class DBZ_Handler : public ExceptionHandler {
	public:
		virtual void handle_exception(REGS * _regs) {
			Console::puts("DIVISION BY ZERO!\n");
			for(;;);
		}
	} dbz_handler;

	ExceptionHandler::register_handler(0, &dbz_handler);

	/* -- INITIALIZE MEMORY -- */
	/*    NOTE: We don't have paging enabled in this MP. */
	/*    NOTE2: This is not an exercise in memory management. The implementation
	 *    of the memory management is accordingly *very* primitive! */

	/* ---- Initialize a frame pool; details are in its implementation */
	FramePool kernel_mem_pool(KERNEL_POOL_START_FRAME, KERNEL_POOL_SIZE, 0);
	SYSTEM_FRAME_POOL = &kernel_mem_pool;

	/* ---- Create a memory pool of 256 frames. */
	MemPool memory_pool(SYSTEM_FRAME_POOL, 256);
	MEMORY_POOL = &memory_pool;

	/* -- INITIALIZE THE TIMER (we use a very simple timer).-- */

	/* Question: Why do we want a timer? We have it to make sure that 
                 we enable interrupts correctly. If we forget to do it,
                 the timer "dies". */

	SimpleTimer timer(100); /* timer ticks every 10ms. */
	SIMPLE_TIMER = &timer;

#ifdef _USES_SCHEDULER_

    /* -- SCHEDULER -- IF YOU HAVE ONE -- */
  
	Scheduler system_scheduler = Scheduler();
	SYSTEM_SCHEDULER = &system_scheduler;

#endif
   

	/* -- DISK DEVICE -- IF YOU HAVE ONE -- */

#ifdef _USES_DISK_

#ifdef _USES_MIRRORED_DISK_

	MirroredDisk system_disk = MirroredDisk(MASTER,SLAVE,SYSTEM_DISK_SIZE);

#else

	BlockingDisk system_disk = BlockingDisk(MASTER, SYSTEM_DISK_SIZE);

#endif

	SYSTEM_DISK = &system_disk;

#endif

	InterruptHandler::register_handler(0, SIMPLE_TIMER);
	/* The Timer is implemented as an interrupt handler. */

#ifndef _USES_MIRRORED_DISK_

	//Register the the disk interrupt handler, which reponds to interrupt 14
	class Disk_Handler : public InterruptHandler {
	public:
		virtual void handle_interrupt(REGS * _regs) {
			//call the corresponding interrupt handler of Disk I/O
			BlockingDisk::interrupt_handler(_regs);
		}
	} disk_handler;

	InterruptHandler::register_handler(14, &disk_handler);

#else
	
	//Register the the disk interrupt handler, which reponds to interrupt 14
	class Disk_Handler : public InterruptHandler {
	public:
		virtual void handle_interrupt(REGS * _regs) {
			//call the corresponding interrupt handler of Disk I/O
			MirroredDisk::interrupt_handler(_regs);
		}
	} disk_handler;

	InterruptHandler::register_handler(14, &disk_handler);

#endif

#ifdef _USES_FILESYSTEM_

     /* -- FILE SYSTEM  -- IF YOU HAVE ONE -- */

	FileSystem file_system = FileSystem();
	FILE_SYSTEM = &file_system;

#endif


	/* NOTE: The timer chip starts periodically firing as 
             soon as we enable interrupts.
             It is important to install a timer handler, as we 
             would get a lot of uncaptured interrupts otherwise. */  

    /* -- ENABLE INTERRUPTS -- */

	machine_enable_interrupts();

    /* -- MOST OF WHAT WE NEED IS SETUP. THE KERNEL CAN START. */

	Console::puts("Hello World!\n");

    /* -- LET'S CREATE SOME THREADS... */

    Console::puts("CREATING THREAD 1...\n");
    char * stack1 = new char[1024];
    thread1 = new Thread(fun1, stack1, 1024);
    Console::puts("DONE\n");

    Console::puts("CREATING THREAD 2...");
    char * stack2 = new char[1024];
    thread2 = new Thread(fun2, stack2, 1024);
    Console::puts("DONE\n");

    Console::puts("CREATING THREAD 3...");
    char * stack3 = new char[1024];
    thread3 = new Thread(fun3, stack3, 1024);
    Console::puts("DONE\n");

    Console::puts("CREATING THREAD 4...");
    char * stack4 = new char[1024];
    thread4 = new Thread(fun4, stack4, 1024);
    Console::puts("DONE\n");

#ifdef _USES_SCHEDULER_

    /* WE ADD thread2 - thread4 TO THE READY QUEUE OF THE SCHEDULER. */

    SYSTEM_SCHEDULER->add(thread2);
    SYSTEM_SCHEDULER->add(thread3);
    SYSTEM_SCHEDULER->add(thread4);

#endif

    /* -- KICK-OFF THREAD1 ... */

    Console::puts("STARTING THREAD 1 ...\n");
    Thread::dispatch_to(thread1);

    /* -- AND ALL THE REST SHOULD FOLLOW ... */
 
	assert(FALSE); /* WE SHOULD NEVER REACH THIS POINT. */

    /* -- WE DO THE FOLLOWING TO KEEP THE COMPILER HAPPY. */
    return 1;
}
