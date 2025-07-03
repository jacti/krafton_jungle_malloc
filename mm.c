#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
    "a",
    /* First member's email address */
    "a",
    /* Second member's full name (leave blank if none) */
    "b",
    /* Second member's email address (leave blank if none) */
    "b",
    /* Third member's full name (leave blank if none) */
    "c",
    /* Third member's email address (leave blank if none) */
    "c"
};

/* 기본 매크로 */
#define WSIZE      8                   /* 헤더/푸터 크기 */
#define DSIZE     16                   /* 더블 워드 크기 */
#define ALIGNMENT 16
#define ALIGN(sz) (((sz) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define PACK(sz,al) ((sz) | (al))

#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p,val)   (*(uintptr_t *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0xF)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp)     ((char*)(bp) - WSIZE)
#define FTRP(bp)     ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define CHUNKSIZE  (1<<12)
#define MAX(a,b)   ((a) > (b) ? (a) : (b))

/* 포인터 압축: 상위 32비트는 모두 같으므로 저장 안 함 */
static uintptr_t upper_32;
#define PTR_TO_UINT(p)  ((uint32_t)(uintptr_t)(p))
#define UINT_TO_PTR(u)  ((void*)((uintptr_t)upper_32 | (u)))

typedef enum { RED=0, BLACK=1 } color_t;

/* free-block payload: 16바이트 (32비트 포인터 3개 + color) */
typedef struct {
    uint32_t parent, left, right;
    color_t  color;
} RBNode;

static RBNode *root, *NIL;

/* 내부 함수 프로토타입 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void  place(void *bp, size_t asize);

static void  rb_insert(RBNode *z);
static void  insert_fixup(RBNode *z);
static void  left_rotate(RBNode *x);
static void  right_rotate(RBNode *x);
static void  remove_node(RBNode *z);
static void  rb_transplant(RBNode *u, RBNode *v);
static void  delete_fixup(RBNode *x);

/*-------------------------------------------------------------
 * mm_init
 *    – prologue를 sentinel(NIL)로 쓰고, 초기 힙 확장
 *-------------------------------------------------------------*/
