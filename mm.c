/* 
 * Simple allocator based on implicit free lists with boundary 
 * tag coalescing. Each block has header and footer of the form:
 * 
 *      31                     3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap  
 *  -----------------------------------------------------------------   
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include "mm.h"
#include "memlib.h"

team_t team = {"jepsin11mdemali","James Espinosa", "jespin11","Matt Demali","mdemali"}; /* so we're compatible with 15213 driver */

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)   /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Additional constants and macros */
#define PADDING    4
#define PROLOGSIZE 16
#define EPILOGSIZE 8
#define RIGHT(bp) ((void *)* (int *)(bp+WSIZE))
#define LEFT(bp) ((void *)* (int *)(bp))
#define SETLEFT(bp, bq) (*(int *)(bp)) = (int)(bq)
#define ADJUSTSIZE(size) ((((size) + DSIZE + 7) / DSIZE ) * DSIZE)
#define SETRIGHT(bp, bq) (*(int *)(bp+WSIZE)) = (int)(bq)
#define GETSIZE(bp) ((*(int*) (bp-WSIZE)) & ~7)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))  //store val where p is pointing to

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)  
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* $end mallocmacros */

/* The only global variable is a pointer to the first block */
static char *heap_listp;
static void *tree_root;

/* function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void *place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

/* Additional function declarations */
void *mm_insert(void *rt, void *bp);
void *mm_remove(void *rt, void *bp);
void *mm_ceiling(void *rt,  int size);
void *mm_parent(void *rt, void *bp);
void *mm_replace(void *bp);
void *removeFruitless(void *rt, void *bp);
void *removeOnlyChild(void *rt, void *bp);
void *remove2Kids(void *rt, void *bp);
int countChildren(void *rt);

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
    void *bp;

    tree_root = NULL;

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(PROLOGSIZE)) == NULL)
        return -1;

    PUT(heap_listp, 0);                        /* alignment padding */
    PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1));  /* prologue header */ 
    PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1));  /* prologue footer */ 
    PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1));   /* epilogue header */
    heap_listp += DSIZE;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    bp = extend_heap(CHUNKSIZE/WSIZE);

    if (bp == NULL)
        return -1;

    tree_root = mm_insert(tree_root, bp);

    return 0;
}
/* $end mminit */

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size) 
{
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = DSIZE + OVERHEAD;
    else
        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = mm_ceiling(tree_root,asize)) != NULL) 
    {
        tree_root = mm_remove(tree_root,bp);
        bp = place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);

    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    bp = place(bp, asize);

    return bp;
} 
/* $end mmmalloc */

/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    tree_root = mm_insert(tree_root,coalesce(bp));
}

/* $end mmfree */

/* Not implemented. For consistency with 15-213 malloc driver */
void *mm_realloc(void *ptr, size_t size)
{   

    void *bp;
    size_t asize = ADJUSTSIZE(size);

    if(!GETSIZE(NEXT_BLKP(ptr)))
    {
        size_t extendsize = MAX(asize, CHUNKSIZE); 
        bp = extend_heap(extendsize/4);
        size_t nsize = extendsize + GETSIZE(ptr) - asize;       
        
        //update header and footer
        PUT(HDRP(ptr), PACK(asize,1));
        PUT(FTRP(ptr), PACK(asize,1));
        
        //Split off extra blocks
        void *nBlock = NEXT_BLKP(ptr);
        PUT(HDRP(nBlock), PACK(nsize,0));
        //nBlock = nBlock + WSIZE;
        PUT(FTRP(nBlock), PACK(nsize, 0));
        tree_root = mm_insert(tree_root, nBlock);
        
        return ptr;     
    }
    
    if(!(GET_ALLOC(HDRP(NEXT_BLKP(ptr)))))
    {
        bp = NEXT_BLKP(ptr);
        
        size_t total = GETSIZE(ptr) + GETSIZE(bp);
            
        if(total >= asize)
        {
        
            size_t nsize = total - asize;
            tree_root = mm_remove(tree_root,bp);
            
            if(nsize < 16)
            {
                PUT(HDRP(ptr), PACK(total, 1));
                PUT(FTRP(ptr), PACK(total, 1));
                return ptr;
            }
            else 
            {
                PUT(HDRP(ptr), PACK(asize, 1));
                PUT(FTRP(ptr), PACK(asize, 1));
                
                void *nBlock = NEXT_BLKP(ptr);
                PUT(HDRP(nBlock), PACK(nsize,0));
                PUT(FTRP(nBlock), PACK(nsize,0));
                tree_root = mm_insert(tree_root, nBlock);
                
                return ptr;
            }                                    
        }
        
        else if(!GETSIZE(NEXT_BLKP(bp)))
        {
            size_t extendsize = MAX(asize, CHUNKSIZE);
            extend_heap(extendsize/4);
            size_t nsize = extendsize + total - asize;
            
            PUT(HDRP(ptr), PACK(asize,1));
            PUT(FTRP(ptr), PACK(asize,1));
            
            void *nBlock = NEXT_BLKP(ptr);
            PUT(HDRP(nBlock), PACK(nsize,0));
            PUT(FTRP(nBlock), PACK(nsize,0));
            tree_root = mm_insert(tree_root, nBlock);
            return ptr;
        }                                                       
    }
    
    bp = mm_malloc(size);
    
    memcpy(bp, ptr, (GETSIZE(ptr) - DSIZE));
    mm_free(ptr);
    return bp;  
}

