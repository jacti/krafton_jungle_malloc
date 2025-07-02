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

// #define SIZE_T_SIZE     (ALIGN(sizeof(size_t)))

#define CHUNK_SIZE      (4*(1<<10))     // 4KB

#define MAX(a,b)       (a >= b ? a : b)

#define GET_SIZE(p)     (*((uintptr_t *)p) & ~(ALIGNMENT -1 ))

#define GET_ALIGN(p)    (*((uintptr_t *)p) & 1)

#define PUT(p, v)       (*((uintptr_t *)p) = v)

#define PACK(size, align)       (size | align)

#define GET_HEADER(p)     ((uintptr_t *)((char *)p - WSIZE))

#define NXT_BLK(p)      ((uintptr_t *)((char *)p + GET_SIZE((uintptr_t*)p)))

#define PREV_BLK(p)     ((uintptr_t *)((char *)p - GET_SIZE(((uintptr_t*)p - 1))))

#define SEGREGATE_LEN       11

typedef struct  _Header
{
    uintptr_t header;
    struct _Header *prev;
    struct _Header *next;
} FreeHeader;

static FreeHeader* NIL;
static FreeHeader** segregate_list;


static void push(FreeHeader* node);
static void mem_remove(FreeHeader *bp_header);
static uintptr_t *expand_heap(size_t size);
static unsigned short get_index(size_t asize);
static uintptr_t *find_fit(size_t asize);
static size_t split_free_block(FreeHeader *bp_header, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
// NOTE : 최초 힙 공간 설정
int mm_init(void)
{
   // STEP 1 : 공간 시작 align 맞춰 패딩 넣기
   uintptr_t heap_base = mem_heap_lo();
   size_t first_align = ((heap_base + (ALIGNMENT -1)) / ALIGNMENT)*ALIGNMENT - heap_base;
   mem_sbrk(first_align);

   // STEP 2 : segregate_list 생성
   segregate_list = mem_sbrk(sizeof(FreeHeader *) * 11);

   // STEP 3 : 프롤로그, 에필로그 생성
   FreeHeader *prologue = mem_sbrk(sizeof(FreeHeader) + WSIZE);
   prologue->header = PACK((sizeof(FreeHeader) + WSIZE),1);
   NIL = prologue;  //NIL 등록
   PUT((prologue +1),prologue->header); //Footer 등록

   uintptr_t* epilogue = mem_sbrk(WSIZE);
   PUT(epilogue, PACK(0,1));

   // STEP 4 : segregate_list 초기화
   for (int i = 0; i < SEGREGATE_LEN; i++){
    segregate_list[i] = NIL;
   }
   return 0;
}

/*
    NOTE : Free list 앞에 추가하는 함수
*/
static void push(FreeHeader* node){
    if(node == 0x7ffff68ce208){
        int a = 1;
    }
    unsigned short index = get_index(GET_SIZE(node));
    FreeHeader* old_head = segregate_list[index];
    node->prev = NIL;
    node->next = old_head;
    old_head ->prev = node;
    segregate_list[index] = node;
}

/*
    NOTE : Free list에서 제거하는 함수
*/
static void mem_remove(FreeHeader* node){
    FreeHeader* prev = node->prev;
    FreeHeader* next = node->next;
    prev->next = next;
    next->prev = prev;

    // ! 만약 prev 가 NIL이면 Free list의 HEAD이기 때문에 새 HEAD 등록
    if(prev == NIL){
        unsigned short index = get_index(GET_SIZE(node));
        segregate_list[index] = next;
    }
}

// NOTE : 힙 영역 확장
static uintptr_t* expand_heap(size_t size)
{
    // STEP 1 : asize 계산
    //      STEP 1-1 : CHUNKSIZE와 size 중 더 큰거 + align 고려
    size_t asize = MAX(CHUNK_SIZE, size);

    // STEP 2 : heap 확장
    void *old_brk = mem_sbrk(asize);
    // !mem_sbrk가 -1 리턴하면 확장 불가능이므로 NULL 반환
    if(old_brk == (void *)-1){
        return NULL;
    }

    // STEP 3 : 새 epilogue 생성
    uintptr_t* epilogue = (mem_sbrk(0) - WSIZE);
    PUT(epilogue, PACK(0,1));

    // STEP 4 : 이전 block 연결
    uintptr_t* header = GET_HEADER(old_brk);
    uintptr_t* prev_footer = header -1;

    if(GET_ALIGN(prev_footer) == 0){
        header = (uintptr_t *)((char *)header - GET_SIZE(prev_footer));
        //  STEP 4-1 : 이전 블럭을 Free list에서 제거
        if(header == 0x7ffff68ce208){
                    printf("here");
                }
        mem_remove((FreeHeader*)header);
        asize += GET_SIZE(prev_footer);
    }

    // STEP 5 : header, footer 업데이트
    PUT(header,PACK(asize,0));
    PUT((epilogue -1), *header);

    return header;
}

/*
    NOTE : size가 들어갈 index 계산
*/
static unsigned short get_index(size_t asize){
    size_t s = asize;
    s >>= ALIGNBIT + 1;
    unsigned short index = 0;
    
    // 가장 앞 1 비트의 자리 - align
    while(s > 0){
        index +=1;
        s >>= 1;
    }
    
    // 뒤쪽에 나머지가 있으면 index + 1
    if(asize & ((1<< (ALIGNBIT + index))-1)){
        index += 1;
    }
    // 4KB 보다 큰놈들은 젤 마지막 Index
    if(index >= SEGREGATE_LEN) index = SEGREGATE_LEN-1;
    return index;
}


/*
    NOTE : malloc 할 메모리 영역을 찾아서 반환
*/
static uintptr_t *find_fit(size_t asize)
{
    FreeHeader *header;
    // STEP 1 : segregate_list 에서 검색해볼 index 계산
    for(unsigned short index = get_index(asize); index < SEGREGATE_LEN; index ++){
        // STEP 2 : 현재 리스트에서 내 크기를 못찾으면 다음 크기로 넘어감
        header = segregate_list[index];
        // STEP 3 : 현재 segregate_list 안에서 NIL이 나올 때 까지 First fit 탐색
        while(header != NIL){
            if(GET_SIZE(header) >= asize){
                if(header == 0x7ffff68ce208){
                    printf("here");
                }
                mem_remove(header);
                return (uintptr_t *)header;
            }
            header = header->next;
        }
    }

    // 위 리스트에서 못찾았으면 힙 확장
    return expand_heap(asize);
}

static size_t split_free_block(FreeHeader* bp_header, size_t asize)
{
    size_t bsize = GET_SIZE(bp_header);
    if(bsize >= asize + sizeof(FreeHeader) + WSIZE){
        // STEP 1: 이전 헤더 수정
        PUT(bp_header, asize);

        // STEP 2: Footer 추가
        uintptr_t* new_footer = ((char *)bp_header + asize - WSIZE);
        PUT(new_footer,bp_header->header);

        // STEP 3 : 새 분할 블럭의 Header 추가
        FreeHeader * new_free_header = (FreeHeader *)(new_footer + 1);
        new_free_header->header = PACK(bsize - asize,0);

        // STEP 4 : 새 분할 블럭의 Footer 수정
        uintptr_t* footer = ((char *)bp_header + bsize - WSIZE);
        PUT(footer,new_free_header->header);

        // STEP 5 : 새 분할 블럭을 free list에 추가
        if(new_free_header == 0x7ffff68ce208){
            printf("plz here");
        }
        push(new_free_header);
        // 최종 확정된 크기 반환
        return asize;
    }
    else {
        return bsize;
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // STEP 1 : align
    size_t asize = ALIGN(size) + DWSIZE;
    // STEP 2 : find fitted memory
    uintptr_t * header = find_fit(asize);
    // STEP 2-1 : find 하지 못하면 malloc 못하는 상태 return NULL
    if(header == NULL){
        return NULL;
    }
    // STEP 3 : split
    asize = split_free_block((FreeHeader *)header, asize);
    // STEP 4 : alloc
    if (header == 0x7ffff68ce208){
        printf("asdfasdf");
    }
    PUT(header, PACK(asize,1));
    uintptr_t *footer = NXT_BLK(header)-1;
    PUT(footer, *header);
    
    return (void *)(header +1);
}

/*
    NOTE : 앞 뒤의 free block과 연결
*/
static uintptr_t* coalesce(uintptr_t * header){
    uintptr_t *footer = NXT_BLK(header) - 1;
    // CASE 1 : 앞에 블럭이 free block이면
    uintptr_t *prev_header = PREV_BLK(header);
    if(GET_ALIGN(prev_header) == 0){
        if(header == 0x7ffff68ce208){
            printf("here2");
        }
        mem_remove((FreeHeader*)prev_header);
        PUT(prev_header,PACK((GET_SIZE(prev_header)+GET_SIZE(header)),0));
        PUT(footer,*prev_header);
        header = prev_header;
    }

    // CASE 2 : 뒤의 블럭이 free block 이면
    uintptr_t *nxt_header = NXT_BLK(header);
    if(GET_ALIGN(nxt_header) == 0){
        if(header == 0x7ffff68ce208){
                    printf("here3");
                }
        mem_remove((FreeHeader*)nxt_header);
        PUT(header,PACK((GET_SIZE(nxt_header)+GET_SIZE(header)),0));
        uintptr_t* nxt_footer = NXT_BLK(nxt_header) - 1;
        PUT(nxt_footer,*header);
        footer = nxt_footer;
    }

    return header;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // STEP 1 : 앞뒤 연결하기
    uintptr_t * header = coalesce(GET_HEADER(ptr));

    // STEP 2 : align bit 0으로 바꾸기
    *header &= ~1;

    // STEP 3 : segregate_list에 추가하기
    push((FreeHeader *)header);

}

/*
 * mm_realloc - Reallocate a memory block.
 */
void *mm_realloc(void *ptr, size_t size)
{
    // Edge cases
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    uintptr_t *header = GET_HEADER(ptr);
    size_t old_size = GET_SIZE(header);
    size_t asize = ALIGN(size + DWSIZE); // New required block size
    if (asize < 2 * DWSIZE) {
        asize = 2 * DWSIZE;
    }

    // Case 1: New size is smaller or equal. Shrink the block if possible.
    if (old_size >= asize) {
        size_t remainder = old_size - asize;
        if (remainder >= (2 * DWSIZE)) {
            // Shrink and split the block
            PUT(header, PACK(asize, 1));
            PUT((uintptr_t *)((char *)header + asize - WSIZE), PACK(asize, 1));
            
            // Create a new free block with the remainder
            uintptr_t *next_header = (uintptr_t *)((char *)header + asize);
            PUT(next_header, PACK(remainder, 0));
            PUT((uintptr_t *)((char *)next_header + remainder - WSIZE), PACK(remainder, 0));
            
            // Free the new remainder block to coalesce it with the next one if possible
            mm_free((void *)((char *)next_header + WSIZE));
        }
        // If the remainder is too small to be a new free block,
        // do nothing. The block is slightly larger than needed, which is fine.
        return ptr;
    }

    // Case 2: New size is larger. Try to expand in-place by checking the next block.
    uintptr_t *next_header = NXT_BLK(header);
    size_t next_is_free = (GET_ALIGN(next_header) == 0);
    size_t next_size = GET_SIZE(next_header);
    
    // Check if next block is free, not the epilogue, and provides enough space
    if (next_is_free && (old_size + next_size >= asize)) {
        mem_remove((FreeHeader *)next_header); // Remove the next block from free list
        
        size_t total_size = old_size + next_size;
        size_t remainder = total_size - asize;

        if (remainder >= (2 * DWSIZE)) {
            // Expand and split
            PUT(header, PACK(asize, 1));
            PUT((uintptr_t *)((char *)header + asize - WSIZE), PACK(asize, 1));
            
            uintptr_t *new_free_header = (uintptr_t *)((char *)header + asize);
            PUT(new_free_header, PACK(remainder, 0));
            PUT((uintptr_t *)((char *)new_free_header + remainder - WSIZE), PACK(remainder, 0));
            push((FreeHeader *)new_free_header);
        } else {
            // Expand and use the whole merged block
            PUT(header, PACK(total_size, 1));
            PUT((uintptr_t *)((char *)header + total_size - WSIZE), PACK(total_size, 1));
        }
        return ptr; // Data is already in place, just return the original pointer
    }

    // Case 3: Can't expand in-place. Malloc a new block and copy data.
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    // Copy data from the old block to the new block
    size_t copy_size = old_size - DWSIZE; // Old payload size
    if (size < copy_size) {
        copy_size = size; // If new size is smaller, copy less data
    }
    memcpy(new_ptr, ptr, copy_size);
    
    // Free the old block
    mm_free(ptr);
    
    return new_ptr;
}
