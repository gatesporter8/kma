/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *

 ************************************************************************/
 
 /************************************************************************
 Project Group: NetID1, NetID2, NetID3
 
 ***************************************************************************/

#ifdef KMA_BUD
#define __KMA_IMPL__
#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define NUMPAGES 1500

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
/************Global Variables*********************************************/
kma_page_t *root = NULL;

typedef struct free_list {
    struct free_list *next;
    struct free_list *prev;
    int list_ndx;
} free_list;

/************Function Prototypes******************************************/
void init();

inline int get_list_index(kma_size_t);

inline int size_from_index(int);

inline void *buddy_addr(void *, int);

//could consolidate these two to toggle
inline void set_bitmask(void *);

inline void unset_bitmask(void *);

inline int check_bitmask(void *);

inline void split_block(void *, int, int);

inline void *merge_block(void *, int *);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void init() {
    //fetch a page and initialize our bookkeeping
    root = get_page();
    *((int *) root->ptr) = 0; //track number of allocated buffers
    kma_page_t **freelist = (root->ptr + sizeof(int));
    freelist[0] = NULL; //size 32 list head
    freelist[1] = NULL; //size 64 list head
    freelist[2] = NULL; //size 128 list head
    freelist[3] = NULL; //size 256 list head
    freelist[4] = NULL; //size 512 list head
    freelist[5] = NULL; //size 1024 list head
    freelist[6] = NULL; //size 2048 list head
    freelist[7] = NULL; //size 4096 list head
    freelist[9] = 0; //this will be used as slack for lazy buddy

    //additional pages for bookkeeping
    freelist[10] = get_page();
    freelist[11] = get_page();
    freelist[12] = get_page();
    freelist[13] = get_page();
    freelist[14] = get_page();
    freelist[15] = get_page();
}

inline int get_page_index(void *ptr) {
    kma_page_t **freelist = (root->ptr + sizeof(int));
    return (BASEADDR(ptr) - freelist[15]->ptr) / 8192;
}

inline int get_list_index(kma_size_t size) {
    return 32 - __builtin_clz(size - 1) - 5;
}

inline int size_from_index(int ndx) {
    return 1 << (ndx + 5);
}

inline void *buddy_addr(void *orig, int size) {
    //assert(__builtin_parity(size) == 1); //ensure we use powers of 2
    return (void *) (((int) orig) ^ size);
}


inline void set_bitmask(void *block) {
    int pg_ndx = get_page_index(block);
    int pg_blk_num = (((int) block) & 8191) / 32;
    int index_offset = pg_blk_num / 32;
    int mask = 1 << (pg_blk_num % 32);

    int *freelist = (root->ptr + sizeof(int));
    freelist[10 + NUMPAGES + index_offset + (8 * pg_ndx)] |= mask;
}

inline void unset_bitmask(void *block) {
    int pg_ndx = get_page_index(block);
    int pg_blk_num = (((int) block) & 8191) / 32;
    int index_offset = pg_blk_num / 32;
    int mask = 1 << (pg_blk_num % 32);

    int *freelist = (root->ptr + sizeof(int));
    freelist[10 + NUMPAGES + index_offset + (8 * pg_ndx)] &= ~mask;
}

inline int check_bitmask(void *block) {
    int pg_ndx = get_page_index(block);
    int pg_blk_num = (((int) block) & 8191) / 32;
    int index_offset = pg_blk_num / 32;
    int mask = 1 << (pg_blk_num % 32);

    int *freelist = (root->ptr + sizeof(int));
    //0 if bit is unset, else some non-zero value
    return freelist[10 + NUMPAGES + index_offset + (8 * pg_ndx)] & mask;
}