/* 
 * mm_checkheap - Check the heap for consistency 
 */
void mm_checkheap(int verbose) 
{
    char *bp = heap_listp;

    if (verbose) {
        printf("Heap (%p):\n", heap_listp);
        printf("Root (%p):\n", tree_root);
    }

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose)
            printblock(bp);

        checkblock(bp);
    }
     
    if (verbose)
        printblock(bp);

    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

/* The remaining routines are internal helper routines */

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((int)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}
/* $end mmextendheap */

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void *place(void *bp, size_t asize)
/* $end mmplace-proto */
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t split_size = (csize - asize);

    if (split_size >= (DSIZE + OVERHEAD)) {
        size_t avg = (GETSIZE(NEXT_BLKP(bp)) + GETSIZE(PREV_BLKP(bp)))/2; 
        void* large;
        void* small;
        int side; // 0: split off end | 1: Split off front
        
        if(GETSIZE(NEXT_BLKP(bp)) > GETSIZE(PREV_BLKP(bp)))
        {
            large = NEXT_BLKP(bp);
            small = PREV_BLKP(bp);
        }
        else
        {
            large = PREV_BLKP(bp);
            small = NEXT_BLKP(bp);
        }
         
        //check if we should split off of the front or back
        if(asize > avg)
        {
            if(PREV_BLKP(bp) == large)
                side = 0;
            else 
                side = 1;           
        }
        else
        {
            if(PREV_BLKP(bp) == large)
                side = 1;
            else 
                side = 0;
        }
        
        //split depending on case
        if(side == 0)
        {
            //split off the end
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            void* split = NEXT_BLKP(bp);
            PUT(HDRP(split), PACK(csize-asize, 0));
            PUT(FTRP(split), PACK(csize-asize, 0));
            tree_root = mm_insert(tree_root,split);
            return bp;
        }
        else
        {
            //split off the front
            PUT(HDRP(bp), PACK(split_size,0));
            PUT(FTRP(bp), PACK(split_size,0));
        
            void *aBlock = NEXT_BLKP(bp);
            PUT(HDRP(aBlock), PACK(asize, 1));
            PUT(FTRP(aBlock), PACK(asize, 1));
            tree_root = mm_insert(tree_root,bp);
            return aBlock;
        }
    }
    else
    { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        return bp;
    }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit-proto */
{
    void *bp;

    /* first fit search */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* no fit */
}
/* $end mmfirstfit */

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1: Neighbors both allocated */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2: Only the previous is allocated*/
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        /* If only the previous block is allocated, remove the next block */
        tree_root = mm_remove(tree_root, NEXT_BLKP(bp));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));

        return(bp);
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3: Only the next is allocated */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        /* If only the next block is allocated, remove the previous block */
        tree_root = mm_remove(tree_root, PREV_BLKP(bp));

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        return(PREV_BLKP(bp));
    }

    else {                                     /* Case 4: Neither are allocated */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));

        /* If neither blocks are allocated, remove them both */
        tree_root = mm_remove(tree_root, NEXT_BLKP(bp));
        tree_root = mm_remove(tree_root, PREV_BLKP(bp));

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        return(PREV_BLKP(bp));
    }
}
/* $end mmfree */

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
    return;
    }

    if (bp == heap_listp) {
      printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f')); 

    } else if (!halloc) {
      printf("%p: header: [%d:%c] | left: %p, right: %p | footer: [%d:%c]\n", bp, hsize, (halloc ? 'a' : 'f'),
         LEFT(bp), RIGHT(bp), fsize, (falloc ? 'a' : 'f')); 

    } else {
      printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f')); 
    }
  
}

