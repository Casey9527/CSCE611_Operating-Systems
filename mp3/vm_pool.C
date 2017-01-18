/*
    File: vm_pool.C

    Description: Management of the Virtual Memory Pool


*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* V M  P o o l  */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long _base_address,
				unsigned long _size,
				FramePool* _frame_pool,
				PageTable* _page_table) {
	page_table = _page_table;   //connect this VM pool with the current page table
	base_address = _base_address;
	size = _size;
	frame_pool = _frame_pool;
	region_no = 0;
	page_table->register_vmpool(this);   //get this VM pool registered in this page table
	
	//leave the first 5 virtual frames to store the free region information
	alloc_region = (VM_Region*) base_address;
	alloc_region[0].vm_base = base_address;
	alloc_region[0].vm_size = 5 * FRAME_SIZE;
	region_no++;   //this is the first region allocated
}

unsigned long VMPool::allocate(unsigned long _size) {
	unsigned long size_needed = (_size / FRAME_SIZE + 1) * FRAME_SIZE;
	unsigned long i, j;
	unsigned long alloc_address;
	page_table->register_vmpool(this);
	//all the regions are sorted based on base address
	//find the first hole that can satisfy the needed size
	//insert this region into the region list based on its base address
	//first check the holes between allocated regions
	for (i = 1; i < region_no - 1; ++i) {
		if (alloc_region[i + 1].vm_base - (alloc_region[i].vm_base + alloc_region[i].vm_size)
			>= size_needed) {
			alloc_address = alloc_region[i].vm_base + alloc_region[i].vm_size;
			//insert the new allocated region
			region_no++;
			for (j = region_no - 1; j > i + 1; --j) {
				alloc_region[j] = alloc_region[j - 1];
			}
			alloc_region[i + 1].vm_base = alloc_address;
			alloc_region[i + 1].vm_size = size_needed;
			return alloc_address;
		}
	}
	//then check the space after the last region
	if (size - (alloc_region[region_no - 1].vm_base + alloc_region[region_no - 1].vm_size) >= size_needed) {
		alloc_address = alloc_region[region_no - 1].vm_base + alloc_region[region_no - 1].vm_size;
		alloc_region[region_no].vm_base = alloc_address;
		alloc_region[region_no].vm_size = size_needed;
		region_no++;
		return alloc_address;
	}
	return 0;
}

void VMPool::release(unsigned long _start_address) {
	unsigned long i, j;
	unsigned long page_allocated;
	for (i = 1; i < region_no; ++i) {
		if (_start_address == alloc_region[i].vm_base) {
			//find the corrsponding region
			page_table->register_vmpool(this);   //get this VM pool registered in this page table
			region_no--;
			page_allocated = alloc_region[i].vm_size / FRAME_SIZE;
			//delete this region
			for (j = i; j < region_no; ++j) {
				//delete the relased region
				alloc_region[j].vm_base = alloc_region[j + 1].vm_base;
				alloc_region[j].vm_size = alloc_region[j + 1].vm_size;
			}
			//delete the last region (already moved forward)
			alloc_region[region_no].vm_base = 0;
			alloc_region[region_no].vm_size = 0;
			//free the corresponding page
			for (j = 0; j < page_allocated; ++j) {
				//free all the pages corresponding to this region
				page_table->free_page(_start_address / FRAME_SIZE + j);
				page_table->load();   //flush TLB
			}
			return;
		}
	}
	Console::puts("The region to release has not been allocated.\n");
}

BOOLEAN VMPool::is_legitimate(unsigned long _address) {
	unsigned long i;
	//check if the address is in the information region
	if (_address >= base_address && _address - base_address < 5 * FRAME_SIZE) {
		return TRUE;
	}
	for (i = 1; i < region_no - 1; ++i) {
		if (alloc_region[i].vm_base <= _address && alloc_region[i + 1].vm_base > _address) {
			//find the region where this address is possibly located
			//check if this address is in this region or not
			if (_address - alloc_region[i].vm_base <= alloc_region[i].vm_size) {
				return TRUE;
			} else {
				return FALSE;
			}
		}
	}
	//check the last region
	if (alloc_region[i].vm_base <= _address && 
			_address - alloc_region[i].vm_base <= alloc_region[i].vm_size) {
		return TRUE;
	} else {
		return FALSE;
	}
}