inline void split_block(void *block, int curr_ndx, int target_ndx) {
    if (curr_ndx == target_ndx) return; //base case - block split to target size
    --curr_ndx;
    int size = size_from_index(curr_ndx);
    free_list *buddy = buddy_addr(block, size);

    //add upper half of block to corresponding free list and recur
    void **freelist = (root->ptr + sizeof(int));
    buddy->next = freelist[curr_ndx];
    buddy->prev = NULL;
    buddy->list_ndx = curr_ndx;
    if (buddy->next != NULL) {
        buddy->next->prev = buddy;
    }
    freelist[curr_ndx] = buddy;

    split_block(block, curr_ndx, target_ndx);
}

inline void *merge_block(void *block, int *ndx) {
    //base case - full page block
    if (*ndx == 8) {
        return NULL;
    }
    int size = size_from_index(*ndx);
    free_list *buddy = buddy_addr(block, size);
    if (check_bitmask(buddy) != 0 || buddy->list_ndx != *ndx) { //buddy is occupied or further split up
        return block;
    }
    //remove from free list and merge with buddy
    if (buddy->next != NULL) {
        buddy->next->prev = buddy->prev;
    }

    if (buddy->prev != NULL) {
        buddy->prev->next = buddy->next;
    }
    else {
        void **freelist = (root->ptr + sizeof(int));
        freelist[*ndx] = buddy->next;
    }

    ++(*ndx);
    return merge_block((void *) (((int) block) & ((int) buddy)), ndx); //recur
}


void *kma_malloc(kma_size_t size) {
    //return immediately for too large a request
    if ((size + sizeof(kma_page_t * )) > PAGESIZE) return NULL;

    if (root == NULL) init();

    ++(*((int *) root->ptr)); //update used count
    size = MAX(32, size);

    void **freelist = (root->ptr + sizeof(int));
    int ndx = get_list_index(size);

    //remove smallest available block from its list, update bitmap, split to desired size
    int i;
    for (i = ndx; i < 8; i++) {
        if (freelist[i] != NULL) {
            free_list *buffer = freelist[i];
            //assert(check_bitmask(buffer) == 0);
            //assert(((size_from_index(ndx) - 1) & ((int) buffer)) == 0);
            if (buffer->next != NULL) {
                buffer->next->prev = buffer->prev;
            }
            freelist[i] = buffer->next;
            set_bitmask(buffer);
            split_block(buffer, i, ndx);
            return buffer;
        }
    }
    //allocate new page and split
    kma_page_t *page = get_page();
    void *buffer = page->ptr;
    int pg_ndx = get_page_index(buffer);
    freelist[15 + pg_ndx] = page;
    set_bitmask(buffer);
    split_block(buffer, 8, ndx);

    return buffer;
}

void kma_free(void *ptr, kma_size_t size) {
    size = MAX(32, size);
    void **freelist = (root->ptr + sizeof(int));

    //unset bitmap and merge to largest possible size
    unset_bitmask(ptr);

    int ndx = get_list_index(size);
    free_list *buffer = merge_block(ptr, &ndx);
    if (buffer == NULL) { // unused page
        int pg_ndx = get_page_index(ptr);

        //free up bitmap
        freelist[10 + NUMPAGES + (8 * pg_ndx)] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 1] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 2] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 3] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 4] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 5] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 6] = 0;
        freelist[10 + NUMPAGES + (8 * pg_ndx) + 7] = 0;
        free_page(freelist[15 + pg_ndx]);
        freelist[15 + pg_ndx] = NULL;
    }
    else {
        buffer->next = freelist[ndx];
        buffer->prev = NULL;
        buffer->list_ndx = ndx;
        if (buffer->next != NULL) {
            buffer->next->prev = buffer;
        }
        freelist[ndx] = buffer;
    }

    //update used pages count and release control page if everything free
    if (0 == --(*((int *) root->ptr))) {
        int i;
        for (i = NUMPAGES - 1; i > -1; --i) {
            if (freelist[10 + i] != NULL)
                free_page(freelist[10 + i]);
        }
        free_page(root);
        root = NULL;
    }
}

#endif // KMA_BUD