static void checkblock(void *bp) 
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

/*
*Inserts a free block of memory into the Binary Tree and returns a pointer 
*to the new root of the tree
*
*@param rt  Pointer to the root of a tree or subtree
*@param bp  Pointer to the node to be inserted into the tree
*@return    Pointer to the new root of the tree
*/
void *mm_insert(void *rt, void* bp)
{
    //if the tree is empty set the leaves of the node to NULL and return
    if(rt == NULL)
    {
        SETLEFT(bp, NULL);
        SETRIGHT(bp, NULL);
        return bp;
    }
    //if the new node bp is smaller or equal to the current node rt insert it into the left subtree
    else if(GETSIZE(bp) <= GETSIZE(rt))
    {
        SETLEFT(rt, mm_insert(LEFT(rt),bp));
        return rt;
    }
    //if the new node bp is larger than the current node rt insert rt into the right subtree
    else if(GETSIZE(bp) >  GETSIZE(rt))
    {
        SETRIGHT(rt, mm_insert(RIGHT(rt),bp));
        return rt;
    }
    
    /* If there's an error, return -1 */
    return -1;
}

/*
*Removes the given node from the tree and returns a pointer to the 
*new root
*
*@param rt  Pointer to the root of a tree or subtree
*@param bp  Pointer to the node to be removed from the tree
*@return    Pointer to the new root of the tree, returns NULL if tree is empty
*/

void *mm_remove(void *rt, void *bp)
{
    if(countChildren(bp) == 0)  //If there are no children call removeFruitless
        return removeFruitless(rt, bp);
    else if(countChildren(bp) == 1) //If there is one child call removeOnlyChild
        return removeOnlyChild(rt, bp);
    else // else call remove2Kids
        return remove2Kids(rt, bp);
}

/*
*Finds a pointer to the node that is the best fit for a given size
*A best fit is the smallest node that is bigger than the size or that is the same size
*
*@param rt  Pointer to the root of a tree or subtree
*@param bp  Size that we are looking for
*@return    Pointer to the node that is the best size, returns NULL if tree is empty
*/
void * mm_ceiling(void* rt, int size)
{
    void* candidate; //stores the current best fit that has been found
    
    //checks if the node is NULL, or empty and return NULL
    if(rt == NULL)
        return NULL;
        
    int rtSize = GETSIZE(rt); //the size of the current node or the root
    
    if(rtSize  ==  size)//check if the current node is a perfect fit
        return rt;
    else if(rtSize > size)// if the node is bigger check the left and store it in candidate
        candidate = mm_ceiling(LEFT(rt), size);
    else if (rtSize < size)// if node is small check the right and store it in candidate
        candidate = mm_ceiling(RIGHT(rt), size);
    
    //if child is NULL check if it can fit in current Node, if it is not return NULL
    if(candidate == NULL)
    {
        //is current node too small return NULL
        if(rtSize < size)
            return NULL;
        else 
            return rt; //return the last best fit
    }
    //return the best fit
    else 
        return candidate; // if it is not NULL just return
                    
}


/* Tree Helper Functions */

