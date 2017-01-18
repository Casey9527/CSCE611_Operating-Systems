/*
    File: page_pool.C

    Description: Basic Paging.


*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "page_table.H"
#include "paging_low.H"
#include "console.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

PageTable* PageTable::current_page_table; 
unsigned int PageTable::paging_enabled;   
FramePool* PageTable::kernel_mem_pool;    
FramePool* PageTable::process_mem_pool;  
unsigned long PageTable::shared_size;   

/*--------------------------------------------------------------------------*/
/* P A G E - T A B L E  */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

void PageTable::init_paging(FramePool* _kernel_mem_pool,
							FramePool* _process_mem_pool,
							const unsigned long _shared_size) {
	paging_enabled = 0;   //turn off paging
	kernel_mem_pool = _kernel_mem_pool;   //record kernel frame pool
	process_mem_pool = _process_mem_pool;   //record process frame pool
	shared_size = _shared_size;   //record shared size
}

PageTable::PageTable() {
	//create first PD
	page_directory = (unsigned long*) (kernel_mem_pool->get_frame() * PAGE_SIZE);
	//get one free frame to store directory
	//create first PT
	unsigned long* page_table = (unsigned long*) (process_mem_pool->get_frame() * PAGE_SIZE);
	//get one free frame to store page
	//fill the first PT
	unsigned long i = 0;
	unsigned long address = 0;
	for (i = 0; i < shared_size / PAGE_SIZE; ++i) {
		//4MB/4KB = 1024 entries needed for whole kernel memory   
		//one page has: 4KB/4B = 1KB entries
		//entries for kernel memory exactly fill in one page
		//shared size cannot be larger than 4MB, so one page is enough
		//set the first PT of kernel memory
		page_table[i] = address | 0x3;   //supervisor, R/W, present
		address += PAGE_SIZE;   //next 4KB
	}
	//fill the first PD entry
	page_directory[0] = (unsigned long) page_table | 0x3;   //supervisor, R/W, present
	//fill the last PD entry
	page_directory[ENTRIES_PER_PAGE - 1] = (unsigned long) page_directory | 0x3;
	//the last entry of PD is the address of this PD; superviosr, R/W, present
	//set other entries of PD
	for (i = 1; i < ENTRIES_PER_PAGE - 1; ++i) {
		page_directory[i] = 0 | 0x2;   //supervisor, R/W, not present
	}
}

void PageTable::load() {
	write_cr3((unsigned long) page_directory);   
	//write the current PD to CR3
	current_page_table = this;   //record the current object
}

void PageTable::enable_paging() {
	write_cr0((unsigned long) (read_cr0() | 0x80000000));   
	//set the paging bit in CR0 to 1
	paging_enabled = 1;
}

void PageTable::handle_fault(REGS* _r) {
	unsigned long errcode = _r->err_code;
	if ((errcode & 0x1) == 0) {   //page fault caused by non-present page
		unsigned long page_fault_addr = read_cr2();   //get the address
		//check if the page address is legtimate
		if (!(current_page_table->vm_pool->is_legitimate(page_fault_addr))) {
			Console::puts("The virtual address is not legitimate!\n");
			abort();
		} else {   //the address is legitimate
			//check if the page fault is in PD or PT
			//the approach is to find the entry of the fault page address in PD
			//if the entry's present bit is 1, page fault happens in PT
			//if the entry's present bit is 0, page fault happens in PD
			unsigned long fault_pd_index = page_fault_addr >> 22;
			if ((current_page_table->page_directory[fault_pd_index] & 0x1) == 0) {
				//page fault happens in PD
				unsigned long new_pt_addr;   //physical address of new page table
				new_pt_addr = process_mem_pool->get_frame() * PAGE_SIZE;
				if (new_pt_addr == 0) {   //no more frame available
					Console::puts("No available frames in kernel frame pool.\n");
					return;
				} else {   //successfully get one new frame
					//get the index of the page table entry in PDT
					unsigned long new_pd_index = page_fault_addr >> 22;
					//fill this PD entry
					current_page_table->page_directory[new_pd_index] = new_pt_addr | 0x3;   
					//supervisor, R/W, present
					//get a new page and set the first entry in this new page table
					unsigned long new_page_addr;
					new_page_addr = process_mem_pool->get_frame() * PAGE_SIZE;   //get a new page for new PT
					if (new_page_addr == 0) {   //no more frame available
						Console::puts("No available frames in process frame pool.\n");
						return;
					} else {
						//fill all the entries in this new PT
						unsigned long i;
						for (i = 0; i < ENTRIES_PER_PAGE; ++i) {
							unsigned long* temp_pt_entry = (unsigned long*) 
								((((page_fault_addr >> 10) & 0xfffff000) + i * (1 << 31)) | 0xffc00000);
							*temp_pt_entry = 0 | 0x2;
							//address 0 with superviosr, R/W, not present
						}
						//fill the corresponding entry
						unsigned long* mask_pt_entry = (unsigned long*) 
							(((page_fault_addr >> 10) & 0xfffffffc) | 0xffc00000); 
						*mask_pt_entry = new_page_addr | 0x3;   
						//supervisor, R/W, present
						return;
					}
				}
			} else {
				//page fault happens in PT
				unsigned long new_page_addr;   //physical address of new page
				new_page_addr = process_mem_pool->get_frame() * PAGE_SIZE;
				if (new_page_addr == 0) {   //no more frame available
					Console::puts("No available frames in process frame pool.\n");
					return;
				} else {   //successfully get one new frame
					//get the index of the new page entry in PT
					unsigned long* mask_pt_entry = (unsigned long*) 
							(((page_fault_addr >> 10) & 0xfffffffc) | 0xffc00000); 
					*mask_pt_entry = new_page_addr | 0x3;
					//supervisor, R/W, present
					return;
				}
			}
		}
	} else {   //page fault caused by page-protection violation
		Console::puts("Page-protection Violation!\n");
		return;
	}
}

void PageTable::free_page(unsigned long _page_no) {
	//check if the page to release is present
	if ((current_page_table->page_directory[_page_no >> 10] & 0x1) == 0) {
		Console::puts("The page to release is not present!\n");
		abort();
	} else {
		//release the virtual address
		//release the physical frame
		unsigned long* mask_pt_entry = (unsigned long*) ((_page_no << 2) | 0xffc00000);
		unsigned long physical_address = *mask_pt_entry;
		FramePool::release_frame(physical_address / PAGE_SIZE);   //frame no = physical address / page size
		//change the current page table
		*mask_pt_entry = (*mask_pt_entry & 0xfffffff8) | 0x2;
		//superviosr, R/W, not present
		//check if all the entries in this page table is not present
		unsigned long i;
		for (i = 0; i < ENTRIES_PER_PAGE; ++i) {
			unsigned long* temp_pt_entry = (unsigned long*)
				((((_page_no << 2) & 0xfffff000) + i * (1 << 31)) | 0xffc00000);
			if ((*temp_pt_entry & 0x1) != 0)   //find a present page entry
				break;
		}
		if (i == ENTRIES_PER_PAGE) {   //this page table can be released
			page_directory[_page_no >> 10] = page_directory[_page_no >> 10] & 0xfffffffe;
			//set the PD entry for this PT not present
			FramePool::release_frame(page_directory[_page_no >> 10] / PAGE_SIZE);
		}
	}
}

void PageTable::register_vmpool(VMPool* _pool) {
	vm_pool = _pool;
}
