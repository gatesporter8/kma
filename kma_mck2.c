/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the McKusick-Karels
 *              algorithm
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

#ifdef KMA_MCK2
#define __KMA_IMPL__
#define MAX(a, b) (((a)>(b))?(a):(b))
#define NUMPAGES 1776 //we could allocate another additional bookkeeping page and bump this up

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
/************External Declaration*****************************************/

/**************Implementation***********************************************/

//bookkeeping structure
//list heads
//free page head
//kma_page struct pointers for freeing pages later
//kmemsizes - unused pages form a linked list; used pages split upper half for number of used buffers, lower half for buffer size

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
    freelist[8] = NULL; //size 8192 list head
    //additional page for bookkeeping
    freelist[10] = get_page();
}

inline int get_page_index(void *ptr) {
    kma_page_t **freelist = (root->ptr + sizeof(int));
    return (BASEADDR(ptr) - freelist[10]->ptr) / 8192;
}

inline int get_list_index(kma_size_t size) {
    return 32 - __builtin_clz(size - 1) - 5;
}

inline int size_from_index(int ndx) {
    return 1 << (ndx + 5);
}

void *kma_malloc(kma_size_t size) {
    //return immediately for too large a request
    if ((size + sizeof(kma_page_t * )) > PAGESIZE) return NULL;

    if (root == NULL) init();

    ++(*((int *) root->ptr)); //update used count
    size = MAX(32, size);

    void **freelist = (root->ptr + sizeof(int));
    int ndx = get_list_index(size);

    findFree:
    if (freelist[ndx] != NULL) {
        void **buffer = freelist[ndx];
        freelist[ndx] = buffer[0];
        //update page usage count
        int i = get_page_index(buffer);
        freelist[NUMPAGES + 10 + i] += (1 << 16);
        return buffer;
    }
    int buffer_size = size_from_index(ndx);
    //allocate new page and update data
    kma_page_t *page = get_page();
    int i = get_page_index(page->ptr);
    freelist[10 + i] = page;
    freelist[NUMPAGES + 10 + i] = (int *) (buffer_size);
    //initialize buffer headers
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
    size = MAX(32, size);

    void **buffer = ptr;
    int ndx = get_list_index(size);
    void **freelist = (root->ptr + sizeof(int));
    buffer[0] = freelist[ndx];
    freelist[ndx] = buffer;
    int i = get_page_index(buffer);
    //update page usage count
    freelist[NUMPAGES + 10 + i] -= (1 << 16);
    if (((int) freelist[NUMPAGES + 10 + i]) >> 16 == 0) { // unused page
        //remove buffers from list
        void *page = ((kma_page_t *) freelist[10 + i])->ptr;
        buffer = &freelist[ndx];
        while (buffer != NULL) {
            void **next = *buffer;
            if (BASEADDR(next) == page) {
                *buffer = *next;
                continue;
            }
            buffer = next;
        }
        //free unused page
        free_page(freelist[10 + i]);
        freelist[10 + i] = NULL;
    }

    //update used pages count and release control page if everything free
    if (0 == --(*((int *) root->ptr))) {
        void **freelist = (root->ptr + sizeof(int));
        int i;
        for (i = NUMPAGES - 1; i > -1; --i) {
            if (freelist[10 + i] != NULL)
                free_page(freelist[10 + i]);
        }
        free_page(root);
        root = NULL;
    }
}

#endif // KMA_MCK2
