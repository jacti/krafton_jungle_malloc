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

#if defined __x86_64__ && !defined __ILP32__
    #define __WORDSIZE			64
    /* FIX : x86-64 -> 16*/
    #define ALIGNBIT            4
    #define WSIZE               8
    #define DWSIZE              16
    typedef uint64_t            word_t;
#else
    #define __WORDSIZE			32
    /* single word (4) or double word (8) alignment */
    #define ALIGNBIT            3
    #define WSIZE               4
    #define DWSIZE              8
    typedef uint32_t            word_t;
#endif

static unsigned ALIGNMENT = 1 << ALIGNBIT;

#define SIZEBIT             (__WORDSIZE - ALIGNBIT)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)     (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define SIZE_T_SIZE     (ALIGN(sizeof(size_t)))

#define CHUNK_SIZE      (4*(1<<10))     // 4KB

#define MAX(a,b)       (a >= b ? a : b)

static void* HEAD;

/* 
 * mm_init - initialize the malloc package.
 */
// NOTE : 최초 힙 공간 설정
//  prologue, epilogue 생성
int mm_init(void)
{
   
}

// NOTE : 힙 영역 확장
static void *expand_heap(size_t size)
{
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void *find_memory(size_t size)
{
}

void *mm_malloc(size_t size)
{
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
}