/*
*Get the Parent of a given node bp
*
*@param rt  Pointer to the current tree or subtree being checked
*@param bp  Pointer to the node that is looking for its parent
*@return    Pointer to the parent of the given node bp, returns NULL if bp us the root
*/
void *mm_parent(void *rt, void *bp)
{
    if(bp == rt) //if the node is the root return NULL
        return NULL;
    if(GETSIZE(bp) <= GETSIZE(rt)) //if the size of bp is less than or equal to the size of the current node rt 
    {
        if(LEFT(rt) == bp)//check if the child on the left is bp, if it is return the current node
            return rt;
        else //if the left child is not bp check its children
            return mm_parent(LEFT(rt),bp);
    }
    else //if the size of bp is great than the size of the current node rt 
    {
        if(RIGHT(rt) == bp) // check if the right child is bp, if it is return the current node
            return rt;
        else //if the right child is not bp check the children of the right child
            return mm_parent(RIGHT(rt),bp);
    }
} 

/*
*Counts the number of children a given node rt has
*
*@param rt  Pointer to the node to have its children counted
*@return    Returns the number of children a node has (1,2, or 3)
*/
int countChildren(void *rt)
{
    int count = 0;// counter startting at 0
    if(LEFT(rt) != NULL)//if there is a child on the left increase the counter
         count++;
    if(RIGHT(rt) != NULL)//if there is a child on the right increase the counter
         count++;
                            
    return count;// return the final count
}         

/*
*Removes the given node bp from the tree
*bp must have no children
*
*@param rt  Pointer to root of the tree or subtree
*@param bp  Pointer to the node to be removed
*@return    returns the pointer to the new root of the tree
*/
void *removeFruitless(void *rt, void *bp)
{
    void *pt = mm_parent(rt, bp); //stores the parent of the node bp
    
    if(pt != NULL)//if the parent is not NULL 
    {
        //check which side the node bp is on of the parent then have it point to NULL
        if(LEFT(pt) == bp) 
            SETLEFT(pt, NULL);
        else 
            SETRIGHT(pt, NULL);
        return rt;
    } 
    else //if it is the root set the root to NULL and return NULL 
    {
        return NULL;
    }
}

/*
*Removes the given node bp from the tree
*bp must have exactly 1 child
*
*@param rt  Pointer to root of the tree or subtree
*@param bp  Pointer to the node to be removed
*@return    returns the pointer to the new root of the tree
*/
void *removeOnlyChild(void *rt, void* bp)
{
    void *child;//store the child of bp
    
    //gets the child of bp
    if(LEFT(bp) != NULL)
        child = LEFT(bp);
    else
        child = RIGHT(bp);
                
    void *pt = mm_parent(rt, bp); //get the parent of bp

    //if bp is not the root replace its child with the child of its child
    if(pt != NULL)
    {
        if(LEFT(pt) == bp)
            SETLEFT(pt, child);
        else 
            SETRIGHT(pt, child); 
        return rt;
    }
    else // if it is the root set the root to the child and return the child
    {
        return child;                               
    }
}

/*
*Removes the given node bp from the tree
*bp must have exactly 2 pieces of child
*
*@param rt  Pointer to root of the tree or subtree
*@param bp  Pointer to the node to be removed
*@return    returns the pointer to the new root of the tree
*/
void *remove2Kids(void *rt, void *bp)
{
    void *pt = mm_parent(rt, bp); //gets the parent of bp
    void *replacement = mm_replace(LEFT(bp)); //gets the replacement for the removed node bp
    
    void *bpLeft; //stores the new tree after replacement has been removed
    
    bpLeft = mm_remove(LEFT(bp), replacement);//removes the replacement and stores the new tree
    
    //set the children of the replacement to bp's children
    SETLEFT(replacement, bpLeft);
    SETRIGHT(replacement, RIGHT(bp));
    
    if(pt != NULL)// if it is not the root insert the replacement 
    {
        if(LEFT(pt) == bp)
            SETLEFT(pt, replacement);
        else
            SETRIGHT(pt, replacement);
        return rt; //return the new tree
    }
    else //if it is the root set the root to the replacement and return it
    {
        return replacement; 
    }   
              
}
/*
*Finds the replacement for replacing parents two children
*must me the left of the node to be replaced
*
*@param bp  the left node of the node being replaced
*@return    node that is to be a replacement
*/
void *mm_replace(void *bp)
{
     if(RIGHT(bp) == NULL) //check if there right is NULL, if it is return bp
        return bp;
     else //if the right is not NULL keep going right
        return mm_replace(RIGHT(bp));
}