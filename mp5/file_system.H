/* 
    File: file_system.H

    Description: File System.
    

*/

#ifndef _FILE_SYSTEM_H_                   // include file only once
#define _FILE_SYSTEM_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/
#define INT_LENGTH 32   //the length of integer is 32 bits
#define CHAR_PER_BLOCK 512   
#define INT_PER_BLOCK 128
#define TOTAL_INODE 160   //160 inodes are needed.
#define BLOCK_FOR_FREELIST 5   //5 blocks for bitmap
#define INT_FOR_FREELIST 640
#define CHAR_FOR_FREELIST 2560
#define INODE_OFFSET 5   //inode region starts at the 5th block
#define INODE_BITMAP_LENGTH 5   //160 inodes corresponds to 5 integers in free list

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "utils.H"
#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS */ 
/*--------------------------------------------------------------------------*/

class FileSystem;

/*--------------------------------------------------------------------------*/
/* F i l e */
/*--------------------------------------------------------------------------*/

class File {
private: 
	
	//static const unsigned long int_per_block = 512 / sizeof(int);
	//static const unsigned long char_per_block = 512 / sizeof(char);   

	FileSystem* file_system;
	unsigned int file_id;

	unsigned long size;
	unsigned long current_pos;
	
	static BOOLEAN write_lock;

public:

    File(FileSystem* _file_system);
    /* Constructor for the file handle. Set the 'current
       position' to be at the beginning of the file. */
    
	File(FileSystem* _file_system, unsigned int _file_id);

    unsigned int FileName() {return file_id;}
	/* return the file name (file id). */

	unsigned int Read(unsigned int _n, unsigned char * _buf);
    /* Read _n characters from the file starting at the 
       current location and copy them in _buf.
       Return the number of characters read. */

    unsigned int Write(unsigned int _n, unsigned char * _buf);
    /* Write _n characters to the file starting at the current 

       location, if we run past the end of file, we increase 
       the size of the file as needed. 
     */

    void Reset();
    /* Set the 'current position' at the beginning of the file. */

    void Rewrite();
    /* Erase the content of the file. Return any freed blocks. 
       Note: This function does not delete the file! It just erases its
       content. */

    BOOLEAN EoF();
    /* Is the current location for the file at the end of the file? */
};

/*--------------------------------------------------------------------------*/
/* F i l e S y s t e m  */
/*--------------------------------------------------------------------------*/

class FileSystem {

friend class File;

private:
	
	/* Here, we use indexed allocation to implement file system.
	 * To simplify the approach, the inodes are single level.

	 * One block is used for one inode, and one block can contain 128 indices,
	 * therefore the file length is bounded by 128 * 512B = 64KB.
	 * The total number of files is bounded by 10MB / 64KB = 160, so 160 inodes are needed.
	 * We use bitmap for all blocks to facilitate free block allocation.
	 * 10MB / 512B = 20K bits are needed, and one block has 512B * 8 = 4K bits.
	 * Threfore, 20K / 4K = 5 blocks are needed to store bitmap.
	 */

	SimpleDisk* disk;
	unsigned int size;
	unsigned long free_list[INT_FOR_FREELIST];
	static BOOLEAN write_lock;
	static BOOLEAN inode_write_lock;

public:

	FileSystem() {}
	/* Just initializes local data structures. Does not connect to disk yet. */

	long get_inode();   //get an available file index
	long get_block();   //get a free block according to the free block bitmap
	void free_inode(long allocated_inode);   //free the allocated inode
	void free_block(long allocated_block);   //free the allocated block
	
	BOOLEAN Mount(SimpleDisk * _disk);
	/* Associates the file system with a disk. We limit ourselves to at most one
      file system per disk. Returns TRUE if 'Mount' operation successful (i.e. there
      is indeed a file system on the disk. */

	BOOLEAN Format(SimpleDisk * _disk, unsigned int _size);
	/* Wipes any file system from the given disk and installs a new, empty, file
      system that supports up to _size Byte. */

	BOOLEAN LookupFile(int _file_id);
	/* Find file with given id in file system. If found, initialize the file 
      object and return TRUE. Otherwise, return FALSE. */

	BOOLEAN CreateFile(int _file_id);
	/* Create file with given id in the file system. If file exists already,
      abort and return FALSE. Otherwise, return TRUE. */

	BOOLEAN DeleteFile(int _file_id);
	/* Delete file with given id in the file system and free any disk block
      occupied by the file. */
};

#endif
