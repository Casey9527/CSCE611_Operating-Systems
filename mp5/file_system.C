/* 
    File: file_system.C

    Description: File System.
    

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "file_system.H"
#include "machine.H"
#include "console.H"
#include "utils.H"
#include "scheduler.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

//File class
	
extern Scheduler* SYSTEM_SCHEDULER;
BOOLEAN File::write_lock = FALSE;

File::File(FileSystem* _file_system) {
	file_system = _file_system;
	file_id = file_system->get_inode() - INODE_OFFSET;
	if (!_file_system->CreateFile(file_id)) {   //no more file_id available
		Console::puts("The number of files exceeds the upper-bound.\n");
		for(;;);
	}
	current_pos = 0;
	size = 0;
}

File::File(FileSystem* _file_system, unsigned _file_id) {
	file_system = _file_system;
	file_id = _file_id;
	long inode[INT_PER_BLOCK];
	file_system->disk->read(file_id + INODE_OFFSET, (unsigned char*)inode);
	for (unsigned long i = 0; i < INT_PER_BLOCK; ++i) {
		if (inode[i] == -1) {   //reach the end
			size = (i - 1) * CHAR_PER_BLOCK;
		}
	}
	current_pos = 0;
}

unsigned int File::Read(unsigned int _n, unsigned char* _buf) {
	// read the inode information into memory
	long inode[INT_PER_BLOCK];
	file_system->disk->read(file_id + INODE_OFFSET, (unsigned char*)inode);
	unsigned long current_index = current_pos / CHAR_PER_BLOCK;
	unsigned long end_pos = current_pos + _n - 1;   //the last character needs to be read
	unsigned long block_need_to_read = end_pos / CHAR_PER_BLOCK - current_index + 1;
	
	// Begin Reading
	unsigned char tmp_buf[CHAR_PER_BLOCK];
	unsigned long runner = 0;
	// we need to check:
	// if the last character falls into the same block as the start character, we need to stop
	// at position of the last character; otherwise, we read until meet the end of this block
	unsigned long pos_to_stop  = block_need_to_read > 1 ? CHAR_PER_BLOCK : (end_pos + 1);
	// read the first block
	if (inode[current_index] == -1) {   //nothing can be read
		current_pos += runner;   //update current position
		return runner;
	}
	file_system->disk->read(inode[current_index], tmp_buf);
	for (unsigned long i = current_pos; i < pos_to_stop; ++i) {
		_buf[runner] = tmp_buf[i];
		++runner;
	}
	// if exist, read the contiguous blocks between the first and the last
	if (block_need_to_read > 2) {
		for (unsigned long i = 0; i < block_need_to_read - 2; ++i) {
			current_index++;
			if (inode[current_index] == -1) {   //already reach the end
				current_pos += runner;   //update current position
				return runner;
			}
			file_system->disk->read(inode[current_index], tmp_buf);
			for (unsigned long j = 0; j < CHAR_PER_BLOCK; ++j) {
				_buf[runner] = tmp_buf[j];
				++runner;
			}
		}
	}
	// if the last and the first block are not the same block, read the last block; otherwise, we
	// already finished reading
	if (block_need_to_read > 1) {
		current_index++;
		if (inode[current_index] == -1) {   //again, the size wanted to read exceeds the real size
			current_pos += runner;   //update current position	
			return runner;   //done reading
		}
		file_system->disk->read(inode[current_index], tmp_buf);
		//read until meets the end position
		for (unsigned long i = 0; i <= end_pos % CHAR_PER_BLOCK; ++i) {
			_buf[runner] = tmp_buf[i];
			++runner;
		}
	}
	current_pos += runner;   //update current position	
	return runner;
}

unsigned int File::Write(unsigned int _n, unsigned char* _buf) {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	// read the inode information into memory
	unsigned long i;
	long inode[INT_PER_BLOCK];
	file_system->disk->read(file_id + INODE_OFFSET, (unsigned char*)inode);
	unsigned long current_index = current_pos / CHAR_PER_BLOCK;
	unsigned long end_pos = current_pos + _n - 1;   //the last character needs to be write
	unsigned long block_need_to_write = end_pos / CHAR_PER_BLOCK - current_index + 1;

	// Begin Writing
	unsigned long runner = 0;
	// we need to check:
	// if the last character falls into the same block as the start character, we need to stop
	// at position of the last character; otherwise, we write until meet the end of this block
	unsigned long pos_to_stop = block_need_to_write > 1 ? CHAR_PER_BLOCK : (end_pos + 1);
	// write the first block
	if (inode[current_index] == -1) {   //nothing can be read
		current_pos += runner;   //update current position
		size += runner;
		write_lock = FALSE;   //free the lock
		return runner;
	}
	// get prepared
	i = current_pos;
	unsigned char tmp_buf[CHAR_PER_BLOCK];
	// read from the first disk, in case that the previous data will not be covered
	if (current_pos != 0) {
		file_system->disk->read(inode[current_index], tmp_buf);
		// if we are appending content to an existing file
		// check if the original file ends at \0, if yes,
		// starts at one character ahead of where it should be, in order to overwrite the \0
		if (tmp_buf[current_pos-1] == '\0')
			--i;
	}
	// write to the buffer starting from the current position
	for (; i < pos_to_stop; ++i) {
		tmp_buf[i] = _buf[runner];
		++runner;
	}
	// write to the disk
	file_system->disk->write(inode[current_index], tmp_buf);
	// if exist, write onto the contiguous blocks between the first and the last
	if (block_need_to_write > 2) {
		for (unsigned long i = 0; i < block_need_to_write - 2; ++i) {
			// get a free block to write
			long new_block = file_system->get_block();
			// update the inode
			current_index++;
			if (current_index >= INT_PER_BLOCK) {   // the file size exceeds the bound
				current_pos += runner;
				size += runner;
				write_lock = FALSE;   //free the lock
				return runner;
			}
			inode[current_index] = new_block;
			for (unsigned long j = 0; j < CHAR_PER_BLOCK; ++j) {
				tmp_buf[j] = _buf[runner];
				++runner;
			}
			file_system->disk->write(inode[current_index], tmp_buf);
		}
		file_system->disk->write(file_id + INODE_OFFSET, (unsigned char*)inode);
	}
	// if the last and the first block are not the same block, write the last block; otherwise, we
	// already finished writing
	if (block_need_to_write > 1) {
		if (current_index >= INT_PER_BLOCK) {   // the file size exceeds the bound
			current_pos += runner;
			size += runner;
			write_lock = FALSE;   //free the lock
			return runner;
		}
		// get a free block to write
		long new_block = file_system->get_block();
		// update the inode
		current_index++;
		inode[current_index] = new_block;
		if (inode[current_index] == -1) {   //again, the size wanted to write exceeds the real size
			current_pos += runner;   //update current position
			size += runner;
			write_lock = FALSE;   //free the lock
			return runner;   //done writing
		}
		// write until meets the end position
		unsigned long i;
		for (i = 0; i <= end_pos % CHAR_PER_BLOCK; ++i) {
			tmp_buf[i] = _buf[runner];
			++runner;
		}
		file_system->disk->write(inode[current_index], (unsigned char*)tmp_buf);
		file_system->disk->write(file_id + INODE_OFFSET, (unsigned char*)inode);
	}
	current_pos += runner;   //update current position
	size += runner;
	write_lock = FALSE;   //free the lock
	return runner;
}

void File::Reset() {
	current_pos = 0;
}

void File::Rewrite() {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	// read the inode information into memory
	long inode[INT_PER_BLOCK];
	file_system->disk->read(file_id + INODE_OFFSET, (unsigned char*)inode);
	unsigned long i = 1;
	while (inode[i] != -1 && i < INT_PER_BLOCK) {   
		//free all the blocks owned, except for the first one
		file_system->free_block(inode[i]);
		++i;
	}
	// clean the first block
	unsigned long tmp_buf[INT_PER_BLOCK];
	for (i = 0; i < INT_PER_BLOCK; ++i)
		tmp_buf[i] = 0;
	file_system->disk->write(inode[0], (unsigned char*)tmp_buf);
	current_pos = 0;
	size = 0;
	write_lock = FALSE;   //free the lock
}

BOOLEAN File::EoF() {
	return (current_pos >= size ? TRUE : FALSE);
}

// FileSystem class

BOOLEAN FileSystem::write_lock = FALSE;
BOOLEAN FileSystem::inode_write_lock = FALSE;

long FileSystem::get_inode() {
	while (inode_write_lock) {}   //wait other threads to finish writing
	inode_write_lock = TRUE;   //set the lock
	
	// search the first 160 blocks
	// 160 bits / 32 bits_per_integer = 5 integers
	// however, the first 5 bits are for free list, therefore there is a 5-bit offset
	for (unsigned long i = 0; i < INODE_BITMAP_LENGTH; ++i) {
		unsigned long searcher = 0x80000000;
		for (unsigned long j = 0; j < INT_LENGTH; ++j) {
			if ((searcher & free_list[i]) != 0) {   //find the free inode
				// set the corresponding bit to 0
				free_list[i] = free_list[i] & ~searcher;
				// rewrite the free list to disk
				for (unsigned long k = 0; k < BLOCK_FOR_FREELIST; ++k) {
					disk->write(k, (unsigned char*)(free_list + k * INT_PER_BLOCK));
				}
				inode_write_lock = FALSE;   //free the lock
				return (i * INT_LENGTH + j);
			}
			else
				searcher = searcher >> 1;
		}
	}
	// check the last 5 bits
	unsigned long searcher = 0x80000000;
	for (unsigned long j = 0; j < INODE_OFFSET; ++j) {
		if ((searcher & free_list[INODE_OFFSET]) != 0) {
			free_list[INODE_OFFSET] = free_list[INODE_OFFSET] & ~searcher;
			// rewrite the free list to disk
			for (unsigned long k = 0; k < BLOCK_FOR_FREELIST; ++k) {
				disk->write(k, (unsigned char*)(free_list + k * INT_PER_BLOCK));
			}
			inode_write_lock = FALSE;   //free the lock
			return (INODE_OFFSET * INT_LENGTH + j);
		}
		else
			searcher = searcher >> 1;
	}
	// if get here, no inode is available
	inode_write_lock = FALSE;   //free the lock
	return -1;
}

long FileSystem::get_block() {
	while (inode_write_lock) {}   //wait other threads to finish writing
	inode_write_lock = TRUE;   //set the lock
	
	// search from the 161th block
	// in the 161th block, the first 5 bits are for inode
	unsigned long searcher = 0x80000000 >> 5;
	for (unsigned long j = 5; j < INT_LENGTH; ++j) {
		if ((searcher & free_list[INODE_BITMAP_LENGTH]) != 0) {   //find the free block
			free_list[INODE_BITMAP_LENGTH] = free_list[INODE_BITMAP_LENGTH] & ~searcher;
			// rewrite the free list to disk
			for (unsigned long k = 0; k < BLOCK_FOR_FREELIST; ++k) {
				disk->write(k, (unsigned char*)(free_list + k * INT_PER_BLOCK));
			}
			inode_write_lock = FALSE;   //free the lock
			return (INODE_BITMAP_LENGTH * INT_LENGTH + j);
		}
		searcher = searcher >> 1;
	}
	// searche other blocks
	for (unsigned long i = INODE_BITMAP_LENGTH + 1; i < INT_FOR_FREELIST; ++i) {
		searcher = 0x80000000;
		for (unsigned long j = 0; j < INT_LENGTH; ++j) {
			if ((searcher & free_list[i]) != 0) {   //find the free block
				// set the corresponding bit to 0
				free_list[i] = free_list[i] & ~searcher;
				// rewrite the free list to disk
				for (unsigned long k = 0; k < BLOCK_FOR_FREELIST; ++k) {
					disk->write(k, (unsigned char*)(free_list + k * INT_PER_BLOCK));
				}
				inode_write_lock = FALSE;   //free the lock
				return (i * INT_LENGTH + j);
			}
			else
				searcher = searcher >> 1;
		}
	}
	// if get here, no block is available
	inode_write_lock = FALSE;   //free the lock
	return -1;
}

void FileSystem::free_inode(long allocated_inode) {
	while (inode_write_lock) {}   //wait other threads to finish writing
	inode_write_lock = TRUE;   //set the lock
	
	if (allocated_inode == -1 || allocated_inode >= 160) {
		//inode dose not exist or the block to free is not inode
		inode_write_lock = FALSE;   //free the lock
		return;
	}
	unsigned long searcher = 0x80000000;
	unsigned long index = (allocated_inode + INODE_OFFSET) / INT_LENGTH;
	unsigned long offset = (allocated_inode + INODE_OFFSET) % INT_LENGTH;
	free_list[index] = free_list[index] | (searcher >> offset);   //set the corresponding bit to 1
	// rewrite the free list to disk
	for (unsigned long i = 0; i < BLOCK_FOR_FREELIST; ++i) {
		disk->write(i, (unsigned char*)(free_list + i * INT_PER_BLOCK));
	}
	inode_write_lock = FALSE;   //free the lock
}

void FileSystem::free_block(long allocated_block) {
	while (inode_write_lock) {}   //wait other threads to finish writing
	inode_write_lock = TRUE;   //set the lock
	
	if (allocated_block == -1 || allocated_block < 160) {   
		//inode dose not exist or the block to free is not file block
		inode_write_lock = FALSE;   //free the lock
		return;
	}
	unsigned long searcher = 0x80000000;
	unsigned long index = allocated_block / INT_LENGTH;
	unsigned long offset = allocated_block % INT_LENGTH;
	free_list[index] = free_list[index] | (searcher >> offset);   //set the corresponding bit to 1
	// rewrite the free list to disk
	for (unsigned long i = 0; i < BLOCK_FOR_FREELIST; ++i) {
		disk->write(i, (unsigned char*)(free_list + i * INT_PER_BLOCK));
	}
	inode_write_lock = FALSE;   //free the lock
}

BOOLEAN FileSystem::Mount(SimpleDisk* _disk) {
	/* the disk is formatted as follows:
	 *            -------------------
	 *  5 blocks  | free block list |
	 *            -------------------
	 * 160 blocks |     inodes      |
	 *            -------------------
	 *            |     files       |
	 *            -------------------
	 */
	if (_disk->size() < 165)   //if the disk is smaller than 165 blocks
		return FALSE;   //then this disk is illegal
	size = _disk->size();
	disk = _disk;
	// read the free block list from the disk
	for (unsigned long i = 0; i < BLOCK_FOR_FREELIST; ++i) {
		disk->read(i, (unsigned char*)(free_list + i * INT_PER_BLOCK));
	}
	return TRUE;
}

