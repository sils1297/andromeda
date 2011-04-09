/*
    Orion OS, The educational operatingsystem
    Copyright (C) 2011  Bart Kuivenhoven

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This code still needs improvement.
/*
 * Right now the code assumes all of the memory is available, as in reality
 * it isn't. The memory needs to be limited, to what is available.
 * This requires growing the heap dynamically to it's maximum size, if possible.
 * We need the multi-boot-header for that though, and that requires a little
 * more investigation.
 */

#include <mm/memory.h>
#include <mm/heap.h>
#include <error/panic.h>
#include <text.h>

#define ALLOC_MAX 4*1024*1024

#define HDRMAGIC 0xAF00BEA8
#define PAGEBOUNDARY 0x1000

memNode_t* blocks = NULL; // Head pointer of the linked list maintaining the heap


// Some random headers used later in the code
boolean useBlock(memNode_t* block);
void returnBlock(memNode_t* block);
memNode_t* split(memNode_t* block, size_t size);
memNode_t* splitMul(memNode_t* block, size_t size, boolean pageAlligned);
memNode_t* merge(memNode_t* alpha, memNode_t* beta);

#ifdef MMTEST
// Debugging function used to examine the heap (duh ...)
void examineHeap()
{
	printf("Head\n");
	printhex((int)blocks); putc('\n');
	memNode_t* carrige;
	for (carrige = blocks; carrige!=NULL; carrige=carrige->next)
	{
		printf("node: "); printhex((int)carrige); putc('\n');
	}
}

#endif

// This code is called whenever a new block header needs to be created.
// It initialises the header to a good position and simplifies the code a bit.
void initHdr(memNode_t* block, size_t size)
{
	block->size = size;
	block->previous = NULL;
	block->next = NULL;
	block->used = FALSE;
	block->hdrMagic = HDRMAGIC;
}
// This initialises the heap to hold a block of the maximum possible size.
// In the case of the compressed kernel that's 128 MB, which is huge, since
// allocmax = 4KB
void initBlockMap ()
{
	memNode_t* node = (memNode_t*)heapBase;
	initHdr(node, heapSize-sizeof(memNode_t));
	blocks = node;
	#ifdef MMTEST
	testAlloc();
	#endif
}


// Finds a block on the heap, which is free and which is large enough.
// In the case that pageAlligned is enabled the block also has to hold
// page alligned data (usefull for the page directory).
void* alloc (size_t size, boolean pageAlligned)
{
	if(size > ALLOC_MAX)
	{
		return NULL;
	}
	memNode_t* carrige;
	for(carrige = blocks; carrige!=NULL; carrige=carrige->next)
	{
		if (pageAlligned == TRUE)
		{
			if (!carrige->used)
			{
				/* 
				 * If the block isn't used and the block should be alligned with the page boundary
				 * this block is called. The ifdef below is just to get the syntax right for different
				 * kind of architectures. X86 reffers to a 32-bits environment, else we'll assume a
				 * 64 bits environment.
				 *
				 * The code figures out the required offset for the block to be able to hold the desired
				 * block.
				 */
				#ifdef X86
				unsigned int offset = PAGEBOUNDARY-((int)carrige+sizeof(memNode_t))%PAGEBOUNDARY;
				#else
				unsigned int offset = PAGEBOUNDARY-(long long)carrige+sizeof(memNode_t)%PAGEBOUNDARY;
				#endif
				
				offset %= PAGEBOUNDARY;
				unsigned int blockSize = offset+size;
				
				// The below code is debugging code
				#ifdef MMTEST
				printf("BlockSize:\t");
				printhex(blockSize); putc('\n');
				printf("Offset:\t");
				printhex(offset); putc('\n');
				printf("Size:\t");
				printhex(size); putc('\n');
				printf("Carrige size:\t");
				printhex(carrige->size); putc('\n');
				printf("Ifresult:\t");
				printhex(carrige->size-blockSize); putc('\n');
				#endif
				if (carrige->size >= blockSize) // if the size is large enough to be split into
								// page alligned blocks, then do it.
				{
					memNode_t* ret = splitMul(carrige, size, TRUE); // Split the block
					useBlock(ret); // Mark the block as used
					// Display the block size if debugging is compiled in
					#ifdef MMTEST
					printf("Size of block\n");
					printhex(ret->size); putc('\n');
					#endif
					//return the desired block
					return (void*)ret+sizeof(memNode_t);
				}
				else
				{
					// The block isn't the right size so find another one.
					continue;
				}
			}
			else
			{
				// The block is used, which may be the case in multi threaded environments.
				// This means the other thread isn't done with this block and we need to
				// leave it alone. This code still isn't thread safe, but it's a start.
				continue;
			}
		}
		else if (carrige->size >= size && carrige->size < size+sizeof(memNode_t))
		{
			if (useBlock(carrige) == TRUE) // check the usage of the block
			{
				continue;
			}
			// If the block is the right size or too small to hold 2 separate blocks,
			// In which one of them is the size allocated, then allocate the entire block.
			#ifdef MMTEST
			printf("Size of block\n");
			printhex(carrige->size); putc('\n');
			#endif
			return (void*)carrige+sizeof(memNode_t);
		}
		else if(carrige->size >= size+sizeof(memNode_t)) // the block is too large
		{
			if(carrige->used!=FALSE) // assert that the block isn't used
			{
				continue;
			}
			
			memNode_t* tmp = split(carrige, size);  // split the block
			if(useBlock(tmp) == TRUE) // mark the block used.
			{
				continue;
			}
			#ifdef MMTEST
			printf("Size of block\n");
			printhex(tmp->size); putc('\n');
			#endif
			return (void*)tmp+sizeof(memNode_t);
		}
		if (carrige->next == NULL || carrige->next == carrige)
		{
			break; 	// If we haven't found anything but we're at the end of the list
				// or heap corruption occured we break out of the loop and return
				// the default pointer (which is NULL).
		}
	}
	return NULL;
}

