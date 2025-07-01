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

#define HEAP_BASE       (mem_heap_lo())

// #define INIT_HEAP_SIZE  (10*(1<<20)) // 10MB

#define CHUNK_SIZE      (4*(1<<10))     // 4KB

#define MAX(a,b)       (a >= b ? a : b)

static void* HEAD;

typedef union {
    word_t raw;
    struct 
    {
        word_t align : ALIGNBIT;
        word_t size : SIZEBIT;
    };
}Header;

#define GET_SIZE(p)             (((Header*)p)->size << ALIGNBIT)
#define SET_SIZE(p, new_size)             (((Header*)p)->size = new_size >> ALIGNBIT)

#define GET_ALIGN(p)            (((Header*)p)->align)

#define GET_HEADER(p)           ((Header *) ((char *)p - WSIZE))
#define GET_PREV_FOOTER(p)      ((Header *) ((char *)p - DWSIZE))
#define SET_FOOTER(p, header)   (((Header *) p)->raw = ((Header *) header)->raw)

#define NEXT_BRK(p)             (p + GET_SIZE(GET_HEADER(p)))


/* 
 * mm_init - initialize the malloc package.
 */
// NOTE : 최초 힙 공간 설정
//  prologue, epilogue 생성
int mm_init(void)
{
    // STEP 1 : padding, prologue, epilogue 공간 확보
    mem_sbrk(ALIGNMENT << 1);
    // STEP 2 : padding 생성
    // ?HEAP_BASE가 ALIGNMENT의 배수라 가정
    memset(HEAP_BASE, 0, WSIZE);
    // STEP 3 : prologue 생성
    Header *header = (Header *) ((char *)HEAP_BASE + WSIZE);
        // STEP 3-1 : header
        SET_SIZE(header,ALIGNMENT);
        header->align = 1;
        // STEP 3-2 : footer
        SET_FOOTER((header+1),header);
        HEAD = ++header;  // HEAD 저장
    // STEP 4 : epilogue 생성
    header += 1;
    header->size = 0;
    header->align = 1;
    return 0;
}

// NOTE : 힙 영역 확장
static void *expand_heap(size_t size)
{
    // STEP 1 : 할당 영역 계산
    size_t asize = MAX(size, CHUNK_SIZE);
    asize = (asize % ALIGNMENT) ? ALIGN(asize) : asize;
    void* oldbrk = mem_sbrk(asize);

    // CASE 0 : 할당 불가능일 경우 NULL 리턴
    if(oldbrk == (void *)-1){
        return NULL;
    }

    // STEP 2 : EPILOGUE 이동
    Header *epilogue = GET_HEADER(mem_sbrk(0));
    epilogue->size = 0;
    epilogue->align = 1;
    

    // STEP 3 : 새 HEADER, FOOTER 등록
    Header *header = GET_HEADER(oldbrk);
    SET_SIZE(header,asize);
    header->align = 0;

    SET_FOOTER((epilogue-1), header);

    return oldbrk;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void *find_memory(size_t size)
{
    // STEP 1 : 사이즈에 맞는 블락 찾기
    for(Header * header = GET_HEADER(HEAD); header->size >0; header = (Header *)((char *)header + GET_SIZE(header))){
        if(GET_SIZE(header) >= size && header->align == 0){
            return (void *)header + WSIZE;
        }
    }

    // STEP 2 : 없으면 확장
    return expand_heap(size);
}

void *mm_malloc(size_t size)
{
    // STEP 1 : 할당할 사이즈 계산 = align으로 올림한 size + header + footer
    size_t align = ALIGN(size) + DWSIZE;
    void *mem = find_memory(align);
    if(!mem){
        return NULL;
    }

    // STEP 2 : 분할
    Header* header = (Header *) (mem - WSIZE);
    uintptr_t oldsize = GET_SIZE(header);
    header->align = 1;
    Header *footer;
    // STEP 2 - 1 : 분할
    if(oldsize >= align + DWSIZE + DWSIZE){
        SET_SIZE(header,align);
        footer = (char *)header + align - WSIZE;
        SET_FOOTER(footer,header);
        header = (char *)footer + WSIZE;
        SET_SIZE(header,(oldsize-align));
        header->align = 0;

        footer = (char *)footer + GET_SIZE(header);
        SET_FOOTER(footer,header);

    } else{
        footer = (char *)header + oldsize - WSIZE;
        SET_FOOTER(footer, header);
    }

    return (void *)mem;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    Header * header = GET_HEADER(ptr);
    // STEP 1 : 이전 블럭과 연결
    Header *prev_footer = header -1;
    if(prev_footer->align == 0){
        uintptr_t cur_size = GET_SIZE(header);
        header = (Header * )((char *)header - GET_SIZE(prev_footer));
        SET_SIZE(header,(GET_SIZE(header) + cur_size));
        SET_FOOTER(((char *)header + GET_SIZE(header) - WSIZE), header);
    }
    // STEP 2 : 다음 블럭과 연결
    uintptr_t cur_size = GET_SIZE(header);
    Header *next_header = (Header *)((void*)header + cur_size);
    if(next_header->align == 0){
        cur_size += GET_SIZE(next_header);
        SET_SIZE(header, cur_size);
        SET_FOOTER(((char *)header + cur_size - WSIZE), header);
    }
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
    copySize = header->size;
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

