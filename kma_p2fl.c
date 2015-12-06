/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
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
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 
 ************************************************************************/

/************************************************************************
 Project Group: NetID1, NetID2, NetID3
 
 ***************************************************************************/

#ifdef KMA_P2FL
#define __KMA_IMPL__
#define MAX(a, b) (((a)>(b))?(a):(b))

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

/************Function Prototypes******************************************/
void init();

inline int get_list_index(kma_size_t);

inline int size_from_index(int);

void dummy_free(void *);

void *dummy_alloc();
/************External Declaration*****************************************/

/**************Implementation***********************************************/
void *dummy_alloc() {
    kma_page_t *page = get_page();
    *((kma_page_t **) page->ptr) = page;
    return page->ptr + sizeof(kma_page_t * );
}

void dummy_free(void *ptr) {
    kma_page_t *page = *((kma_page_t **) BASEADDR(ptr));
    free_page(page);
}

void init() {
    //fetch a page and initialize our bookkeeping
    root = get_page();
    *((int *) root->ptr) = 0; //track number of allocated buffers
    void **freelist = (root->ptr + sizeof(int));
    freelist[0] = NULL; //size 32 list head
    freelist[1] = NULL; //size 64 list head
    freelist[2] = NULL; //size 128 list head
    freelist[3] = NULL; //size 256 list head
    freelist[4] = NULL; //size 512 list head
    freelist[5] = NULL; //size 1024 list head
    freelist[6] = NULL; //size 2048 list head
    freelist[7] = NULL; //size 4096 list head
    freelist[8] = &freelist[8]; //pointer to last element of our kma_page array
}

inline int get_list_index(kma_size_t size) {
    return 32 - __builtin_clz(size - 1) - 5;
}

inline int size_from_index(int ndx) {
    return 1 << (ndx + 5);
}

/*test index function
assert(get_list_index(17) == 0);
assert(get_list_index(31) == 0);
assert(get_list_index(32) == 0);
assert(get_list_index(33) == 1);
assert(get_list_index(53) == 1);
assert(get_list_index(63) == 1);
assert(get_list_index(64) == 1);
assert(get_list_index(65) == 2);
assert(get_list_index(126) == 2);
assert(get_list_index(128) == 2);
assert(get_list_index(2059) == 7);
assert(get_list_index(3065) == 7);
assert(get_list_index(4096) == 7);
*/

void *kma_malloc(kma_size_t size) {
    //return immediately for too large a request
    if ((size + sizeof(kma_page_t * )) > PAGESIZE) return NULL;

    if (root == NULL) init();

    ++(*((int *) root->ptr)); //update used count
    size += 4;
    size = MAX(32, size);

    if (size > 4096) return dummy_alloc(); // whole page requests simplify to using the dummy system
    void **freelist = (root->ptr + sizeof(int));
    int ndx = get_list_index(size);

    findFree:
    if (freelist[ndx] != NULL) {
        void **buffer = freelist[ndx];
        freelist[ndx] = buffer[0];
        buffer[0] = &freelist[ndx];
        return &buffer[1];
    }
    //setup a new page and add to page array. each page has the same size buffers
    kma_page_t *page = get_page();
    freelist[8] += sizeof(kma_page_t * );
    //assert(freelist[8] < root->ptr + PAGESIZE);//did not have enough space to store pages
    *((kma_page_t * *)(freelist[8])) = page;
    //initialize buffer headers
    int buffer_size = size_from_index(ndx);
    void *curr_buffer;
    void *last_buff = page->ptr - buffer_size + PAGESIZE;
    for (curr_buffer = page->ptr; curr_buffer < last_buff; curr_buffer += buffer_size) {
        *((void **) curr_buffer) = curr_buffer + buffer_size;
    }
    //insert elements into free list
    *((void **) last_buff) = freelist[ndx];
    freelist[ndx] = page->ptr;

    goto findFree; //recur
}

void kma_free(void *ptr, kma_size_t size) {
    size += 4;
    size = MAX(32, size);

    if (size > 4096)
        dummy_free(ptr);
    else {
        void **buffer = ptr - sizeof(void *);
        void **freelist = buffer[0];
        buffer[0] = *freelist;
        *freelist = buffer;
    }


    //update used pages count and release control page if everything free
    if (0 == --(*((int *) root->ptr))) {
        kma_page_t **freelist = (root->ptr + sizeof(int) + 8 * sizeof(void *));
        kma_page_t **page = *(kma_page_t ***) freelist;
        while (page != freelist) {
            free_page(*page);
            page -= 1;
        }
        free_page(root);
        root = NULL;
    }
}
#endif // KMA_P2FL