int free (void* ptr)
{
	memNode_t* block = (void*)ptr-sizeof(memNode_t);
	memNode_t* carrige;
	if (block->hdrMagic != HDRMAGIC)
	{
		return -1;
	}
	
	// Try to put the block back into the list of free nodes,
	// Actually claim it's free and then merge it into the others if possible.
	// This code is littered with debugging code.
	
	// Debugging code
	#ifdef MMTEST
	printf("Before:\n");
	examineHeap();
	printf("\n");
	#endif
	returnBlock(block); // actually mark the block unused.
	// more debugging code
	#ifdef MMTEST
	printf("During:\n");
	examineHeap();
	printf("\n");
	#endif
	// Now find a place for the block to fit into the heap list
	for (carrige = blocks; carrige!=NULL; carrige=carrige->next) // Loop through the heap list
	{
		// if we found the right spot, merge the lot.
		if ((void*)block+block->size+sizeof(memNode_t) == (void*)carrige || (void*)carrige+carrige->size+sizeof(memNode_t) == (void*)block)
		{
			memNode_t* test = merge(block, carrige); // merging code
			if (test == NULL) // if the merge failed
			{
				printf("Merge failed\n");
				#ifdef MMTEST
				printf("After\n");
				examineHeap();
				printf("\n");
				wait();
				#endif
				return -1;
			}
			else
			{
				block = test;
				carrige = test;
				// We can now continue trying to merge the rest of the list, which might be possible.
			}
		}
	}
	// Even more debugging code
	#ifdef MMTEST
	printf("After\n");
	examineHeap();
	printf("\n");
	#endif
	
	return 0; // Return success
}