BOOLEAN FileSystem::Format(SimpleDisk* _disk, unsigned int _size) {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	// free block list takes 5 blocks;
	// inodes take 160 blocks.
	/* format the disk as follows:
	 *            -------------------
	 *  5 blocks  | free block list |
	 *            -------------------
	 * 160 blocks |     inodes      |
	 *            -------------------
	 *            |     files       |
	 *            -------------------
	 */
	if (_size < 165)   //if the size is insufficent for disk management information
		return FALSE;   //then fails in formatting
	unsigned long tmp_buf[INT_PER_BLOCK];
	for (unsigned long i = 0; i < INT_PER_BLOCK; ++i) {   //mark as unused
		tmp_buf[i] = 0xffffffff;
	}
	for (unsigned long i = 1; i < BLOCK_FOR_FREELIST; ++i) {
		_disk->write(i, (unsigned char*)tmp_buf);
	}
	tmp_buf[0] = tmp_buf[0] >> 5;   //mark the first 5 blocks as used
	_disk->write(0, (unsigned char*)tmp_buf);
	
	write_lock = FALSE;   //free the lock
	return TRUE;
}

BOOLEAN FileSystem::LookupFile(int _file_id) {
	unsigned long searcher = 0x80000000;
	unsigned long index = (_file_id + INODE_OFFSET) / INT_PER_BLOCK;
	unsigned long offset = (_file_id + INODE_OFFSET) % INT_PER_BLOCK;
	if ((free_list[index] & (searcher >> offset)) == 1) {   //the corresponding inode is available
		// file does not exist
		return FALSE;
	}
	else {
		return TRUE;
	}
}

