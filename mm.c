/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team 4",
    /* First member's full name */
    "김기래",
    /* First member's email address */
    "jacti",
    /* Second member's full name (leave blank if none) */
    "김동규",
    /* Second member's email address (leave blank if none) */
    "nanamatz",
    /* Third member's full name (leave blank if none) */
    "김수민",
    /* Third member's email address (leave blank if none) */
    "SunowMin"
};

#ifdef __LP64__
    #define __WORDSIZE			64
    /* FIX : x86-64 -> 16*/
    #define ALIGNBIT            4
    typedef uint64_t            word_t;
#else
    #define __WORDSIZE			32
    /* single word (4) or double word (8) alignment */
    #define ALIGNBIT            3
    typedef uint32_t            word_t;
#endif

#define ALIGNMENT           (1 << ALIGNBIT)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)     (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define SIZE_T_SIZE     (ALIGN(sizeof(size_t)))

#define HEAP_BASE       mem_heap_lo()

#define FIRST_HEADER    (HEAP_BASE + (ALIGNMENT - (__WORDSIZE >> 3)))

#define INIT_HEAP_SIZE  (10*(1<<20)) // 10MB

typedef union _Header
{
    word_t ptr;
    struct 
    {
        word_t align : ALIGNBIT;
        word_t size : (__WORDSIZE - ALIGNBIT);
    }field;
    
}Header;


/* 
 * mm_init - initialize the malloc package.
 */
// STEP : 최초 힙 만들어 두기
//  
int mm_init(void)
{
    printf("\nHeader size is : %d\nFirst Header start at : %p\nBase Heap start at : %p\n", sizeof(Header),FIRST_HEADER,HEAP_BASE);
    mem_sbrk(INIT_HEAP_SIZE);
    Header *first_header = FIRST_HEADER;
    first_header->field.align = 0;
    first_header->field.size = INIT_HEAP_SIZE - sizeof(Header) - (FIRST_HEADER - HEAP_BASE);
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void *find_memory(size_t size)
{
    Header *header = FIRST_HEADER;
    while(header < mem_sbrk(0)){
        if(header->field.align == 0 && header->field.size > size){
            return header;
        }
        header = (Header*)((void*)header + header->field.size);
    }
    return NULL;
}

void *mm_malloc(size_t size)
{
    size_t align = ALIGN(size);
    Header *header = (Header *)find_memory(align);
    if(!header){
        return NULL;
    }

    // 분할
    Header *new_header = (Header *)((void *)header + align);
    new_header->field.size = header->field.size - align;
    new_header->field.align = 0; 

    header->field.size = align;
    header->field.align = 1;
    
    return (void *)(header +1);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    Header * header = ptr;
    ptr--;
    header->field.align = 0;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    Header *header = (Header *)ptr -1;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = header->field.size;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, ptr, copySize);
    char * _newptr = newptr;
    for( size_t i = copySize; i<size ; i++){
        _newptr[i] = 0;
    }
    mm_free(ptr);
    return newptr;
}