int mm_init(void) {
    /* 충분한 공간 확보: 정렬 패딩 + prologue(32B) + epilogue(8B) */
    if (mem_sbrk(ALIGNMENT + 2*DSIZE + WSIZE) == (void*)-1)
        return -1;

    /* 힙 시작 주소와 보정 */
    char *base = (char*)mem_heap_lo();
    size_t pad = (ALIGNMENT - ((uintptr_t)base % ALIGNMENT)) % ALIGNMENT;
    char *bp = base + pad + WSIZE;  /* bp는 prologue payload 시작 */

    /* 상위 32비트 캐시 */
    upper_32 = ((uintptr_t)bp) & ~0xFFFFFFFFu;

    /* prologue 블록 헤더/푸터 (크기=32, 할당됨) */
    PUT(bp - WSIZE, PACK(2*DSIZE,1));
    PUT(bp + DSIZE,  PACK(2*DSIZE,1));
    /* epilogue 헤더 */
    PUT(bp + 2*DSIZE, PACK(0,1));

    /* NIL(=prologue) 초기화 */
    NIL = (RBNode*)bp;
    NIL->parent = PTR_TO_UINT(NIL);
    NIL->left   = PTR_TO_UINT(NIL);
    NIL->right  = PTR_TO_UINT(NIL);
    NIL->color  = BLACK;
    root = NIL;

    /* 첫 빈 블록 */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*-------------------------------------------------------------
 * extend_heap
 *-------------------------------------------------------------*/
static void *extend_heap(size_t words) {
    size_t size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    char *bp0 = mem_sbrk(size);
    if (bp0 == (void*)-1) return NULL;

    /* 새로 만든 빈 블록 헤더/푸터 + 새로운 epilogue */
    PUT(bp0,               PACK(size,0));
    PUT(bp0 + size - WSIZE, PACK(size,0));
    PUT(bp0 + size,         PACK(0,1));

    return coalesce(bp0 + WSIZE);
}

/*-------------------------------------------------------------
 * coalesce: 인접 빈 블록과 합치고, RB-트리에 삽입
 *-------------------------------------------------------------*/
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC((char*)bp - DSIZE);
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc && !next_alloc) {
        remove_node((RBNode*)NEXT_BLKP(bp));
        remove_node((RBNode*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
                GET_SIZE(HDRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }
    else if (!prev_alloc) {
        remove_node((RBNode*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
    }
    else if (!next_alloc) {
        remove_node((RBNode*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    }

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));

    /* RB-트리에 삽입 */
    RBNode *node = (RBNode*)bp;
    node->parent = PTR_TO_UINT(NIL);
    node->left   = PTR_TO_UINT(NIL);
    node->right  = PTR_TO_UINT(NIL);
    node->color  = RED;
    rb_insert(node);

    return bp;
}

/*-------------------------------------------------------------
 * mm_malloc
 *-------------------------------------------------------------*/
void *mm_malloc(size_t size) {
    if (size == 0) return NULL;
    size_t asize = ALIGN(size + WSIZE);
    RBNode *fit = find_fit(asize);
    if (fit) {
        place(fit, asize);
        return (void*)fit;
    }
    size_t extend = MAX(asize, CHUNKSIZE);
    void *bp = extend_heap(extend/WSIZE);
    if (!bp) return NULL;
    place(bp, asize);
    return bp;
}

/*-------------------------------------------------------------
 * mm_free
 *-------------------------------------------------------------*/
void mm_free(void *ptr) {
    if (!ptr) return;
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),       PACK(size,0));
    PUT(FTRP(ptr),       PACK(size,0));
    RBNode *node = (RBNode*)ptr;
    node->parent = PTR_TO_UINT(NIL);
    node->left   = PTR_TO_UINT(NIL);
    node->right  = PTR_TO_UINT(NIL);
    node->color  = RED;
    rb_insert(node);
    coalesce(ptr);
}

/*-------------------------------------------------------------
 * mm_realloc
 *-------------------------------------------------------------*/
void *mm_realloc(void *ptr, size_t size) {
    if (!ptr) return mm_malloc(size);
    if (size == 0) { mm_free(ptr); return NULL; }
    void *newp = mm_malloc(size);
    if (!newp) return NULL;
    size_t copy = GET_SIZE(HDRP(ptr)) - WSIZE;
    if (size < copy) copy = size;
    memcpy(newp, ptr, copy);
    mm_free(ptr);
    return newp;
}

/*-------------------------------------------------------------
 * find_fit: best-fit 탐색
 *-------------------------------------------------------------*/
static void *find_fit(size_t asize) {
    RBNode *x = root, *best = NIL;
    while (x != NIL) {
        size_t sz = GET_SIZE(HDRP(x));
        if (sz == asize) { best = x; break; }
        if (sz > asize) {
            best = x;
            x = (RBNode*)UINT_TO_PTR(x->left);
        } else {
            x = (RBNode*)UINT_TO_PTR(x->right);
        }
    }
    return best!=NIL ? (void*)best : NULL;
}

/*-------------------------------------------------------------
 * place: 블록 분할 & 할당
 *-------------------------------------------------------------*/
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    remove_node((RBNode*)bp);
    if (csize - asize >= 2*DSIZE) {
        PUT(HDRP(bp),      PACK(asize,1));
        PUT(FTRP(bp),      PACK(asize,1));
        void *next = NEXT_BLKP(bp);
        size_t rsz = csize - asize;
        PUT(HDRP(next),    PACK(rsz,0));
        PUT(FTRP(next),    PACK(rsz,0));
        RBNode *node = (RBNode*)next;
        node->parent = PTR_TO_UINT(NIL);
        node->left   = PTR_TO_UINT(NIL);
        node->right  = PTR_TO_UINT(NIL);
        node->color  = RED;
        rb_insert(node);
    } else {
        PUT(HDRP(bp),      PACK(csize,1));
        PUT(FTRP(bp),      PACK(csize,1));
    }
}

/*-------------------------------------------------------------
 * Red–Black Tree Utilities (brace style and logic cleaned up)
 *-------------------------------------------------------------*/

/* Insert node z into the tree rooted at 'root' */
static void rb_insert(RBNode *z) {
    RBNode *y = NIL;
    RBNode *x = root;

    while (x != NIL) {
        y = x;
        if (GET_SIZE(HDRP(z)) < GET_SIZE(HDRP(x))) {
            x = (RBNode *)UINT_TO_PTR(x->left);
        } else {
            x = (RBNode *)UINT_TO_PTR(x->right);
        }
    }

    z->parent = PTR_TO_UINT(y);
    if (y == NIL) {
        root = z;
    } else if (GET_SIZE(HDRP(z)) < GET_SIZE(HDRP(y))) {
        y->left = PTR_TO_UINT(z);
    } else {
        y->right = PTR_TO_UINT(z);
    }

    z->left  = PTR_TO_UINT(NIL);
    z->right = PTR_TO_UINT(NIL);
    z->color = RED;

    insert_fixup(z);
}

/* Restore red–black properties after insert */
static void insert_fixup(RBNode *z) {
    while ( ((RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(z->parent))->parent))->color == RED ) {
        RBNode *p = (RBNode *)UINT_TO_PTR(z->parent);
        RBNode *g = (RBNode *)UINT_TO_PTR(p->parent);

        if (p == (RBNode *)UINT_TO_PTR(g->left)) {
            RBNode *u = (RBNode *)UINT_TO_PTR(g->right);
            if (u->color == RED) {
                p->color = BLACK;
                u->color = BLACK;
                g->color = RED;
                z = g;
            } else {
                if (z == (RBNode *)UINT_TO_PTR(p->right)) {
                    z = p;
                    left_rotate(z);
                    p = (RBNode *)UINT_TO_PTR(z->parent);
                }
                p->color = BLACK;
                g->color = RED;
                right_rotate(g);
            }
        } else {
            RBNode *u = (RBNode *)UINT_TO_PTR(g->left);
            if (u->color == RED) {
                p->color = BLACK;
                u->color = BLACK;
                g->color = RED;
                z = g;
            } else {
                if (z == (RBNode *)UINT_TO_PTR(p->left)) {
                    z = p;
                    right_rotate(z);
                    p = (RBNode *)UINT_TO_PTR(z->parent);
                }
                p->color = BLACK;
                g->color = RED;
                left_rotate(g);
            }
        }
    }
    root->color = BLACK;
}

