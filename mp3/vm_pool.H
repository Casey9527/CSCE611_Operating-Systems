/*
    File: vm_pool.H

    Description: Management of the Virtual Memory Pool


*/

#ifndef _VM_POOL_H_                   // include file only once
#define _VM_POOL_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "utils.H"
#include "machine.H"
#include "frame_pool.H"
#include "page_table.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* Forward declaration of class PageTable */
class PageTable;

/*--------------------------------------------------------------------------*/
/* V M  P o o l  */
/*--------------------------------------------------------------------------*/

class VMPool { /* Virtual Memory Pool */
private:
	unsigned long base_address;
	unsigned long size;
	FramePool* frame_pool;
	PageTable* page_table;
	unsigned long region_no;

	typedef struct VM_Region {
		unsigned long vm_base;
		unsigned long vm_size;
	} VM_Region;

	VM_Region* alloc_region;
public:
	static const unsigned long FRAME_SIZE = Machine::PAGE_SIZE;
	
	VMPool(unsigned long _base_address,
			unsigned long _size,
			FramePool *_frame_pool,
			PageTable *_page_table);
	/* Initializes the data structures needed for the management of this
	 * virtual-memory pool.
	 * _base_address is the logical start address of the pool.
	 * _size is the size of the pool in bytes.
	 * _frame_pool points to the frame pool that provides the virtual
	 * memory pool with physical memory frames.
	 * _page_table points to the page table that maps the logical memory
	 * references to physical addresses. */

	unsigned long allocate(unsigned long _size);
	/* Allocates a region of _size bytes of memory from the virtual
	 * memory pool. If successful, returns the virtual address of the
	 * start of the allocated region of memory. If fails, returns 0. */

	void release(unsigned long _start_address);
	/* Releases a region of previously allocated memory. The region
	 * is identified by its start address, which was returned when the
	 * region was allocated. */

	BOOLEAN is_legitimate(unsigned long _address);
	/* Returns FALSE if the address is not valid. An address is not valid
	 * if it is not part of a region that is currently allocated. */
};

#endif