boolean useBlock(memNode_t* block)
{
	// mark the block as used and remove it from the heap list
	if(block->used == FALSE)
	{
		block->used = TRUE;
		if (block->previous!=NULL) // if we're not at the top of the list
		{
			block->previous->next = block->next; // set the previous block to hold the next block
		}
		else if (blocks == block) // if we are at the top of the list, move the top of the list to the next block
		{
			blocks = block->next;
		}
		if (block->next!=NULL) // if we're not at the end of the list
		{
			block->next->previous = block->previous; // set the next block to hold the previous block
		}
		// Over here the block should be removed from the heap lists.
		return FALSE; // return that the block wasn't used.
	}
	else
	{
		return TRUE;
	}
}
void returnBlock(memNode_t* block)
{
	// This code marks the block as unused and puts it back in the list.
	if (block->used == FALSE || block->hdrMagic != HDRMAGIC) // Make sure we're not corrupting the heap
	{
		#ifdef MMTEST
		printf("WARNING");
		#endif
		return;
	}
	block->used = FALSE;
	memNode_t* carrige;
	if ((void*)block < (void*)blocks && (void*)block >= (void*)heapBase)
	{// if we're at the top of the heap list add the block there.
		blocks -> previous = block;
		block -> next = blocks;
		block -> previous = NULL;
		blocks = block;
		return;
	}
	// We're apparently not at the top of the list
	for (carrige=blocks; carrige!=NULL; carrige=carrige->next) // Loop through the heap list.
	{
		if ((void*)carrige+sizeof(memNode_t)+carrige->size == block) // if the carrige connects to the bottom of our block
		{
			block -> next = carrige -> next;
			block -> previous = carrige;
			carrige -> next = block;
			return; // add the block to the list after the carrige
		}
		else if ((void*)block+sizeof(memNode_t)+block->size == carrige) // if the block connects to the bottom of the carrige
		{
			block -> next = carrige;
			block -> previous = carrige -> previous;
			carrige -> previous = block;
			return; // add the block to the list before the carrige
		}
		else if (carrige->next == NULL)
		{
			carrige -> next = block;
			block -> previous = carrige;
			return; // if we have gotten to the end of the heap we must add the block here
		}
	}
}
memNode_t* split(memNode_t* block, size_t size)
{
	// This code splits the block into two parts, the lower of which is returned
	// to the caller.
	memNode_t* second = (memNode_t*)((void*)(block)+size+sizeof(memNode_t)); // find out what the address of the upper block should be
	initHdr(second, block->size-size-sizeof(memNode_t)); // initialise the second block to the right size
	second->previous = block; // fix the heap lists
	second->next = block->next;
	block->next = second;
	block->size = size;
	return block; // return the bottom block
}
memNode_t* splitMul(memNode_t* block, size_t size, boolean pageAlligned)
{
	// if the block should be pageAlligned
	if (pageAlligned)
	{
		// figure out whether or not the block is at the right place
		#ifdef X86
		if (((int)((void*)block+sizeof(memNode_t)))%PAGEBOUNDARY == 0)
		#else
		if (((long long)((void*)block+sizeof(memNode_t)))%PAGEBOUNDARY == 0)
		#endif
		{	// if so we can manage with a simple split
			#ifdef MMTEST
			printf("Simple split\n");
			#endif
			// If this block gets reached the block is at the offset in memory.
			return split (block, size);
		}
		#ifdef X86
		else if ((int)((void*)block+sizeof(memNode_t))%PAGEBOUNDARY != 0)
		#else
		else if ((long long)((void*)block+sizeof(memNode_t))%PAGEBOUNDARY != 0)
		#endif
		{	// if not we must do a bit more complex
			#ifdef MMTEST
			printf("Complex split\n");
			#endif
			// If we get here the base address of the block isn't alligned with the offset.
			// Split the block and then use split on the higher block so the middle is
			// pageAlligned.
			// Below we figure out where the second block should start using some algorithms
			// of which it isn't if a shame a beginner doesn't fully get it.
			#ifdef X86
			unsigned int secondAddr;
			unsigned int base = (unsigned int)((void*)block+2*sizeof(memNode_t)); // the base address is put in an int with some header
											      // sizes because the calculation requires them.
			unsigned int offset = PAGEBOUNDARY-(base%PAGEBOUNDARY); // the addrress is used to figure out the offset to the page boundary
			secondAddr = (unsigned int)((void*)block+sizeof(memNode_t)); // put the base address into second
			secondAddr += offset; // add the offset to second
			memNode_t* second = (void*)secondAddr; // put the actual address in second
			#else
			unsigned long long secondAddr; // do the same as above but now for 64-bits machines.
			unsigned long long base = (unsigned long long)((void*)block+2*sizeof(memNode_t));
			unsigned long long offset = PAGEBOUNDARY-(base%PAGEBOUNDARY);
			secondAddr = (unsigned long long)((void*)block+sizeof(memNode_t));
			secondAddr += offset;
			memNode_t* second = (void*)secondAddr;
			#endif
			memNode_t* next = block->next; // Temporarilly store next
			int secondSize = block->size - ((void*)second - (void*)block); // second's temporary size gets calculated.
			initHdr(second, secondSize); // init the second block with the temporary size
			block->size = (void*)second-((void*)block+sizeof(memNode_t)); // fix the original block size as it isn't correct anymore.
			block->next = second; // fix the heap lists to make a split or return possible
			second->previous = block;
			second->next = next;
			if (second->size>size+sizeof(memNode_t))
			{
				return split(second, size); // if the second block still is too large do a normal split because this will return the
							    // right address anyways.
			}
			else
			{
				return second; // the size is right and at the right address, what more could we want.
			}
		}
	}
	else
	{
		// here we can just do a normal block split because there is no address requirement.
		return split(block, size);
	}
}
memNode_t* merge(memNode_t* alpha, memNode_t* beta)
{
	// First we check for possible corruption
	if (alpha->hdrMagic != HDRMAGIC || beta->hdrMagic != HDRMAGIC)
	{
		#ifdef MMTEST
		printf("HDR error\n"); // debugging code
		#endif
		return NULL; // return error
	}
	if ((alpha->next != beta) && (beta->next != alpha))
	{ // if the pointers don't match, we should not proceed.
		#ifdef MMTEST
		printf("WARNING!!!\n"); // more debugging code
		
		printf("Alpha->next:\t");
		printhex((int)(void*)alpha->next); putc('\n');
		printf("Beta->next:\t");
		printhex((int)(void*)beta->next); putc('\n');
		printf("\nAlpha:\t");
		printhex((int)(void*)alpha); putc('\n');
		printf("Alpha:\t");
		printhex((int)(void*)beta); putc('\n');
		#endif
		return NULL; // return error
	}
	memNode_t* tmp;
	if ((void*)alpha+alpha->size+sizeof(memNode_t) == (void*)beta)
	{	// if the blocks are in reversed order, put them in the right order
		tmp = alpha;
		alpha = beta;
		beta = tmp;
		#ifdef MMTEST
		printf("Alpha\n"); // even more debugging info
		#endif
	}
	beta->size = beta->size+alpha->size+sizeof(memNode_t); // do the actual merging
	beta->next = alpha->next;
	beta->used = FALSE;
	return beta; // return success
}