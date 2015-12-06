/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
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

#ifdef KMA_RM
#define __KMA_IMPL__
#define MAX(a, b) (((a)>(b))?(a):(b))
#define LISTHEAD *((free_list_t **) root->ptr)
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
typedef struct freeList {
    int size;
    struct freeList *prev;
    struct freeList *next;
} free_list_t;

kma_page_t *root = NULL;

/************Function Prototypes******************************************/
void remove_node(free_list_t *);

free_list_t *insert_node(free_list_t *, int);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
void init() {
    //fetch a page and initialize our free list. root->ptr is the head pointer of our list
    root = get_page();
    free_list_t *head = root->ptr + sizeof(free_list_t *) + sizeof(int);
    LISTHEAD = head;
    *((int *) (root->ptr + sizeof(free_list_t *))) = 0;

    head->size = PAGESIZE - sizeof(free_list_t *) - sizeof(int);
    head->prev = NULL;
    head->next = NULL;
}

//check list sorted sanity
void checkList() {
    free_list_t *buf = LISTHEAD;
    while (buf) {
        assert(buf->prev == NULL || buf->prev < buf);
        assert(buf->next == NULL || buf->next > buf);
        buf = buf->next;
    }
}

void * kma_malloc(kma_size_t size) {
    size = MAX(sizeof(free_list_t), size); //min request size must fit our list structure
    //return immediately for too large a request
    if ((size + sizeof(kma_page_t * )) > PAGESIZE) return NULL;

    if (root == NULL) init();

    ++(*((int *) (root->ptr + sizeof(free_list_t *)))); //update used count
    free_list_t *buf;

    findFree:
    buf = LISTHEAD;
    while (buf) {
        if (buf->size == size) {
            remove_node(buf);
            return buf;
        }
        else if (buf->size >= size + sizeof(free_list_t)) {
            buf->size = buf->size - size;//resize free portion and return tail chunk
            free_list_t *new_buf = ((void *) buf) + buf->size;
            return new_buf;
        }
        buf = buf->next;
    }

    //add new page to list
    kma_page_t *page = get_page();
    *((kma_page_t **) page->ptr) = page;
    buf = page->ptr + sizeof(kma_page_t * );
    insert_node(buf, PAGESIZE - sizeof(kma_page_t * ));

    goto findFree; //recur
}

void remove_node(free_list_t *node) {
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    else {
        LISTHEAD = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
}

free_list_t *insert_node(free_list_t *addr, int size) {
    addr->size = size;

    if (LISTHEAD == NULL) {
        LISTHEAD = addr;
        addr->next = NULL;
        addr->prev = NULL;
    }
    else {
        free_list_t *node = LISTHEAD;
        //add to front of list
        if (addr < node) {
            LISTHEAD = addr;
            addr->next = node;
            addr->prev = NULL;
            node->prev = addr;
        }
        else {
            //add in sorted order
            while (node->next != NULL && node->next < addr) {
                node = node->next;
            }
            addr->prev = node;
            addr->next = node->next;
            node->next = addr;
            if (addr->next != NULL) {
                addr->next->prev = addr;
            }
        }
    }
    return addr;
}

void kma_free(void *ptr, kma_size_t size) {
    size = MAX(sizeof(free_list_t), size);
    //add node to free list and coalese
    free_list_t *node = insert_node(ptr, size);
    bool co_right = (void *) node + node->size == node->next;
    bool co_left = node->prev && (void *) node->prev + node->prev->size == node;
    if (co_right) {
        node->size += node->next->size;
        remove_node(node->next);
    }
    if (co_left) {
        node = node->prev;
        node->size += node->next->size;
        remove_node(node->next);
    }
    //free up unused page
    if (node->size == PAGESIZE - sizeof(kma_page_t * )) {
        remove_node(node);
        free_page(*((kma_page_t **) BASEADDR(ptr)));
    }
    //update used pages count and release control page if everything free
    if (0 == --(*((int *) (root->ptr + sizeof(free_list_t *))))) {
        free_page(root);
        root = NULL;
    }
}

#endif // KMA_RM