BOOLEAN FileSystem::CreateFile(int _file_id) {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	if (!LookupFile(_file_id)) {   //the inode index has already been allocated
		write_lock = FALSE;   //free the lock
		return FALSE;
	}
	else {   //the file ID has not been used
		long tmp_buf[INT_PER_BLOCK];
		// get one free block for the file
		long free_block_no = get_block();
		// initialize the inode and write the block no. onto the inode
		for (unsigned long i = 0; i < INT_PER_BLOCK; ++i)
			tmp_buf[i] = -1;
		tmp_buf[0] = free_block_no;
		// write the inode onto disk
		disk->write(_file_id + INODE_OFFSET, (unsigned char*)tmp_buf);
		write_lock = FALSE;   //free the lock
		return TRUE;
	}
}

BOOLEAN FileSystem::DeleteFile(int _file_id) {
	while (write_lock) {}   //wait other threads to finish writing
	write_lock = TRUE;   //set the lock
	
	unsigned long searcher = 0x80000000;
	unsigned long index = (_file_id + INODE_OFFSET) / INT_LENGTH;
	unsigned long offset = (_file_id + INODE_OFFSET) % INT_LENGTH;
	if ((free_list[index] & (searcher >> offset)) != 0) {   //the file does not exist
		write_lock = FALSE;   //free the lock
		return FALSE;
	}
	else {
		// get the inode of the file
		long tmp_inode[INT_PER_BLOCK];
		disk->read(_file_id + INODE_OFFSET, (unsigned char*)tmp_inode);
		// free all allocated blocks
		unsigned long i;
		for (i = 0; i < INT_PER_BLOCK; i++) {
			long allocated_block = tmp_inode[i];
			free_block(allocated_block);
		}
		// free the inode
		free_inode(_file_id);
		write_lock = FALSE;   //free the lock
		return TRUE;
	}
}