/* Rotate subtree left around node x */
static void left_rotate(RBNode *x) {
    RBNode *y = (RBNode *)UINT_TO_PTR(x->right);

    /* Turn y's left subtree into x's right subtree */
    x->right = y->left;
    if ((RBNode *)UINT_TO_PTR(y->left) != NIL) {
        ((RBNode *)UINT_TO_PTR(y->left))->parent = PTR_TO_UINT(x);
    }

    /* Link x's parent to y */
    y->parent = x->parent;
    if ((RBNode *)UINT_TO_PTR(x->parent) == NIL) {
        root = y;
    } else if (x == (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->left)) {
        ((RBNode *)UINT_TO_PTR(x->parent))->left = PTR_TO_UINT(y);
    } else {
        ((RBNode *)UINT_TO_PTR(x->parent))->right = PTR_TO_UINT(y);
    }

    /* Put x on y's left */
    y->left = PTR_TO_UINT(x);
    x->parent = PTR_TO_UINT(y);
}

/* Rotate subtree right around node y */
static void right_rotate(RBNode *y) {
    RBNode *x = (RBNode *)UINT_TO_PTR(y->left);

    /* Turn x's right subtree into y's left subtree */
    y->left = x->right;
    if ((RBNode *)UINT_TO_PTR(x->right) != NIL) {
        ((RBNode *)UINT_TO_PTR(x->right))->parent = PTR_TO_UINT(y);
    }

    /* Link y's parent to x */
    x->parent = y->parent;
    if ((RBNode *)UINT_TO_PTR(y->parent) == NIL) {
        root = x;
    } else if (y == (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(y->parent))->right)) {
        ((RBNode *)UINT_TO_PTR(y->parent))->right = PTR_TO_UINT(x);
    } else {
        ((RBNode *)UINT_TO_PTR(y->parent))->left = PTR_TO_UINT(x);
    }

    /* Put y on x's right */
    x->right = PTR_TO_UINT(y);
    y->parent = PTR_TO_UINT(x);
}

/* Replace subtree rooted at u with subtree rooted at v */
static void rb_transplant(RBNode *u, RBNode *v) {
    if ((RBNode *)UINT_TO_PTR(u->parent) == NIL) {
        root = v;
    } else if (u == (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(u->parent))->left)) {
        ((RBNode *)UINT_TO_PTR(u->parent))->left = PTR_TO_UINT(v);
    } else {
        ((RBNode *)UINT_TO_PTR(u->parent))->right = PTR_TO_UINT(v);
    }
    v->parent = u->parent;
}

