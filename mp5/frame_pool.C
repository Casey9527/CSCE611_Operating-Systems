/*
	File: frame_pool.C

    Description: Management of the Free-Frame Pool.


*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define KERNEL_MAP 0x200000   //2MB

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "frame_pool.H"
#include "console.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

unsigned long FramePool::kernel_base;
unsigned long FramePool::kernel_capacity;
unsigned long FramePool::process_base;
unsigned long FramePool::process_capacity;
unsigned long FramePool::process_map;

/*--------------------------------------------------------------------------*/
/* F r a m e   P o o l  */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
FramePool::FramePool(unsigned long _base_frame_no,
					unsigned long _nframes,
					unsigned long _info_frame_no) {
	//record the corrsponding information
	base_frame_no = _base_frame_no;
	this_capacity = _nframes;
	//check whether the frame pool is built for kernel memory or process memory
	if (_info_frame_no == 0) {
		bitmap = (unsigned long*) KERNEL_MAP;   //store bitmap at 2MB
		kernel_base = base_frame_no;   //record kernel memory base
		kernel_capacity = this_capacity;   //record the capacity
	} else {
		process_map = _info_frame_no;
		bitmap = (unsigned long*) process_map;   //store bitmap
		process_base = base_frame_no;   //record process memory base
		process_capacity = this_capacity;   //record the capacity
	}
	//initialization: set all the valid bits to 1
	unsigned long i;
	for (i = 0; i < this_capacity / 32; i++) {
		bitmap[i] = ~0;
	}
	bitmap[i] = ~(~0 >> this_capacity % 32);
	//set the valid bit corresponding to the bitmap to 0
	//to prevent the frame storing bitmap not to be touched
	unsigned long* tmp_map = (unsigned long*) KERNEL_MAP;
	if (_info_frame_no == 0) {
		tmp_map[KERNEL_MAP / 32] =
			tmp_map[KERNEL_MAP / 32] & ~(0x80000000 >> KERNEL_MAP % 32);
	} else {
		tmp_map[(process_map - kernel_base) / 32] =
			tmp_map[(process_map - kernel_base) / 32] &
			~(0x80000000 >> (process_map - kernel_base) % 32);
	}
}

unsigned long FramePool::get_frame() {
	unsigned long i;
	unsigned long start = 0;
	unsigned long searcher = 0x80000000;   //set a searcher for finding
	for (i = 0; i < this_capacity; i++) {
		if (bitmap[start] == 0) {   //no need to check these 32 bits
			start++;
			i += 32;
			continue;
		}
		if(searcher == 0) {
			searcher = 0x80000000;   //start again
			start++;
		}
		if((bitmap[start] & searcher) != 0) {   //find the frame whose valid bit is 1
			searcher = ~searcher;   //clean the valid bit of the frame
			bitmap[start] = bitmap[start] & searcher;
			return base_frame_no + i;   //return the frame number
		} else {
			searcher = searcher >> 1;
		}
	}
	return 0;
}

void FramePool::mark_inaccessible(unsigned long _base_frame_no,
								unsigned long _nframes) {
	unsigned long diff = _base_frame_no - base_frame_no;
	//set all the corresponding valid bits to 0
	if (_nframes <= (32 - diff % 32)) {   //start and end in the same frame
		bitmap[diff / 32] = bitmap[diff / 32] &
			((~0 << (32 - diff % 32)) | (~0 >> ((diff + _nframes) % 32)));
	} else {   //general cases
		bitmap[diff / 32] = bitmap[diff / 32] & ~((1 << (32 - diff % 32)) - 1);
		unsigned long i;
		for (i = 1; i < (_nframes - (32 - diff % 32)) / 32; i++) {
			bitmap[diff / 32 + i] = 0x0;
		}
		bitmap[diff / 32 + i + 1] = bitmap[diff / 32 + i + 1]
			& (~0 >> (diff + _nframes) % 32);
	}
}

void FramePool::release_frame(unsigned long _base_frame_no) {
	unsigned long* getmap;   //store the address of corresponding bit map
	unsigned long this_base;   //store the base frame number of corresponding frame pool
	if (_base_frame_no >= kernel_base 
			&& _base_frame_no - kernel_base <= kernel_capacity) {
		//the frame is within kermel memory
		getmap = (unsigned long*) KERNEL_MAP;
		this_base = kernel_base;
	} else if (_base_frame_no >= process_base
			&& _base_frame_no - process_base <= process_capacity) {
		//the frame is within process memory
		getmap = (unsigned long*) process_map;
		this_base = process_base;
	} else {
		getmap = NULL;
		return;
	}
	unsigned long diff = _base_frame_no - this_base;
	getmap[diff / 32] = getmap[diff / 32] | (1 << (32 - diff % 32));
	//set the corresponding valid bit to 1
}