/* Remove node z from the tree */
static void remove_node(RBNode *z) {
    RBNode *y = z;
    RBNode *x;
    color_t y_original_color = y->color;

    if ((RBNode *)UINT_TO_PTR(z->left) == NIL) {
        x = (RBNode *)UINT_TO_PTR(z->right);
        rb_transplant(z, x);
    } else if ((RBNode *)UINT_TO_PTR(z->right) == NIL) {
        x = (RBNode *)UINT_TO_PTR(z->left);
        rb_transplant(z, x);
    } else {
        /* Find z's successor y */
        y = (RBNode *)UINT_TO_PTR(z->right);
        while ((RBNode *)UINT_TO_PTR(y->left) != NIL) {
            y = (RBNode *)UINT_TO_PTR(y->left);
        }
        y_original_color = y->color;
        x = (RBNode *)UINT_TO_PTR(y->right);

        if ((RBNode *)UINT_TO_PTR(y->parent) == z) {
            x->parent = PTR_TO_UINT(y);
        } else {
            rb_transplant(y, x);
            y->right = z->right;
            ((RBNode *)UINT_TO_PTR(y->right))->parent = PTR_TO_UINT(y);
        }

        rb_transplant(z, y);
        y->left = z->left;
        ((RBNode *)UINT_TO_PTR(y->left))->parent = PTR_TO_UINT(y);
        y->color = z->color;
    }

    if (y_original_color == BLACK) {
        delete_fixup(x);
    }
}

/* Restore red–black properties after delete */
static void delete_fixup(RBNode *x) {
    while (x != root && x->color == BLACK) {
        if (x == (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->left)) {
            RBNode *w = (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->right);
            if (w->color == RED) {
                w->color = BLACK;
                ((RBNode *)UINT_TO_PTR(x->parent))->color = RED;
                left_rotate((RBNode *)UINT_TO_PTR(x->parent));
                w = (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->right);
            }
            if (((RBNode *)UINT_TO_PTR(w->left))->color == BLACK &&
                ((RBNode *)UINT_TO_PTR(w->right))->color == BLACK) {
                w->color = RED;
                x = (RBNode *)UINT_TO_PTR(x->parent);
            } else {
                if (((RBNode *)UINT_TO_PTR(w->right))->color == BLACK) {
                    ((RBNode *)UINT_TO_PTR(w->left))->color = BLACK;
                    w->color = RED;
                    right_rotate(w);
                    w = (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->right);
                }
                w->color = ((RBNode *)UINT_TO_PTR(x->parent))->color;
                ((RBNode *)UINT_TO_PTR(x->parent))->color = BLACK;
                ((RBNode *)UINT_TO_PTR(w->right))->color = BLACK;
                left_rotate((RBNode *)UINT_TO_PTR(x->parent));
                x = root;
            }
        } else {
            /* mirror case */
            RBNode *w = (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->left);
            if (w->color == RED) {
                w->color = BLACK;
                ((RBNode *)UINT_TO_PTR(x->parent))->color = RED;
                right_rotate((RBNode *)UINT_TO_PTR(x->parent));
                w = (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->left);
            }
            if (((RBNode *)UINT_TO_PTR(w->right))->color == BLACK &&
                ((RBNode *)UINT_TO_PTR(w->left))->color == BLACK) {
                w->color = RED;
                x = (RBNode *)UINT_TO_PTR(x->parent);
            } else {
                if (((RBNode *)UINT_TO_PTR(w->left))->color == BLACK) {
                    ((RBNode *)UINT_TO_PTR(w->right))->color = BLACK;
                    w->color = RED;
                    left_rotate(w);
                    w = (RBNode *)UINT_TO_PTR(((RBNode *)UINT_TO_PTR(x->parent))->left);
                }
                w->color = ((RBNode *)UINT_TO_PTR(x->parent))->color;
                ((RBNode *)UINT_TO_PTR(x->parent))->color = BLACK;
                ((RBNode *)UINT_TO_PTR(w->left))->color = BLACK;
                right_rotate((RBNode *)UINT_TO_PTR(x->parent));
                x = root;
            }
        }
    }
    x->color = BLACK;
}
